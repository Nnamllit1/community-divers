#include <windows.h>
#include <bcrypt.h>
#include "community/preparation.hpp"
#include "community/platform.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <memory>
#include <set>
namespace cd {
namespace {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE h) : value(h) {
        if (h == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot lock preparation path (Windows " +
                                     std::to_string(GetLastError()) + ")");
    }
    ~Handle() {
        if (value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
};
std::string lower(std::string s) {
    for (auto &c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
bool decimal(const std::string &s) {
    return !s.empty() && s.size() <= 20 && s.find_first_not_of("0123456789") == s.npos;
}
BY_HANDLE_FILE_INFORMATION info(HANDLE handle) {
    BY_HANDLE_FILE_INFORMATION out{};
    if (!GetFileInformationByHandle(handle, &out) ||
        (out.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Client preparation does not follow reparse points");
    return out;
}
struct Paths {
    std::map<fs::path, std::unique_ptr<Handle>> directories;
    fs::path resolved(const fs::path &path) const {
        auto it = directories.find(fs::absolute(path).lexically_normal());
        if (it == directories.end())
            throw std::runtime_error("Preparation directory is not locked");
        auto handle = it->second->value;
        auto length =
            GetFinalPathNameByHandleW(handle, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (!length)
            throw std::runtime_error("Cannot resolve preparation directory identity");
        std::wstring name(length, L'\0');
        auto written = GetFinalPathNameByHandleW(handle, name.data(), length,
                                                 FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (!written || written >= length)
            throw std::runtime_error("Cannot resolve preparation directory identity");
        name.resize(written);
        return fs::path(name);
    }
    void lock(const fs::path &path) {
        fs::path current;
        for (const auto &part : fs::absolute(path).lexically_normal()) {
            current /= part;
            if (!current.has_root_directory() || directories.contains(current))
                continue;
            auto h = std::make_unique<Handle>(CreateFileW(
                current.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            if (!(info(h->value).dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                throw std::runtime_error("Preparation ancestor is not a directory");
            directories.emplace(current, std::move(h));
        }
    }
};
std::unique_ptr<Handle> reader(const fs::path &path, Paths &paths, bool single_link = false) {
    paths.lock(path.parent_path());
    auto file = std::make_unique<Handle>(
        CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                    FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    auto details = info(file->value);
    if ((details.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (single_link && details.nNumberOfLinks != 1))
        throw std::runtime_error("Prepared files must be independent regular files");
    return file;
}
uint64_t size(HANDLE file) {
    LARGE_INTEGER n{};
    if (!GetFileSizeEx(file, &n) || n.QuadPart < 0)
        throw std::runtime_error("Cannot read client file size");
    return static_cast<uint64_t>(n.QuadPart);
}
struct Sha1 {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::vector<unsigned char> object;
    static void checked(NTSTATUS status) {
        if (status < 0)
            throw std::runtime_error("Client integrity hash failed");
    }
    Sha1() {
        try {
            checked(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA1_ALGORITHM, nullptr, 0));
            DWORD bytes = 0, returned = 0;
            checked(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                                      reinterpret_cast<PUCHAR>(&bytes), sizeof(bytes), &returned,
                                      0));
            object.resize(bytes);
            checked(BCryptCreateHash(algorithm, &hash, object.data(), bytes, nullptr, 0, 0));
        } catch (...) {
            if (algorithm)
                BCryptCloseAlgorithmProvider(algorithm, 0);
            throw;
        }
    }
    ~Sha1() {
        if (hash)
            BCryptDestroyHash(hash);
        if (algorithm)
            BCryptCloseAlgorithmProvider(algorithm, 0);
    }
    void append(unsigned char *data, DWORD bytes) { checked(BCryptHashData(hash, data, bytes, 0)); }
    std::string finish() {
        unsigned char digest[20];
        checked(BCryptFinishHash(hash, digest, sizeof(digest), 0));
        constexpr char hex[] = "0123456789abcdef";
        std::string out;
        for (auto b : digest) {
            out += hex[b >> 4];
            out += hex[b & 15];
        }
        return out;
    }
};
void check_file(HANDLE file, const DepotFile &expected) {
    if (size(file) != expected.bytes)
        throw std::runtime_error("Client file has the wrong size: " + expected.path);
    LARGE_INTEGER zero{};
    if (!SetFilePointerEx(file, zero, nullptr, FILE_BEGIN))
        throw std::runtime_error("Cannot rewind client file");
    Sha1 hash;
    std::vector<unsigned char> buffer(1024 * 1024);
    uint64_t total = 0;
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &got, nullptr))
            throw std::runtime_error("Cannot read client file: " + expected.path);
        if (!got)
            break;
        total += got;
        hash.append(buffer.data(), got);
    }
    if (total != expected.bytes || hash.finish() != expected.sha1)
        throw std::runtime_error("Client content differs from its installed depot manifest: " +
                                 expected.path);
}
fs::path full_path(const fs::path &path) {
    if (path.empty())
        throw std::runtime_error("An explicit directory is required");
    auto result = fs::absolute(path).lexically_normal();
    auto drive = result.root_name().native();
    if (drive.size() != 2 || drive[1] != L':' ||
        !((drive[0] >= L'A' && drive[0] <= L'Z') || (drive[0] >= L'a' && drive[0] <= L'z')))
        throw std::runtime_error("Client preparation requires a local drive path");
    for (const auto &part : result.relative_path()) {
        auto name = part.native();
        if (name.empty() || name.back() == L'.' || name.back() == L' ' ||
            name.find_first_of(L":<>\"|?*") != name.npos)
            throw std::runtime_error("Ambiguous Windows directory component");
        auto stem = name.substr(0, name.find(L'.'));
        std::transform(stem.begin(), stem.end(), stem.begin(), [](wchar_t c) -> wchar_t {
            return c >= L'a' && c <= L'z' ? static_cast<wchar_t>(c - (L'a' - L'A')) : c;
        });
        if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL" ||
            (stem.size() == 4 && (stem.starts_with(L"COM") || stem.starts_with(L"LPT")) &&
             stem.back() >= L'0' && stem.back() <= L'9'))
            throw std::runtime_error("Reserved Windows directory component");
    }
    return result;
}
bool contains(const fs::path &parent, const fs::path &child) {
    auto p = parent.begin(), c = child.begin();
    for (; p != parent.end(); ++p, ++c)
        if (c == child.end() || _wcsicmp(p->c_str(), c->c_str()) != 0)
            return false;
    return true;
}
json module_fingerprints(const fs::path &root) {
    json out = json::object();
    for (auto relative : client_binary_paths) {
        auto f = fingerprint_file(root / from_utf8(std::string(relative)), std::string(relative));
        if (!f.error.empty())
            throw std::runtime_error(f.error);
        out[relative] = {{"bytes", f.bytes}, {"sha256", f.sha256}};
    }
    return out;
}
} // namespace
PreparationPlan plan_client_copy(const fs::path &source, const fs::path &depot_cache) {
    PreparationPlan out;
    out.source = full_path(source);
    Paths paths;
    paths.lock(out.source);
    auto installation = inspect_installation(out.source);
    if (!installation.valid || !decimal(installation.build))
        throw std::runtime_error("Select the installed Steam game with readable build metadata");
    auto app_path = out.source.parent_path().parent_path() / "appmanifest_553850.acf";
    auto app_lock = reader(app_path, paths);
    auto app_doc = parse_vdf(read_text(app_path));
    auto app = app_doc.get("AppState");
    auto depots = app ? app->get("InstalledDepots") : nullptr;
    if (!app || app->text("appid") != "553850" || app->text("StateFlags") != "4" ||
        app->text("buildid") != installation.build || !depots || depots->children.empty() ||
        depots->children.size() > 64)
        throw std::runtime_error("Steam must report a fully installed game with installed depots");
    out.build = installation.build;
    std::map<std::string, DepotFile> entries;
    std::vector<std::unique_ptr<Handle>> manifest_locks;
    for (const auto &[id, entry] : depots->children) {
        auto gid = entry.text("manifest");
        if (!decimal(id) || !decimal(gid))
            throw std::runtime_error("Invalid installed depot identity");
        auto path = depot_cache / from_utf8(id + "_" + gid + ".manifest");
        manifest_locks.push_back(reader(full_path(path), paths));
        auto manifest = parse_depot_manifest(read_text(path, 64 * 1024 * 1024));
        if (std::to_string(manifest.depot) != id || std::to_string(manifest.manifest) != gid)
            throw std::runtime_error("Depot cache identity does not match the installed game");
        auto digest = fingerprint_file(path, "depot-manifest");
        if (!digest.error.empty())
            throw std::runtime_error(digest.error);
        out.depots.push_back({{"depot", id}, {"manifest", gid}, {"sha256", digest.sha256}});
        for (auto &file : manifest.files) {
            auto key = lower(file.path);
            auto [it, inserted] = entries.emplace(key, file);
            if (entries.size() > 100000)
                throw std::runtime_error("Combined depot inventory is too large");
            if (!inserted &&
                (it->second.path != file.path || it->second.sha1 != file.sha1 ||
                 it->second.bytes != file.bytes || it->second.directory != file.directory))
                throw std::runtime_error("Installed depots contain conflicting file paths");
        }
    }
    for (const auto &[key, file] : entries) {
        for (auto pos = key.find('/'); pos != key.npos; pos = key.find('/', pos + 1)) {
            auto it = entries.find(key.substr(0, pos));
            if (it != entries.end() && !it->second.directory)
                throw std::runtime_error("Installed depots contain conflicting directories");
        }
        if (file.directory)
            continue;
        if (file.bytes > 1024ULL * 1024 * 1024 * 1024 - out.bytes)
            throw std::runtime_error("Installed file inventory is too large");
        auto handle = reader(out.source / from_utf8(file.path), paths);
        if (size(handle->value) != file.bytes)
            throw std::runtime_error("Installed file size differs from the depot manifest: " +
                                     file.path);
        out.bytes += file.bytes;
        out.files.push_back(file);
    }
    if (out.files.empty() || out.files.size() > 100000)
        throw std::runtime_error("Invalid installed file inventory");
    for (auto required : client_binary_paths)
        if (!entries.contains(lower(std::string(required))) ||
            entries.at(lower(std::string(required))).directory)
            throw std::runtime_error("Installed depots do not describe the required game modules");
    return out;
}
void prepare_client_copy(const fs::path &source, const fs::path &depot_cache,
                         const fs::path &destination, PreparationProgress progress) {
    auto plan = plan_client_copy(source, depot_cache);
    auto target = full_path(destination);
    if (contains(plan.source, target) || contains(target, plan.source))
        throw std::runtime_error("Source and isolated destination must not overlap");
    for (const auto &part : target)
        if (_wcsicmp(part.c_str(), L"steamapps") == 0)
            throw std::runtime_error("The isolated destination must be outside Steam libraries");
    auto stage = target;
    stage += L".preparing";
    if (fs::exists(target) || fs::exists(stage))
        throw std::runtime_error("Destination or staging directory already exists; no existing "
                                 "copy will be overwritten");
    Paths paths;
    paths.lock(plan.source);
    auto app = reader(plan.source.parent_path().parent_path() / "appmanifest_553850.acf", paths);
    std::vector<std::unique_ptr<Handle>> manifest_locks;
    for (const auto &depot : plan.depots)
        manifest_locks.push_back(
            reader(full_path(depot_cache /
                             from_utf8(depot.at("depot").get<std::string>() + "_" +
                                       depot.at("manifest").get<std::string>() + ".manifest")),
                   paths));
    if (preparation_report(plan_client_copy(source, depot_cache)) != preparation_report(plan))
        throw std::runtime_error("Steam installation changed before preparation");
    // Existing ancestors are pinned before creating descendants. No junctions/symlinks.
    auto existing = target.parent_path();
    while (!fs::exists(existing))
        existing = existing.parent_path();
    paths.lock(existing);
    auto resolved_source = paths.resolved(plan.source);
    auto resolved_target = paths.resolved(existing) / target.lexically_relative(existing);
    for (const auto &part : resolved_target)
        if (_wcsicmp(part.c_str(), L"steamapps") == 0)
            throw std::runtime_error("The resolved destination is inside a Steam library");
    if (contains(resolved_source, resolved_target) || contains(resolved_target, resolved_source))
        throw std::runtime_error("Resolved source and destination must not overlap");
    fs::create_directories(target.parent_path());
    paths.lock(target.parent_path());
    if (fs::space(target.parent_path()).available < plan.bytes + 64 * 1024 * 1024)
        throw std::runtime_error("Insufficient free space for an independent game copy");
    if (!fs::create_directory(stage))
        throw std::runtime_error("Cannot reserve client staging directory");
    paths.lock(stage);
    size_t index = 0;
    for (const auto &file : plan.files) {
        if (progress)
            progress(index, plan.files.size(), file.path);
        auto input = reader(plan.source / from_utf8(file.path), paths);
        if (size(input->value) != file.bytes)
            throw std::runtime_error("Source changed during preparation: " + file.path);
        auto output_path = stage / from_utf8(file.path);
        fs::create_directories(output_path.parent_path());
        paths.lock(output_path.parent_path());
        Handle output(CreateFileW(output_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                                  nullptr));
        std::vector<unsigned char> buffer(1024 * 1024);
        uint64_t total = 0;
        for (;;) {
            DWORD got = 0, written = 0;
            if (!ReadFile(input->value, buffer.data(), static_cast<DWORD>(buffer.size()), &got,
                          nullptr))
                throw std::runtime_error("Cannot read source: " + file.path);
            if (!got)
                break;
            if (got > file.bytes - total ||
                !WriteFile(output.value, buffer.data(), got, &written, nullptr) || written != got)
                throw std::runtime_error("Cannot copy complete file: " + file.path);
            total += got;
        }
        if (!FlushFileBuffers(output.value))
            throw std::runtime_error("Cannot flush prepared file");
        check_file(output.value, file);
        ++index;
    }
    if (preparation_report(plan_client_copy(source, depot_cache)) != preparation_report(plan))
        throw std::runtime_error("Steam installation changed during preparation");
    auto report = preparation_report(plan);
    report["state"] = "copied";
    report["binary_fingerprints"] = module_fingerprints(stage);
    fs::create_directory(stage / L".community-divers");
    auto receipt_path = stage / L".community-divers/preparation.json";
    auto body = report.dump(2) + "\n";
    Handle receipt(CreateFileW(receipt_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                               CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    DWORD written = 0;
    if (body.size() > MAXDWORD ||
        !WriteFile(receipt.value, body.data(), static_cast<DWORD>(body.size()), &written,
                   nullptr) ||
        written != body.size() || !FlushFileBuffers(receipt.value))
        throw std::runtime_error("Cannot persist preparation receipt");
    // Release handles below stage so the completed directory can be renamed.
    for (auto it = paths.directories.begin(); it != paths.directories.end();) {
        if (contains(stage, it->first))
            it = paths.directories.erase(it);
        else
            ++it;
    }
    // The receipt still has an open handle; permit directory rename by closing it first.
    CloseHandle(receipt.value);
    receipt.value = INVALID_HANDLE_VALUE;
    if (!MoveFileExW(stage.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Could not publish completed copy; staging files were retained");
    if (progress)
        progress(index, plan.files.size(), "Complete");
}
json verify_client_copy(const fs::path &destination, PreparationProgress progress) {
    auto root = full_path(destination);
    Paths paths;
    paths.lock(root);
    auto receipt = reader(root / L".community-divers/preparation.json", paths, true);
    auto report =
        json::parse(read_text(root / L".community-divers/preparation.json", 32 * 1024 * 1024));
    if (report.at("schema") != 1 || report.at("state") != "copied" ||
        !report.at("files").is_array() || report["files"].empty() ||
        report["files"].size() > 100000 || report.at("integration_available") != false ||
        report.at("profile_isolation_verified") != false ||
        report.at("official_network_isolation_verified") != false)
        throw std::runtime_error("Invalid client preparation receipt");
    std::set<std::string> expected;
    std::vector<std::unique_ptr<Handle>> verified_handles;
    uint64_t total = 0;
    size_t index = 0;
    for (const auto &entry : report["files"]) {
        DepotFile f;
        f.path = entry.at("path").get<std::string>();
        f.sha1 = entry.at("sha1").get<std::string>();
        if (!entry.at("bytes").is_number_unsigned())
            throw std::runtime_error("Invalid receipt size");
        f.bytes = entry.at("bytes").get<uint64_t>();
        if (!client_relative_path(f.path) || f.sha1.size() != 40 ||
            f.sha1.find_first_not_of("0123456789abcdef") != f.sha1.npos ||
            !expected.insert(lower(f.path)).second ||
            f.bytes > 1024ULL * 1024 * 1024 * 1024 - total)
            throw std::runtime_error("Invalid preparation file inventory");
        total += f.bytes;
        if (progress)
            progress(index++, report["files"].size(), f.path);
        auto file = reader(root / from_utf8(f.path), paths, true);
        check_file(file->value, f);
        verified_handles.push_back(std::move(file));
    }
    if (total != report.at("bytes").get<uint64_t>() ||
        module_fingerprints(root) != report.at("binary_fingerprints"))
        throw std::runtime_error("Prepared client fingerprint mismatch");
    for (const auto &entry : fs::recursive_directory_iterator(root)) {
        auto relative = entry.path().lexically_relative(root);
        auto name = relative.generic_string();
        if (entry.is_directory()) {
            paths.lock(entry.path());
            continue;
        }
        if (name == ".community-divers/preparation.json")
            continue;
        if (!expected.contains(lower(name)))
            throw std::runtime_error("Unexpected file in prepared client: " + name);
        auto file = reader(entry.path(), paths, true);
    }
    report["state"] = "verified_copy";
    if (progress)
        progress(index, report["files"].size(), "Complete");
    return report;
}
} // namespace cd
