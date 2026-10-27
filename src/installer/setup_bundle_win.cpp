#include <windows.h>
#include "setup_bundle.hpp"
#include "community/platform.hpp"
#include "community/version.hpp"
#include <array>
#include <fstream>
#include <objbase.h>

namespace cd::setup {
namespace {
constexpr std::array<char, 16> magic{'C', 'D', 'S', 'E', 'T', 'U', 'P', 'B',
                                     'U', 'N', 'D', 'L', 'E', '0', '0', '1'};
void need(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
struct File {
    HANDLE h;
    explicit File(const fs::path &path, bool write = false)
        : h(CreateFileW(path.c_str(), write ? GENERIC_WRITE : GENERIC_READ, FILE_SHARE_READ,
                        nullptr, write ? CREATE_NEW : OPEN_EXISTING,
                        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr)) {
        need(h != INVALID_HANDLE_VALUE, "Cannot open setup package");
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(h, &info) || info.nNumberOfLinks != 1 ||
            (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))) {
            CloseHandle(h);
            throw std::runtime_error("Setup expects an independent regular file");
        }
    }
    ~File() { CloseHandle(h); }
    void seek(uint64_t offset) {
        LARGE_INTEGER position{};
        position.QuadPart = static_cast<LONGLONG>(offset);
        need(SetFilePointerEx(h, position, nullptr, FILE_BEGIN) != FALSE, "Cannot seek setup file");
    }
    void read(void *buffer, DWORD size) {
        DWORD got = 0;
        need(ReadFile(h, buffer, size, &got, nullptr) && got == size, "Truncated setup file");
    }
    void write(const void *buffer, DWORD size) {
        DWORD done = 0;
        need(WriteFile(h, buffer, size, &done, nullptr) && done == size,
             "Cannot write setup package");
    }
    uint64_t size() {
        LARGE_INTEGER value{};
        need(GetFileSizeEx(h, &value) && value.QuadPart >= 0, "Cannot size setup file");
        return static_cast<uint64_t>(value.QuadPart);
    }
};
void copy_range(File &input, File &output, uint64_t offset, uint64_t bytes) {
    std::array<char, 65536> buffer{};
    input.seek(offset);
    while (bytes) {
        auto count = static_cast<DWORD>((std::min)(bytes, uint64_t(buffer.size())));
        input.read(buffer.data(), count);
        output.write(buffer.data(), count);
        bytes -= count;
    }
}
Bundle inspect(File &file) {
    auto total = file.size();
    need(total >= 4096 + 32, "This setup executable has no installation payload");
    file.seek(total - 32);
    std::array<char, 16> tag{};
    uint64_t bytes = 0, descriptor_size = 0;
    file.read(tag.data(), 16);
    file.read(&bytes, 8);
    file.read(&descriptor_size, 8);
    need(tag == magic && bytes > 0 && bytes <= update_package_limit && descriptor_size > 0 &&
             descriptor_size <= 65536 && bytes + descriptor_size + 32 + 4096 <= total,
         "Invalid setup payload footer");
    file.seek(total - 32 - descriptor_size);
    std::string body(static_cast<size_t>(descriptor_size), '\0');
    file.read(body.data(), static_cast<DWORD>(body.size()));
    auto j = json::parse(body);
    need(j.at("schema") == 1 && j.at("product") == "community-divers-setup",
         "Unsupported setup package");
    Bundle out;
    out.qa = j.at("qa").get<bool>();
    out.interrupt = j.at("interrupt").get<bool>();
    out.dirty = j.at("source_dirty").get<bool>();
    need(out.qa || (!out.interrupt && !out.dirty), "Invalid production setup flags");
    auto &r = out.release;
    r.release_id = j.at("release_id");
    r.source_commit = j.at("source_commit");
    r.revision = j.at("source_revision");
    r.service_protocol = j.at("service_protocol");
    r.filename = j.at("filename");
    r.sha256 = j.at("sha256");
    r.bytes = bytes;
    need(r.source_commit == version::source_commit && r.release_id == version::release_id &&
             r.revision == version::source_revision,
         "Setup executable and payload have different source versions");
    need(r.filename == "CommunityDivers-" + r.release_id + "-windows-x64.zip" &&
             r.sha256.size() == 64 &&
             r.sha256.find_first_not_of("0123456789abcdef") == std::string::npos,
         "Invalid setup payload identity");
    out.offset = total - 32 - descriptor_size - bytes;
    return out;
}
} // namespace

fs::path create_setup_workspace() {
    GUID id{};
    need(SUCCEEDED(CoCreateGuid(&id)), "Cannot allocate a setup workspace");
    wchar_t text[40]{};
    StringFromGUID2(id, text, 40);
    auto temporary = fs::temp_directory_path();
    // Refuse redirected directory links before creating this process's private workspace.
    fs::path current;
    for (const auto &part : fs::absolute(temporary).lexically_normal()) {
        current /= part;
        if (!current.has_root_directory())
            continue;
        auto attributes = GetFileAttributesW(current.c_str());
        need(attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT),
             "Setup temporary directory cannot traverse links");
    }
    auto work = temporary / (std::wstring(L"CommunityDivers-setup-") + text);
    need(fs::create_directory(work), "Setup workspace already exists");
    return work;
}
StagedBundle::~StagedBundle() {
    // Only this uniquely created temporary tree is eligible for cleanup.
    std::error_code error;
    if (work.parent_path() == fs::temp_directory_path(error) &&
        work.filename().native().starts_with(L"CommunityDivers-setup-"))
        fs::remove_all(work, error);
}
Bundle inspect_bundle(const fs::path &executable) {
    File file(executable);
    return inspect(file);
}
std::unique_ptr<StagedBundle> stage_bundle(const fs::path &executable) {
    File input(executable);
    auto staged = std::make_unique<StagedBundle>();
    staged->bundle = inspect(input);
    staged->work = create_setup_workspace();
    auto zip = staged->work / "payload.zip";
    {
        File output(zip, true);
        copy_range(input, output, staged->bundle.offset, staged->bundle.release.bytes);
        need(FlushFileBuffers(output.h) != FALSE, "Cannot flush staged setup payload");
    }
    staged->payload = staged->work / "payload";
    extract_update_bundle(zip, staged->bundle.release, staged->payload,
                          staged->bundle.qa && staged->bundle.dirty);
    return staged;
}
void pack_bundle(const fs::path &stub, const fs::path &zip, const fs::path &output, bool qa,
                 bool interrupt, bool allow_dirty) {
    need(qa || (!interrupt && !allow_dirty), "Test setup flags require the QA identity");
    // Use only the local portable archive built from this exact executable's source.
    auto work = std::make_unique<StagedBundle>();
    work->work = create_setup_workspace();
    File source(zip), executable(stub);
    auto fingerprint = fingerprint_handle(reinterpret_cast<uintptr_t>(source.h), "setup-payload");
    need(fingerprint.error.empty() && fingerprint.bytes > 0 &&
             fingerprint.bytes <= update_package_limit,
         "Cannot fingerprint setup payload");
    // CMake supplies build-info beside the stub; extraction verifies every field
    // again against the archive's own inventory, so metadata cannot substitute its files.
    auto info = json::parse(read_text(stub.parent_path() / "build-info.json"));
    need(info.at("source_commit") == std::string(version::source_commit) &&
             info.at("release_id") == std::string(version::release_id) &&
             info.at("platform") == "windows-x64",
         "Setup stub and portable package must be built together");
    bool dirty = info.at("source_dirty").get<bool>();
    need(!dirty || (qa && allow_dirty), "Production setup requires clean sources");
    UpdateRelease release;
    release.source_commit = info.at("source_commit");
    release.release_id = info.at("release_id");
    release.revision = info.at("source_revision");
    release.service_protocol = info.at("service_protocol");
    release.filename = to_utf8(zip.filename());
    release.bytes = fingerprint.bytes;
    release.sha256 = fingerprint.sha256;
    extract_update_bundle(zip, release, work->work / "verified", qa && allow_dirty);
    json descriptor{{"schema", 1},
                    {"product", "community-divers-setup"},
                    {"qa", qa},
                    {"interrupt", interrupt},
                    {"source_dirty", dirty},
                    {"release_id", release.release_id},
                    {"source_commit", release.source_commit},
                    {"source_revision", release.revision},
                    {"service_protocol", release.service_protocol},
                    {"filename", release.filename},
                    {"sha256", release.sha256}};
    auto body = descriptor.dump();
    File destination(output, true);
    copy_range(executable, destination, 0, executable.size());
    copy_range(source, destination, 0, source.size());
    destination.write(body.data(), static_cast<DWORD>(body.size()));
    destination.write(magic.data(), 16);
    uint64_t bytes = source.size(), metadata = body.size();
    destination.write(&bytes, 8);
    destination.write(&metadata, 8);
    need(FlushFileBuffers(destination.h) != FALSE, "Cannot flush completed setup bundle");
}
} // namespace cd::setup
