#include <windows.h>
#include "community/installer.hpp"
#include "community/platform.hpp"
#include <miniz.h>
#include <algorithm>
#include <map>
#include <set>
namespace cd {
namespace {
struct Reader {
    HANDLE file = INVALID_HANDLE_VALUE;
    mz_zip_archive zip{};
    bool initialized = false;
    ~Reader() {
        if (initialized)
            mz_zip_reader_end(&zip);
        if (file != INVALID_HANDLE_VALUE)
            CloseHandle(file);
    }
};
size_t read_zip(void *opaque, mz_uint64 offset, void *buffer, size_t bytes) {
    auto file = static_cast<Reader *>(opaque)->file;
    LARGE_INTEGER position;
    position.QuadPart = static_cast<LONGLONG>(offset);
    DWORD got = 0;
    if (bytes > MAXDWORD || !SetFilePointerEx(file, position, nullptr, FILE_BEGIN) ||
        !ReadFile(file, buffer, static_cast<DWORD>(bytes), &got, nullptr))
        return 0;
    return got;
}
struct Output {
    HANDLE file = INVALID_HANDLE_VALUE;
    uint64_t total = 0, limit = 0;
    ~Output() {
        if (file != INVALID_HANDLE_VALUE)
            CloseHandle(file);
    }
};
size_t write_zip(void *opaque, mz_uint64 offset, const void *buffer, size_t bytes) {
    auto &out = *static_cast<Output *>(opaque);
    if (offset != out.total || bytes > out.limit - out.total || bytes > MAXDWORD)
        return 0;
    DWORD n = 0;
    if (!WriteFile(out.file, buffer, static_cast<DWORD>(bytes), &n, nullptr))
        return 0;
    out.total += n;
    return n;
}
std::string folded(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
} // namespace
std::vector<ManagedFile> extract_update_bundle(const fs::path &package,
                                               const UpdateRelease &release,
                                               const fs::path &destination) {
    if (fs::exists(destination))
        throw std::runtime_error("Update extraction directory already exists");
    Reader in;
    in.file = CreateFileW(package.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                          FILE_FLAG_RANDOM_ACCESS, nullptr);
    if (in.file == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot lock the update archive");
    auto fingerprint = fingerprint_file(package, "update-package");
    if (!fingerprint.error.empty() || fingerprint.sha256 != release.sha256 ||
        fingerprint.bytes != release.bytes)
        throw std::runtime_error("Update archive no longer matches its signed manifest");
    in.zip.m_pRead = read_zip;
    in.zip.m_pIO_opaque = &in;
    if (!mz_zip_reader_init(&in.zip, release.bytes, 0))
        throw std::runtime_error("Invalid update ZIP");
    in.initialized = true;
    auto count = mz_zip_reader_get_num_files(&in.zip);
    if (!count || count > 8192)
        throw std::runtime_error("Invalid update archive entry count");
    std::string prefix = release.filename.substr(0, release.filename.size() - 4) + "/";
    std::map<std::string, mz_uint> files;
    std::set<std::string> names;
    uint64_t total = 0;
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&in.zip, i, &stat))
            throw std::runtime_error("Invalid ZIP directory");
        auto length = mz_zip_reader_get_filename(&in.zip, i, nullptr, 0);
        if (!length || length > 512)
            throw std::runtime_error("Update archive path is too long");
        std::string name(length, '\0');
        mz_zip_reader_get_filename(&in.zip, i, name.data(), length);
        name.pop_back();
        if (name.find('\0') != std::string::npos || !name.starts_with(prefix))
            throw std::runtime_error("Update archive has an unexpected package root");
        auto relative = name.substr(prefix.size());
        auto mode = (stat.m_external_attr >> 16) & 0170000;
        if ((mode && mode != 0100000 && mode != 0040000) ||
            (stat.m_external_attr & FILE_ATTRIBUTE_REPARSE_POINT) || stat.m_is_encrypted ||
            !stat.m_is_supported)
            throw std::runtime_error("Update archive contains links or unsupported entries");
        if (stat.m_is_directory) {
            if (relative.empty())
                continue;
            if (relative.back() != '/')
                throw std::runtime_error("Invalid ZIP directory path");
            relative.pop_back();
            if (!managed_update_path(relative + "/placeholder") && relative != "assets" &&
                relative != "assets/brand" && relative != "compatibility" && relative != "updates")
                throw std::runtime_error("Unexpected ZIP directory");
            continue;
        }
        if (!managed_update_path(relative) || !names.insert(folded(relative)).second ||
            stat.m_uncomp_size > update_package_limit ||
            stat.m_uncomp_size > 1024 * 1024 * 1024ULL - total)
            throw std::runtime_error("Unsafe, duplicate or oversized ZIP entry");
        total += stat.m_uncomp_size;
        files.emplace(relative, i);
    }
    if (!files.contains("install-inventory.json"))
        throw std::runtime_error("Update package lacks an install inventory");
    fs::create_directories(destination);
    for (const auto &[relative, index] : files) {
        auto output = destination / from_utf8(relative);
        fs::create_directories(output.parent_path());
        mz_zip_archive_file_stat stat{};
        mz_zip_reader_file_stat(&in.zip, index, &stat);
        Output out;
        out.limit = stat.m_uncomp_size;
        out.file = CreateFileW(output.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (out.file == INVALID_HANDLE_VALUE ||
            !mz_zip_reader_extract_to_callback(&in.zip, index, write_zip, &out, 0) ||
            out.total != out.limit || !FlushFileBuffers(out.file))
            throw std::runtime_error("Update ZIP extraction failed");
    }
    auto inventory = parse_install_inventory(read_text(destination / "install-inventory.json"));
    if (inventory.size() + 1 != files.size())
        throw std::runtime_error("ZIP and inventory do not contain the same files");
    for (const auto &f : inventory) {
        if (!files.contains(f.path))
            throw std::runtime_error("Inventory file is missing from the ZIP");
        auto actual = fingerprint_file(destination / from_utf8(f.path), f.path);
        if (!actual.error.empty() || actual.bytes != f.bytes || actual.sha256 != f.sha256)
            throw std::runtime_error("Extracted file does not match the install inventory");
    }
    auto build = json::parse(read_text(destination / "build-info.json"));
    if (build.at("source_commit") != release.source_commit ||
        build.at("release_id") != release.release_id ||
        build.at("source_revision") != release.revision || build.at("source_dirty") != false ||
        build.at("platform") != "windows-x64" ||
        build.at("service_protocol") != release.service_protocol ||
        build.value("launcher_update_protocol", 0) != 1)
        throw std::runtime_error(
            "Update build identity or startup protocol does not match the signed release");
    return inventory;
}
} // namespace cd
