#include <windows.h>
#include "community/installer.hpp"
#include "community/platform.hpp"
#include "community/version.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <thread>
namespace cd {
namespace {
struct Handle {
    HANDLE h = nullptr;
    Handle() = default;
    explicit Handle(HANDLE value) : h(value) {}
    ~Handle() {
        if (h && h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
    }
    Handle(const Handle &) = delete;
};
struct Process {
    Handle process, thread;
};
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
bool within(const fs::path &path, const fs::path &root) {
    auto p = lower(path_text(path.lexically_normal()));
    auto r = lower(path_text(root.lexically_normal()));
    return p == r || p.starts_with(r + "\\") || p.starts_with(r + "/");
}
void no_links(const fs::path &path) {
    fs::path current;
    for (const auto &part : path) {
        current /= part;
        auto attributes = GetFileAttributesW(current.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::runtime_error("Update paths must not traverse links or reparse points");
    }
}
void roots(const fs::path &root, const fs::path &data) {
    require(root.is_absolute() && data.is_absolute() && root != root.root_path(),
            "Update paths must be absolute application directories");
    no_links(root);
    no_links(data);
    require(!within(root, data) && !within(data, root),
            "Launcher installation and user data must be separate");
    require(!fs::exists(root / "bin/helldivers2.exe") && !fs::exists(root / "data/game/game.dll"),
            "The updater cannot install into a game directory");
}
uint64_t created(HANDLE process) {
    FILETIME a{}, b{}, c{}, d{};
    require(GetProcessTimes(process, &a, &b, &c, &d), "Cannot identify the process instance");
    return (static_cast<uint64_t>(a.dwHighDateTime) << 32) | a.dwLowDateTime;
}
fs::path process_image(HANDLE process) {
    std::wstring text(32768, L'\0');
    DWORD length = static_cast<DWORD>(text.size());
    require(QueryFullProcessImageNameW(process, 0, text.data(), &length),
            "Cannot identify the process image");
    text.resize(length);
    return fs::path(text);
}
void durable_copy(const fs::path &source, const fs::path &target) {
    fs::create_directories(target.parent_path());
    no_links(target.parent_path());
    require(CopyFileW(source.c_str(), target.c_str(), TRUE),
            "Cannot create an update or backup file");
    Handle file(CreateFileW(target.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            0, nullptr));
    require(file.h != INVALID_HANDLE_VALUE && FlushFileBuffers(file.h),
            "Cannot flush the update file");
}
void atomic_json(const fs::path &path, const json &value) {
    fs::create_directories(path.parent_path());
    no_links(path.parent_path());
    auto temp = path;
    temp += ".writing";
    no_links(path);
    no_links(temp);
    auto body = value.dump(2) + "\n";
    Handle file(CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr));
    require(file.h != INVALID_HANDLE_VALUE, "Cannot write update state");
    DWORD n = 0;
    require(WriteFile(file.h, body.data(), static_cast<DWORD>(body.size()), &n, nullptr) &&
                n == body.size() && FlushFileBuffers(file.h),
            "Cannot flush update state");
    CloseHandle(file.h);
    file.h = nullptr;
    require(
        MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),
        "Cannot commit update state");
}
void replace_file(const fs::path &source, const fs::path &target, const std::string &nonce) {
    no_links(target);
    auto temp = target;
    temp += ".cd-new-" + nonce;
    if (fs::exists(temp)) {
        no_links(temp);
        require(DeleteFileW(temp.c_str()), "Cannot clear an interrupted transaction file");
    }
    durable_copy(source, temp);
    if (!MoveFileExW(temp.c_str(), target.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code e;
        fs::remove(temp, e);
        throw std::runtime_error("Cannot replace a launcher file");
    }
}
std::wstring quote(const std::wstring &arg) {
    std::wstring out = L"\"";
    size_t slashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'\"')
            out.append(slashes * 2 + 1, L'\\');
        else
            out.append(slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    out += L'\"';
    return out;
}
void spawn(Process &out, const fs::path &exe, const std::vector<std::wstring> &args,
           const std::vector<HANDLE> &inherited, bool hidden) {
    std::wstring command = quote(exe.wstring());
    for (const auto &arg : args)
        command += L" " + quote(arg);
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<unsigned char> storage(bytes);
    auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    require(InitializeProcThreadAttributeList(attributes, 1, 0, &bytes),
            "Cannot initialize the update process");
    struct Cleanup {
        LPPROC_THREAD_ATTRIBUTE_LIST p;
        ~Cleanup() { DeleteProcThreadAttributeList(p); }
    } cleanup{attributes};
    for (auto handle : inherited)
        require(SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT),
                "Cannot pass an update process handle");
    require(UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                      const_cast<HANDLE *>(inherited.data()),
                                      inherited.size() * sizeof(HANDLE), nullptr, nullptr),
            "Cannot restrict update process handles");
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
    startup.StartupInfo.wShowWindow = hidden ? SW_HIDE : SW_SHOWNORMAL;
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION info{};
    auto ok = CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                             EXTENDED_STARTUPINFO_PRESENT | (hidden ? CREATE_NO_WINDOW : 0),
                             nullptr, exe.parent_path().c_str(), &startup.StartupInfo, &info);
    for (auto handle : inherited)
        SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0);
    require(ok, "Cannot start the updater or launcher");
    out.process.h = info.hProcess;
    out.thread.h = info.hThread;
}
HANDLE event() {
    auto h = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    require(h != nullptr, "Cannot create update startup event");
    return h;
}
std::wstring handle_arg(uintptr_t handle) {
    return std::to_wstring(static_cast<uint64_t>(handle));
}
std::string job_id() {
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return std::to_string(GetCurrentProcessId()) + "-" + std::to_string(counter.QuadPart);
}
void validate_job(const fs::path &job, const fs::path &data) {
    auto name = path_text(job.filename());
    require(!name.empty() && name.size() < 100 &&
                std::all_of(name.begin(), name.end(),
                            [](char c) { return (c >= '0' && c <= '9') || c == '-'; }),
            "Invalid update job identifier");
    require(job.parent_path() == data / "updates/jobs", "Update job is outside launcher data");
    no_links(job);
}
json read_json(const fs::path &p) {
    no_links(p);
    return json::parse(read_text(p));
}
InstallHandoff launch_handoff(json plan, const fs::path &job, bool recover) {
    try {
        Handle ready(event());
        plan["parent_pid"] = static_cast<uint64_t>(GetCurrentProcessId());
        plan["parent_created"] = created(GetCurrentProcess());
        plan["ready_handle"] = reinterpret_cast<uintptr_t>(ready.h);
        auto path = job / (recover ? "recover.json" : "plan.json");
        atomic_json(path, plan);
        Process helper;
        spawn(helper, job / "CommunityDiversUpdate.exe",
              {recover ? L"--recover" : L"--apply", path.wstring()}, {ready.h}, true);
        atomic_json(job / "helper-instance.json", {{"pid", GetProcessId(helper.process.h)},
                                                   {"created", created(helper.process.h)}});
        HANDLE waits[] = {ready.h, helper.process.h};
        auto result = WaitForMultipleObjects(2, waits, FALSE, 60000);
        if (result != WAIT_OBJECT_0) {
            if (WaitForSingleObject(helper.process.h, 0) == WAIT_TIMEOUT) {
                require(TerminateProcess(helper.process.h, 1),
                        "Cannot cancel the timed-out updater");
                require(WaitForSingleObject(helper.process.h, 5000) == WAIT_OBJECT_0,
                        "Timed-out updater did not stop");
            }
            auto outcome = job / "result.json";
            if (fs::exists(outcome))
                throw std::runtime_error(
                    read_json(outcome).value("error", "Update preparation failed"));
            throw std::runtime_error(
                "Updater did not finish preparing; the launcher remains installed");
        }
        return {job, {}};
    } catch (const std::exception &e) {
        return {job, e.what()};
    }
}
void validate_backup(const fs::path &job, const json &journal) {
    std::set<std::string> names;
    for (const auto &entry : journal.at("files")) {
        auto name = entry.at("path").get<std::string>();
        require(managed_update_path(name) && names.insert(lower(name)).second,
                "Invalid rollback file path");
        if (entry.at("existed").get<bool>()) {
            no_links(job / "backup" / from_utf8(name));
            auto f = fingerprint_file(job / "backup" / from_utf8(name), name);
            require(f.error.empty() && f.sha256 == entry.at("sha256").get<std::string>() &&
                        f.bytes == entry.at("bytes").get<uint64_t>(),
                    "An update backup is missing or corrupted");
        }
    }
}
void rollback(const fs::path &root, const fs::path &job, json &journal) {
    validate_backup(job, journal);
    for (const auto &entry : journal["files"]) {
        auto name = entry["path"].get<std::string>();
        auto target = root / from_utf8(name);
        no_links(target);
        if (entry["existed"].get<bool>())
            replace_file(job / "backup" / from_utf8(name), target, path_text(job.filename()));
        else if (fs::exists(target))
            require(DeleteFileW(target.c_str()), "Cannot remove a new file during rollback");
    }
    journal["phase"] = "rolled_back";
    atomic_json(job / "transaction.json", journal);
    require(DeleteFileW((root / ".community-divers-update.json").c_str()),
            "Cannot finish update rollback");
}
void restart(Process &child, const fs::path &root, const fs::path &data, uintptr_t lock,
             uintptr_t health = 0) {
    std::vector<std::wstring> args = {L"--data-dir", data.wstring(), L"--launcher-lock-handle",
                                      handle_arg(lock)};
    std::vector<HANDLE> handles = {reinterpret_cast<HANDLE>(lock)};
    if (health) {
        args.insert(args.end(), {L"--update-health-handle", handle_arg(health)});
        handles.push_back(reinterpret_cast<HANDLE>(health));
    }
    spawn(child, root / "CommunityDivers.exe", args, handles, false);
}
} // namespace
void remember_update_channel(const UpdateRelease &r, const std::string &envelope,
                             const fs::path &data, const std::string &key) {
    auto checked = verify_update_envelope(envelope, "preview", key);
    require(checked.source_commit == r.source_commit && checked.revision == r.revision &&
                checked.sha256 == r.sha256 && checked.bytes == r.bytes,
            "Update result does not match its authenticated envelope");
    auto path = data / "updates/channel-state.json";
    if (fs::exists(path)) {
        auto previous = verify_update_envelope(read_json(path).at("envelope").get<std::string>(),
                                               "preview", key);
        require(r.revision >= previous.revision,
                "The update feed replayed an older observed release");
        if (r.revision == previous.revision)
            require(r.source_commit == previous.source_commit && r.sha256 == previous.sha256 &&
                        r.bytes == previous.bytes && r.release_id == previous.release_id,
                    "The update feed changed an immutable release");
    }
    atomic_json(path, {{"schema", 1}, {"envelope", envelope}});
}
LauncherLock::LauncherLock(const fs::path &root, uintptr_t inherited) {
    auto path = root / ".community-divers-launcher.lock";
    if (inherited) {
        Handle expected(CreateFileW(path.c_str(), 0,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                    OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        BY_HANDLE_FILE_INFORMATION actual{}, reference{};
        auto handle = reinterpret_cast<HANDLE>(inherited);
        require(expected.h != INVALID_HANDLE_VALUE && GetFileInformationByHandle(handle, &actual) &&
                    GetFileInformationByHandle(expected.h, &reference) &&
                    actual.dwVolumeSerialNumber == reference.dwVolumeSerialNumber &&
                    actual.nFileIndexHigh == reference.nFileIndexHigh &&
                    actual.nFileIndexLow == reference.nFileIndexLow,
                "Invalid inherited launcher lock");
        value_ = inherited;
    } else {
        no_links(root);
        no_links(path);
        auto h = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
        require(h != INVALID_HANDLE_VALUE,
                "This launcher installation is already open or updating");
        value_ = reinterpret_cast<uintptr_t>(h);
    }
}
LauncherLock::~LauncherLock() {
    if (value_)
        CloseHandle(reinterpret_cast<HANDLE>(value_));
}
void signal_update_health(uintptr_t handle) {
    require(handle && SetEvent(reinterpret_cast<HANDLE>(handle)),
            "Cannot acknowledge launcher startup");
    CloseHandle(reinterpret_cast<HANDLE>(handle));
}
InstallHandoff start_update_install(const fs::path &package, const std::string &envelope,
                                    const fs::path &installation, const fs::path &user_data,
                                    const std::string &key) {
    fs::path job;
    try {
        auto root = fs::canonical(installation);
        fs::create_directories(user_data);
        auto data = fs::canonical(user_data);
        roots(root, data);
        require(fs::equivalent(process_image(GetCurrentProcess()), root / "CommunityDivers.exe"),
                "Updates must be started by the installed launcher");
        auto current = read_json(root / "build-info.json");
        require(current.at("source_commit").get<std::string>() == version::source_commit &&
                    current.value("launcher_update_protocol", 0) == 1,
                "This launcher installation does not match the running build");
        auto release = verify_update_envelope(envelope, "preview", key);
        require(compare_update(release, current.at("source_commit").get<std::string>(),
                               current.at("source_revision").get<uint64_t>()) ==
                    UpdateRelation::available,
                "Automatic installation requires a newer published main revision");
        remember_update_channel(release, envelope, data, key);
        auto owned = parse_install_inventory(read_text(root / "install-inventory.json"));
        for (const char *name : {"CommunityDivers.exe", "CommunityDiversUpdate.exe"}) {
            auto it = std::find_if(owned.begin(), owned.end(),
                                   [&](const auto &f) { return f.path == name; });
            auto actual = fingerprint_file(root / name, name);
            require(
                it != owned.end() && actual.error.empty() && actual.sha256 == it->sha256 &&
                    actual.bytes == it->bytes,
                "Installed launcher files were modified; install a fresh portable package first");
        }
        job = data / "updates/jobs" / from_utf8(job_id());
        require(fs::create_directories(job), "Cannot create a unique updater job");
        durable_copy(root / "CommunityDiversUpdate.exe", job / "CommunityDiversUpdate.exe");
        auto helper = fingerprint_file(job / "CommunityDiversUpdate.exe", "helper");
        json plan{{"schema", 1},
                  {"root", path_text(root)},
                  {"data", path_text(data)},
                  {"package", path_text(fs::canonical(package))},
                  {"envelope", envelope},
                  {"expected_commit", current.at("source_commit")},
                  {"helper_sha256", helper.sha256}};
        return launch_handoff(std::move(plan), job, false);
    } catch (const std::exception &e) {
        return {job, e.what()};
    }
}
std::future<InstallHandoff> start_update_install_async(fs::path package, std::string envelope,
                                                       fs::path root, fs::path data) {
    auto promise = std::make_shared<std::promise<InstallHandoff>>();
    auto future = promise->get_future();
    std::thread([promise, package = std::move(package), envelope = std::move(envelope),
                 root = std::move(root), data = std::move(data)] {
        try {
            promise->set_value(start_update_install(package, envelope, root, data));
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }).detach();
    return future;
}
InstallHandoff start_update_recovery(const fs::path &installation, const fs::path &user_data) {
    try {
        auto root = fs::canonical(installation);
        auto data = fs::canonical(user_data);
        roots(root, data);
        auto pointer = read_json(root / ".community-divers-update.json");
        auto job = data / "updates/jobs" / from_utf8(pointer.at("job").get<std::string>());
        validate_job(job, data);
        auto plan = read_json(job / "plan.json");
        require(from_utf8(plan.at("root").get<std::string>()) == root &&
                    from_utf8(plan.at("data").get<std::string>()) == data,
                "Recovery belongs to a different installation");
        auto helper = fingerprint_file(job / "CommunityDiversUpdate.exe", "helper");
        require(helper.error.empty() &&
                    helper.sha256 == plan.at("helper_sha256").get<std::string>(),
                "Recovery helper is missing or damaged");
        return launch_handoff(std::move(plan), job, true);
    } catch (const std::exception &e) {
        return {{}, e.what()};
    }
}
int run_update_installer(const fs::path &plan_path, const std::string &key, bool recover,
                         const InstallTiming &timing) {
    auto job = plan_path.parent_path();
    fs::path root, data;
    bool mutation = false;
    json journal;
    std::unique_ptr<LauncherLock> lock;
    Process child;
    try {
        auto plan = read_json(plan_path);
        require(plan.at("schema") == 1, "Unsupported update job");
        root = from_utf8(plan.at("root").get<std::string>());
        data = from_utf8(plan.at("data").get<std::string>());
        roots(root, data);
        validate_job(job, data);
        Handle parent(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                  plan.at("parent_pid").get<DWORD>()));
        require(parent.h && created(parent.h) == plan.at("parent_created").get<uint64_t>() &&
                    fs::equivalent(process_image(parent.h), root / "CommunityDivers.exe"),
                "The launching process instance no longer matches");
        std::vector<ManagedFile> inventory;
        std::optional<UpdateRelease> release;
        if (recover) {
            journal = read_json(job / "transaction.json");
            validate_backup(job, journal);
        } else {
            auto current = read_json(root / "build-info.json");
            require(current.at("source_commit") == plan.at("expected_commit") &&
                        current.at("source_commit").get<std::string>() == version::source_commit,
                    "Installed build changed while preparing the update");
            release =
                verify_update_envelope(plan.at("envelope").get<std::string>(), "preview", key);
            require(compare_update(*release, current.at("source_commit").get<std::string>(),
                                   current.at("source_revision").get<uint64_t>()) ==
                        UpdateRelation::available,
                    "Update does not advance the installed revision");
            remember_update_channel(*release, plan.at("envelope").get<std::string>(), data, key);
            inventory = extract_update_bundle(from_utf8(plan.at("package").get<std::string>()),
                                              *release, job / "prepared");
            std::map<std::string, bool> paths;
            for (const auto &f :
                 parse_install_inventory(read_text(root / "install-inventory.json")))
                paths[f.path] = false;
            for (const auto &f : inventory)
                paths[f.path] = true;
            paths["install-inventory.json"] = true;
            journal = {{"schema", 1},
                       {"phase", "prepared"},
                       {"files", json::array()},
                       {"release_id", release->release_id}};
            for (const auto &[name, present] : paths) {
                auto target = root / from_utf8(name);
                no_links(target);
                bool existed = fs::exists(target);
                json record{{"path", name}, {"existed", existed}, {"new", present}};
                if (existed) {
                    require(fs::is_regular_file(target), "A managed launcher file is a directory");
                    durable_copy(target, job / "backup" / from_utf8(name));
                    auto saved = fingerprint_file(job / "backup" / from_utf8(name), name);
                    require(saved.error.empty(), "Cannot verify the update backup");
                    record["sha256"] = saved.sha256;
                    record["bytes"] = saved.bytes;
                }
                journal["files"].push_back(std::move(record));
            }
            atomic_json(job / "transaction.json", journal);
        }
        require(SetEvent(reinterpret_cast<HANDLE>(plan.at("ready_handle").get<uintptr_t>())),
                "Launcher handoff event is unavailable");
        require(WaitForSingleObject(parent.h, timing.parent_exit_ms) == WAIT_OBJECT_0,
                "Launcher did not exit; no files were replaced");
        lock = std::make_unique<LauncherLock>(root);
        if (recover) {
            if (journal.value("phase", "") != "committed")
                rollback(root, job, journal);
            else
                require(DeleteFileW((root / ".community-divers-update.json").c_str()),
                        "Cannot clear confirmed update state");
            Process restored;
            restart(restored, root, data, lock->handle());
            atomic_json(job / "result.json", {{"ok", true}, {"recovered", true}});
            return 0;
        }
        require(read_json(root / "build-info.json").at("source_commit") ==
                    plan.at("expected_commit"),
                "Installed version changed before replacement");
        atomic_json(root / ".community-divers-update.json",
                    {{"schema", 1}, {"job", path_text(job.filename())}});
        mutation = true;
        journal["phase"] = "applying";
        atomic_json(job / "transaction.json", journal);
        size_t replaced = 0;
        for (const auto &entry : journal["files"]) {
            auto name = entry.at("path").get<std::string>();
            auto target = root / from_utf8(name);
            if (entry.at("new").get<bool>())
                replace_file(job / "prepared" / from_utf8(name), target, path_text(job.filename()));
            else if (fs::exists(target)) {
                no_links(target);
                require(DeleteFileW(target.c_str()), "Cannot remove an obsolete managed file");
            }
            if (timing.after_replace)
                timing.after_replace(++replaced);
        }
        journal["phase"] = "awaiting_health";
        atomic_json(job / "transaction.json", journal);
        Handle health(event());
        restart(child, root, data, lock->handle(), reinterpret_cast<uintptr_t>(health.h));
        HANDLE waits[] = {health.h, child.process.h};
        require(WaitForMultipleObjects(2, waits, FALSE, timing.health_ms) == WAIT_OBJECT_0 &&
                    WaitForSingleObject(child.process.h, timing.settle_ms) == WAIT_TIMEOUT,
                "Updated launcher failed its startup check");
        journal["phase"] = "committed";
        atomic_json(job / "transaction.json", journal);
        require(DeleteFileW((root / ".community-divers-update.json").c_str()),
                "Cannot finish confirmed update state");
        mutation = false;
        auto result = json{{"ok", true},
                           {"release_id", release->release_id},
                           {"recovered", false},
                           {"child_pid", GetProcessId(child.process.h)},
                           {"child_created", created(child.process.h)}};
        try {
            atomic_json(job / "result.json", result);
            atomic_json(data / "updates/last-install.json", result);
        } catch (...) {
        }
        return 0;
    } catch (const std::exception &e) {
        std::string error = e.what();
        if (mutation && lock) {
            try {
                if (child.process.h && WaitForSingleObject(child.process.h, 0) == WAIT_TIMEOUT) {
                    require(TerminateProcess(child.process.h, 1),
                            "Cannot stop the failed replacement process");
                    require(WaitForSingleObject(child.process.h, 10000) == WAIT_OBJECT_0,
                            "Replacement process did not stop");
                }
                rollback(root, job, journal);
                Process restored;
                restart(restored, root, data, lock->handle());
            } catch (const std::exception &failure) {
                error += std::string("; recovery required: ") + failure.what();
            }
        }
        try {
            auto result = json{{"ok", false}, {"error", error}};
            atomic_json(job / "result.json", result);
            if (!data.empty())
                atomic_json(data / "updates/last-install.json", result);
        } catch (...) {
        }
        return 1;
    }
}
} // namespace cd
