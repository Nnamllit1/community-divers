// Test-only process fixture. This executable is never installed or shipped.
#include <windows.h>
#include "community/installer.hpp"
#include "community/platform.hpp"
#include <fstream>
#include <iostream>
using namespace cd;
void put(const fs::path &p, const json &j) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << j.dump(2);
}
int wmain(int argc, wchar_t **argv) {
    try {
        if (argc == 3 &&
            (std::wstring(argv[1]) == L"--apply" || std::wstring(argv[1]) == L"--recover")) {
            auto plan = json::parse(read_text(argv[2]));
            auto root = from_utf8(plan.at("root").get<std::string>());
            auto data = from_utf8(plan.at("data").get<std::string>());
            auto control = json::parse(read_text(data / "fixture-control.json"));
            auto key = read_text(root / "fixture-public.txt");
            auto mode = control.at("mode").get<std::string>();
            if (mode == "helper-wrong-key")
                key = generate_update_key().public_hex;
            InstallTiming timing;
            timing.parent_exit_ms = 5000;
            timing.health_ms = 1000;
            timing.settle_ms = 100;
            if (mode == "crash")
                timing.after_replace = [](size_t n) {
                    if (n == 3)
                        ExitProcess(88);
                };
            return run_update_installer(argv[2], key, std::wstring(argv[1]) == L"--recover",
                                        timing);
        }
        fs::path data, package, envelope;
        uintptr_t lock_handle = 0, health = 0;
        bool begin = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--data-dir" && i + 1 < argc)
                data = argv[++i];
            else if (arg == L"--launcher-lock-handle" && i + 1 < argc)
                lock_handle = static_cast<uintptr_t>(std::stoull(argv[++i]));
            else if (arg == L"--update-health-handle" && i + 1 < argc)
                health = static_cast<uintptr_t>(std::stoull(argv[++i]));
            else if (arg == L"--begin" && i + 2 < argc) {
                begin = true;
                package = argv[++i];
                envelope = argv[++i];
            }
        }
        auto root = executable_directory();
        LauncherLock lock(root, lock_handle);
        auto control = json::parse(read_text(data / "fixture-control.json"));
        if (health) {
            if (control.at("mode") == "fail")
                return 9;
            if (control.at("mode") == "hang") {
                Sleep(5000);
                return 0;
            }
            signal_update_health(health);
            put(data / "health.json", {{"pid", GetCurrentProcessId()}});
            Sleep(600);
            return 0;
        }
        InstallHandoff result;
        if (fs::exists(root / ".community-divers-update.json"))
            result = start_update_recovery(root, data);
        else if (begin)
            result = start_update_install(package, read_text(envelope), root, data,
                                          read_text(root / "fixture-public.txt"));
        else {
            put(data / "restored.json", {{"ok", true}});
            return 0;
        }
        put(data / "handoff.json", {{"job", path_text(result.job)}, {"error", result.error}});
        return result.error.empty() ? 0 : 2;
    } catch (const std::exception &e) {
        std::cerr << "FIXTURE " << e.what() << '\n';
        return 1;
    }
}
