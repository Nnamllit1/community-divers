#include <windows.h>
#include <objbase.h>
#include "community/setup.hpp"
#include "community/platform.hpp"
#include "community/version.hpp"
#include <algorithm>
#include <memory>
#include <set>
namespace cd {
namespace {
constexpr wchar_t marker_name[] = L".community-divers-installed.json";
void need(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE value) : h(value) {
        need(h != INVALID_HANDLE_VALUE, "A setup file is in use or inaccessible");
    }
    ~Handle() {
        if (h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
    }
    void close() {
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            h = INVALID_HANDLE_VALUE;
        }
    }
};
struct TemporaryFile {
    fs::path path;
    Handle &file;
    ~TemporaryFile() {
        file.close();
        DeleteFileW(path.c_str());
    }
};
bool within(const fs::path &parent, const fs::path &child) {
    auto p = parent.begin(), c = child.begin();
    for (; p != parent.end(); ++p, ++c)
        if (c == child.end() || _wcsicmp(p->c_str(), c->c_str()))
            return false;
    return true;
}
struct Pins {
    std::map<fs::path, std::unique_ptr<Handle>> folders;
    void hold(const fs::path &path) {
        fs::path current;
        for (const auto &part : fs::absolute(path).lexically_normal()) {
            current /= part;
            if (!current.has_root_directory() || folders.contains(current))
                continue;
            auto file = std::make_unique<Handle>(CreateFileW(
                current.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            BY_HANDLE_FILE_INFORMATION info{};
            need(GetFileInformationByHandle(file->h, &info) &&
                     !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
                     (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY),
                 "Setup cannot traverse directory links");
            folders.emplace(current, std::move(file));
        }
    }
};
fs::path root_path(const fs::path &input) {
    need(input.is_absolute(), "Setup requires an absolute installation path");
    auto root = fs::absolute(input).lexically_normal();
    need(root != root.root_path() && root.root_name().native().size() == 2,
         "Choose a local application folder");
    auto existing = root;
    while (!fs::exists(existing))
        existing = existing.parent_path();
    Pins pins;
    pins.hold(existing);
    auto resolved = fs::weakly_canonical(root);
    for (const auto &part : resolved)
        need(_wcsicmp(part.c_str(), L"steamapps") != 0,
             "Setup cannot install into Steam libraries");
    auto data = fs::weakly_canonical(user_data_path());
    need(!within(resolved, data) && !within(data, resolved),
         "Launcher installation and user data must be separate");
    need(!fs::exists(root / "bin/helldivers2.exe") && !fs::exists(root / "data/game/game.dll"),
         "Setup cannot modify a game directory");
    return resolved;
}
std::unique_ptr<Handle> file_handle(const fs::path &path, Pins &pins, bool remove = false) {
    pins.hold(path.parent_path());
    auto file = std::make_unique<Handle>(CreateFileW(
        path.c_str(), GENERIC_READ | (remove ? DELETE : 0), FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    BY_HANDLE_FILE_INFORMATION info{};
    need(GetFileInformationByHandle(file->h, &info) &&
             !(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
             info.nNumberOfLinks == 1,
         "Setup expects independent regular files");
    return file;
}
std::string contents(HANDLE file, size_t limit) {
    LARGE_INTEGER size{}, zero{};
    need(GetFileSizeEx(file, &size) && size.QuadPart >= 0 && uint64_t(size.QuadPart) <= limit,
         "Setup metadata exceeds its size bound");
    need(SetFilePointerEx(file, zero, nullptr, FILE_BEGIN) != FALSE,
         "Cannot rewind setup metadata");
    std::string data(static_cast<size_t>(size.QuadPart), '\0');
    DWORD got = 0;
    need(ReadFile(file, data.data(), static_cast<DWORD>(data.size()), &got, nullptr) &&
             got == data.size(),
         "Cannot read setup metadata");
    return data;
}
json marker(const fs::path &root) {
    Pins pins;
    auto file = file_handle(root / marker_name, pins);
    auto j = json::parse(contents(file->h, 8192));
    need(j.at("schema") == 1 && j.at("product") == "community-divers-launcher",
         "Folder is not a registered Community Divers installation");
    auto recorded = from_utf8(j.at("root").get<std::string>());
    need(fs::equivalent(root, recorded),
         "Installed application location does not match its ownership marker");
    auto key = j.at("registry_key").get<std::string>();
    need(key == "CommunityDivers" || key == "CommunityDivers-QA",
         "Unrecognized application registration");
    auto state = j.at("state").get<std::string>();
    need(state == "preparing" || state == "installed", "Unrecognized setup state");
    return j;
}
void atomic_marker(const fs::path &root, const json &j) {
    auto temporary = root / (L".setup-marker-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                             std::to_wstring(GetTickCount64()));
    Handle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL, nullptr));
    auto body = j.dump(2) + "\n";
    DWORD n = 0;
    need(WriteFile(file.h, body.data(), static_cast<DWORD>(body.size()), &n, nullptr) &&
             n == body.size() && FlushFileBuffers(file.h),
         "Cannot persist setup ownership marker");
    file.close();
    need(MoveFileExW(temporary.c_str(), (root / marker_name).c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE,
         "Cannot commit setup ownership marker");
}
void disposition(HANDLE h, bool remove) {
    FILE_DISPOSITION_INFO info{static_cast<BOOLEAN>(remove ? TRUE : FALSE)};
    need(SetFileInformationByHandle(h, FileDispositionInfo, &info, sizeof(info)) != FALSE,
         "Cannot remove an owned launcher file");
}
} // namespace
void require_setup_complete(const fs::path &input) {
    // Portable/source builds have no setup marker. A setup installation must be
    // finalized before a partially replaced launcher can start or self-update.
    if (!fs::exists(input / marker_name))
        return;
    auto root = root_path(input);
    auto info = marker(root);
    need(info.at("state") == "installed",
         "Launcher setup is incomplete. Close this window and run the installer again to repair "
         "the installation. Your profiles and mods are preserved.");
}
void check_setup_destination(const fs::path &input) {
    auto root = root_path(input);
    if (!fs::exists(root))
        return;
    need(fs::is_directory(root), "Setup destination must be a folder");
    if (fs::is_empty(root))
        return;
    marker(root);
    need(!fs::exists(root / ".community-divers-update.json"),
         "Recover the pending launcher update before running setup");
    LauncherLock lock(root);
}
void begin_setup_install(const fs::path &input, const std::string &key) {
    auto root = root_path(input);
    need(key == "CommunityDivers" || key == "CommunityDivers-QA",
         "Unrecognized setup registration");
    if (fs::exists(root / marker_name))
        marker(root);
    else
        for (const auto &entry : fs::directory_iterator(root))
            need(entry.path().filename() == L".community-divers-launcher.lock",
                 "An unregistered setup folder must be empty");
    Pins pins;
    pins.hold(root);
    atomic_marker(root, {{"schema", 1},
                         {"product", "community-divers-launcher"},
                         {"root", to_utf8(root)},
                         {"registry_key", key},
                         {"state", "preparing"}});
}
void verify_setup_files(const fs::path &input) {
    auto root = root_path(input);
    Pins pins;
    pins.hold(root);
    auto inventory = file_handle(root / "install-inventory.json", pins);
    auto files = parse_install_inventory(contents(inventory->h, 2 * 1024 * 1024));
    for (const auto &expected : files) {
        auto file = file_handle(root / from_utf8(expected.path), pins);
        auto actual = fingerprint_handle(reinterpret_cast<uintptr_t>(file->h), expected.path);
        need(actual.error.empty() && actual.bytes == expected.bytes &&
                 actual.sha256 == expected.sha256,
             "Installed launcher files do not match their inventory");
    }
    auto info = json::parse(read_text(root / "build-info.json"));
    need(info.at("source_commit") == std::string(version::source_commit) &&
             info.at("platform") == "windows-x64",
         "Installer payload has a different source identity");
}
void register_setup_install(const fs::path &input, const std::string &key) {
    auto root = root_path(input);
    need(key == "CommunityDivers" || key == "CommunityDivers-QA",
         "Unrecognized setup registration");
    json ownership;
    if (fs::exists(root / marker_name)) {
        ownership = marker(root);
        need(ownership.at("registry_key") == key,
             "Setup identity differs from the installed application");
    }
    verify_setup_files(root);
    Pins pins;
    pins.hold(root);
    json installed = {{"schema", 1},
                      {"product", "community-divers-launcher"},
                      {"root", to_utf8(root)},
                      {"registry_key", key},
                      {"state", "installed"}};
    if (ownership.contains("uninstaller"))
        installed["uninstaller"] = ownership["uninstaller"];
    atomic_marker(root, installed);
}
std::string setup_registration_key(const fs::path &input) {
    return marker(root_path(input)).at("registry_key").get<std::string>();
}
void install_setup_payload(const fs::path &payload, const fs::path &input,
                           const fs::path &uninstaller, const std::string &key,
                           const SetupProgress &progress, bool interrupt_fixture) {
    need(!interrupt_fixture || key == "CommunityDivers-QA",
         "Interruption fixtures require QA identity");
    verify_setup_files(payload);
    check_setup_destination(input);
    auto root = root_path(input);
    fs::create_directories(root);
    Pins pins;
    pins.hold(root);
    LauncherLock lock(root);
    if (fs::exists(root / marker_name))
        need(marker(root).at("registry_key") == key,
             "Setup identity differs from the installed application");
    auto files = parse_install_inventory(read_text(payload / "install-inventory.json"));
    auto extra = [&](const fs::path &path, const std::string &name) {
        auto info = fingerprint_file(path, name);
        need(info.error.empty(), "Cannot read setup input");
        files.push_back({name, info.sha256, info.bytes});
    };
    extra(payload / "install-inventory.json", "install-inventory.json");
    extra(uninstaller, "Uninstall.exe");
    GUID transaction{};
    need(SUCCEEDED(CoCreateGuid(&transaction)), "Cannot create setup transaction identity");
    wchar_t transaction_name[40]{};
    need(StringFromGUID2(transaction, transaction_name, 40) != 0,
         "Cannot encode setup transaction identity");
    begin_setup_install(root, key);
    size_t completed = 0;
    // Put the launcher first so the interruption test exercises its incomplete-state refusal.
    std::stable_sort(files.begin(), files.end(), [](const auto &a, const auto &b) {
        return a.path == "CommunityDivers.exe" && b.path != "CommunityDivers.exe";
    });
    for (const auto &expected : files) {
        const auto source =
            expected.path == "Uninstall.exe" ? uninstaller : payload / from_utf8(expected.path);
        auto input_file = file_handle(source, pins);
        auto actual = fingerprint_handle(reinterpret_cast<uintptr_t>(input_file->h), expected.path);
        need(actual.error.empty() && actual.bytes == expected.bytes &&
                 actual.sha256 == expected.sha256,
             "Setup input changed after verification");
        auto destination = root / from_utf8(expected.path);
        fs::path parent = root;
        for (const auto &part : from_utf8(expected.path).parent_path()) {
            parent /= part;
            if (!fs::exists(parent))
                fs::create_directory(parent);
            pins.hold(parent);
        }
        if (fs::exists(destination)) {
            auto previous = file_handle(destination, pins);
        }
        auto temporary = parent / (std::wstring(L".setup-file-") + transaction_name + L"-" +
                                   std::to_wstring(completed));
        Handle output(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr));
        // Adopt for cleanup only after CREATE_NEW succeeds. Unique names avoid
        // collisions with partial files from a stopped installer.
        TemporaryFile cleanup{temporary, output};
        LARGE_INTEGER zero{};
        need(SetFilePointerEx(input_file->h, zero, nullptr, FILE_BEGIN) != FALSE,
             "Cannot rewind setup input");
        char buffer[65536];
        uint64_t remaining = expected.bytes;
        while (remaining) {
            DWORD got = 0, written = 0;
            auto count = static_cast<DWORD>((std::min)(remaining, uint64_t(sizeof(buffer))));
            need(ReadFile(input_file->h, buffer, count, &got, nullptr) && got == count &&
                     WriteFile(output.h, buffer, got, &written, nullptr) && written == got,
                 "Cannot copy a launcher file; run setup again to repair");
            remaining -= got;
        }
        need(FlushFileBuffers(output.h) != FALSE, "Cannot persist launcher file");
        output.close();
        input_file->close();
        need(MoveFileExW(temporary.c_str(), destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE,
             "Cannot replace a launcher file; close the launcher and retry setup");
        ++completed;
        if (progress)
            progress(completed, files.size(), expected.path);
        if (interrupt_fixture && expected.path == "CommunityDivers.exe") {
            auto checkpoint = root / ".community-divers-test-ready";
            Handle ready(CreateFileW(checkpoint.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                     FILE_ATTRIBUTE_NORMAL, nullptr));
            ready.close();
            Sleep(60000);
        }
    }
    auto state = marker(root);
    const auto &own = files.back();
    need(own.path == "Uninstall.exe", "Uninstaller inventory order is invalid");
    state["uninstaller"] = {{"sha256", own.sha256}, {"bytes", own.bytes}};
    atomic_marker(root, state);
    register_setup_install(root, key);
}
UninstallReport uninstall_managed_launcher(const fs::path &input) {
    auto root = root_path(input);
    auto installed = marker(root);
    need(!fs::equivalent(executable_directory(), root),
         "Uninstall helper must run outside the application folder");
    need(!fs::exists(root / ".community-divers-update.json"),
         "Recover the pending update before uninstalling");
    UninstallReport report;
    std::set<fs::path> directories;
    {
        LauncherLock lock(root);
        Pins pins;
        pins.hold(root);
        auto inventory = file_handle(root / "install-inventory.json", pins, true);
        auto ownership = file_handle(root / marker_name, pins, true);
        auto files = parse_install_inventory(contents(inventory->h, 2 * 1024 * 1024));
        if (installed.contains("uninstaller")) {
            auto &own = installed.at("uninstaller");
            files.push_back({"Uninstall.exe", own.at("sha256").get<std::string>(),
                             own.at("bytes").get<uint64_t>()});
        }
        std::vector<std::unique_ptr<Handle>> removing;
        for (const auto &expected : files) {
            auto path = root / from_utf8(expected.path);
            if (!fs::exists(path))
                continue;
            auto file = file_handle(path, pins, true);
            auto actual = fingerprint_handle(reinterpret_cast<uintptr_t>(file->h), expected.path);
            if (!actual.error.empty() || actual.bytes != expected.bytes ||
                actual.sha256 != expected.sha256) {
                ++report.preserved;
                continue;
            }
            auto attrs = GetFileAttributesW(path.c_str());
            if (attrs & FILE_ATTRIBUTE_READONLY) {
                ++report.preserved;
                continue;
            }
            for (auto parent = path.parent_path(); parent != root; parent = parent.parent_path())
                directories.insert(parent);
            removing.push_back(std::move(file));
        }
        std::vector<HANDLE> marked;
        try {
            for (auto &file : removing) {
                disposition(file->h, true);
                marked.push_back(file->h);
            }
            disposition(inventory->h, true);
            marked.push_back(inventory->h);
            disposition(ownership->h, true);
            marked.push_back(ownership->h);
        } catch (...) {
            for (auto h : marked) {
                FILE_DISPOSITION_INFO keep{FALSE};
                SetFileInformationByHandle(h, FileDispositionInfo, &keep, sizeof(keep));
            }
            throw;
        }
        report.deleted = removing.size();
        removing.clear();
        inventory->close();
        ownership->close();
    }
    // Nonrecursive removal: custom files and nonempty directories are retained.
    std::vector<fs::path> ordered(directories.begin(), directories.end());
    std::sort(ordered.begin(), ordered.end(),
              [](const auto &a, const auto &b) { return a.native().size() > b.native().size(); });
    for (const auto &directory : ordered) {
        auto attrs = GetFileAttributesW(directory.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_REPARSE_POINT))
            RemoveDirectoryW(directory.c_str());
    }
    DeleteFileW((root / L".community-divers-launcher.lock").c_str());
    return report;
}
void refresh_setup_registration(const fs::path &input, const std::string &release) noexcept {
    try {
        if (!fs::exists(input / marker_name))
            return;
        auto root = root_path(input);
        auto info = marker(root);
        auto key = wide("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" +
                        info.at("registry_key").get<std::string>());
        HKEY registry = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, key.c_str(), 0,
                          KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_64KEY,
                          &registry) != ERROR_SUCCESS)
            return;
        wchar_t location[32768]{};
        DWORD size = sizeof(location), type = 0;
        auto status = RegQueryValueExW(registry, L"InstallLocation", nullptr, &type,
                                       reinterpret_cast<LPBYTE>(location), &size);
        std::error_code ec;
        if (status == ERROR_SUCCESS && type == REG_SZ && size >= sizeof(wchar_t) &&
            location[size / sizeof(wchar_t) - 1] == 0 &&
            fs::equivalent(root, fs::path(location), ec)) {
            auto text = wide(release);
            RegSetValueExW(registry, L"DisplayVersion", 0, REG_SZ,
                           reinterpret_cast<const BYTE *>(text.c_str()),
                           static_cast<DWORD>((text.size() + 1) * sizeof(wchar_t)));
        }
        RegCloseKey(registry);
    } catch (...) {
    }
}
} // namespace cd
