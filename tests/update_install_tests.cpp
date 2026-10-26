#include <windows.h>
#include "update_fixture.hpp"
#include "community/installer.hpp"
#include "community/platform.hpp"
#include "community/version.hpp"
#include <miniz.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
using namespace cd;
using namespace cd::test;
namespace {
void put(const fs::path &p, const std::string &text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << text;
}
json inventory(const std::map<std::string, std::string> &files, const fs::path &scratch) {
    json entries = json::array();
    for (const auto &[name, body] : files) {
        auto p = scratch / from_utf8(name);
        put(p, body);
        auto f = fingerprint_file(p, name);
        update_require(f.error.empty(), "Fixture file hash failed");
        entries.push_back({{"path", name}, {"sha256", f.sha256}, {"bytes", f.bytes}});
    }
    return {{"schema", 1}, {"files", entries}};
}
void wait_process(HANDLE h, uint32_t timeout = 15000) {
    update_require(WaitForSingleObject(h, timeout) == WAIT_OBJECT_0,
                   "Fixture process did not finish");
}
void launch(const fs::path &exe, const std::vector<fs::path> &args) {
    std::wstring cmd = L"\"" + exe.wstring() + L"\"";
    for (const auto &a : args)
        cmd += L" \"" + a.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    update_require(CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE,
                                  CREATE_NO_WINDOW, nullptr, exe.parent_path().c_str(), &startup,
                                  &info),
                   "Fixture spawn failed");
    CloseHandle(info.hThread);
    wait_process(info.hProcess);
    CloseHandle(info.hProcess);
}
void wait_helper(const fs::path &job) {
    auto instance = json::parse(read_text(job / "helper-instance.json"));
    HANDLE handle = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                instance.at("pid").get<DWORD>());
    if (!handle) {
        update_require(GetLastError() == ERROR_INVALID_PARAMETER, "Cannot observe helper process");
        return;
    }
    FILETIME a{}, b{}, c{}, d{};
    update_require(GetProcessTimes(handle, &a, &b, &c, &d), "Cannot observe helper instance");
    auto timestamp = (static_cast<uint64_t>(a.dwHighDateTime) << 32) | a.dwLowDateTime;
    update_require(timestamp == instance.at("created").get<uint64_t>(), "Helper PID was reused");
    wait_process(handle);
    CloseHandle(handle);
}
void wait_unlocked(const fs::path &root) {
    auto end = GetTickCount64() + 5000;
    while (GetTickCount64() < end) {
        try {
            LauncherLock lock(root);
            return;
        } catch (...) {
            Sleep(20);
        }
    }
    throw std::runtime_error("Updated fixture did not release its inherited lock");
}
} // namespace
int main() {
    auto sandbox =
        fs::temp_directory_path() /
        from_utf8("community-divers-install-tests-" +
                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        auto fixture_binary =
            read_text(executable_directory() / "update_install_fixture.exe", 32 * 1024 * 1024);
        for (const std::string mode :
             {"ok", "fail", "hang", "bad-zip", "helper-wrong-key", "crash"}) {
            auto base = sandbox / mode;
            auto root = base / from_utf8("installed \xc3\xa4");
            auto data = base / "data";
            fs::create_directories(root);
            fs::create_directories(data);
            auto key = generate_update_key();
            put(root / "fixture-public.txt", key.public_hex);
            put(data / "fixture-control.json", json{{"mode", mode}}.dump());
            put(data / "launcher.json", "user settings preserved");
            put(data / "mods/user-mod.txt", "user mod preserved");
            put(root / "unmanaged.txt", "keep this file");
            json build{{"schema", 1},
                       {"launcher_update_protocol", 1},
                       {"source_commit", version::source_commit},
                       {"release_id", version::release_id},
                       {"source_revision", version::source_revision},
                       {"source_dirty", false},
                       {"platform", "windows-x64"},
                       {"service_protocol", 1}};
            std::map<std::string, std::string> old_files{
                {"CommunityDivers.exe", fixture_binary},
                {"CommunityDiversUpdate.exe", fixture_binary},
                {"build-info.json", build.dump()},
                {"README.md", "old readme"},
                {"assets/brand/obsolete.txt", "old asset"}};
            auto old_inventory = inventory(old_files, root);
            put(root / "install-inventory.json", old_inventory.dump());
            auto old_build = read_text(root / "build-info.json");
            build["source_commit"] = std::string(40, 'a');
            build["release_id"] = "0.1.0-preview.1+aaaaaaaa";
            build["source_revision"] = version::source_revision + 1;
            std::map<std::string, std::string> next_files{
                {"CommunityDivers.exe", fixture_binary},
                {"CommunityDiversUpdate.exe", fixture_binary},
                {"build-info.json", build.dump()},
                {"README.md", "new readme"},
                {"assets/brand/new.txt", "new asset"}};
            auto next_inventory = inventory(next_files, base / "build");
            next_files["install-inventory.json"] = next_inventory.dump();
            auto payload = update_payload();
            payload["source_revision"] = version::source_revision + 1;
            std::string prefix = payload["filename"].get<std::string>();
            prefix.resize(prefix.size() - 4);
            prefix += "/";
            mz_zip_archive zip{};
            update_require(mz_zip_writer_init_heap(&zip, 0, 0), "Cannot create test ZIP");
            for (const auto &[name, body] : next_files)
                update_require(mz_zip_writer_add_mem(&zip, (prefix + name).c_str(), body.data(),
                                                     body.size(), MZ_BEST_SPEED),
                               "Cannot add ZIP entry");
            if (mode == "bad-zip") {
                std::string attack = "escape";
                update_require(mz_zip_writer_add_mem(&zip,
                                                     (prefix + "docs/../../escape.txt").c_str(),
                                                     attack.data(), attack.size(), 0),
                               "Cannot add attack fixture");
            }
            void *buffer = nullptr;
            size_t size = 0;
            update_require(mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size),
                           "Cannot finalize test ZIP");
            put(base / "package.zip", std::string(static_cast<char *>(buffer), size));
            mz_free(buffer);
            mz_zip_writer_end(&zip);
            auto f = fingerprint_file(base / "package.zip", "fixture");
            payload["bytes"] = f.bytes;
            payload["sha256"] = f.sha256;
            put(base / "signed.json", sign_update_payload(payload.dump(), key.private_hex));
            launch(root / "CommunityDivers.exe",
                   {L"--data-dir", data, L"--begin", base / "package.zip", base / "signed.json"});
            auto handoff = json::parse(read_text(data / "handoff.json"));
            if (mode == "bad-zip" || mode == "helper-wrong-key") {
                update_require(!handoff.at("error").get<std::string>().empty(),
                               "Invalid prepared update was accepted");
                update_require(read_text(root / "build-info.json") == old_build &&
                                   read_text(root / "README.md") == "old readme",
                               "Rejected update modified the installation");
                std::cout << "PASS installer rejects " << mode << " before replacing files\n";
                continue;
            }
            update_require(handoff.at("error").get<std::string>().empty(),
                           handoff.at("error").get<std::string>().c_str());
            auto job = from_utf8(handoff.at("job").get<std::string>());
            wait_helper(job);
            if (mode == "crash") {
                update_require(fs::exists(root / ".community-divers-update.json") &&
                                   read_text(root / "README.md") == "new readme",
                               "Interrupted transaction fixture did not leave a partial update");
                launch(root / "CommunityDivers.exe", {L"--data-dir", data});
                wait_helper(job);
            }
            wait_unlocked(root);
            if (mode == "ok") {
                update_require(read_text(root / "README.md") == "new readme" &&
                                   fs::exists(root / "assets/brand/new.txt") &&
                                   !fs::exists(root / "assets/brand/obsolete.txt"),
                               "Successful update did not replace/remove owned files");
                update_require(json::parse(read_text(job / "result.json")).at("ok") == true,
                               "Healthy update not committed");
                std::cout << "PASS native installer restarts a healthy replacement with its "
                             "inherited lock\n";
            } else {
                update_require(read_text(root / "build-info.json") == old_build &&
                                   read_text(root / "README.md") == "old readme" &&
                                   fs::exists(root / "assets/brand/obsolete.txt") &&
                                   !fs::exists(root / "assets/brand/new.txt"),
                               "Rollback did not restore prior owned files");
                std::cout << "PASS native rollback/recovery after " << mode << '\n';
            }
            update_require(!fs::exists(root / ".community-divers-update.json"),
                           "Completed transaction left a recovery pointer");
            update_require(read_text(data / "launcher.json") == "user settings preserved" &&
                               read_text(data / "mods/user-mod.txt") == "user mod preserved" &&
                               read_text(root / "unmanaged.txt") == "keep this file",
                           "Update touched user-owned data");
        }
        update_require(
            fs::weakly_canonical(sandbox.parent_path()) ==
                    fs::weakly_canonical(fs::temp_directory_path()) &&
                sandbox.filename().string().starts_with("community-divers-install-tests-"),
            "Unsafe fixture cleanup target");
        fs::remove_all(sandbox);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\nFixture: " << path_text(sandbox) << '\n';
        return 1;
    }
}
