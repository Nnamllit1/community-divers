#include "community/setup.hpp"
#include "community/platform.hpp"
#include "community/version.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace cd;
namespace {
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void rejects(F f) {
    try {
        f();
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error("Expected setup rejection");
}
void write(const fs::path &p, const std::string &s) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << s;
}
void package(const fs::path &root) {
    json files = json::array();
    for (const auto *name : {"CommunityDivers.exe", "CommunityDiversUpdate.exe",
                             "CommunityDiversInspect.exe", "docs/example.md", "build-info.json"}) {
        auto body =
            std::string(name) == "build-info.json"
                ? json{{"source_commit", version::source_commit}, {"platform", "windows-x64"}}
                      .dump()
                : "abc";
        write(root / name, body);
        auto f = fingerprint_file(root / name, name);
        files.push_back({{"path", name}, {"sha256", f.sha256}, {"bytes", f.bytes}});
    }
    write(root / "install-inventory.json", json{{"schema", 1}, {"files", files}}.dump());
}
} // namespace
int main() {
    auto root =
        fs::temp_directory_path() /
        from_utf8("community-divers-setup-" +
                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                  "-\xc3\xa4");
    try {
        fs::create_directories(root);
        auto app = root / "application";
        check_setup_destination(app);
        require_setup_complete(app); // No setup marker: portable/source builds still work.
        fs::create_directory(app);
        begin_setup_install(app, "CommunityDivers-QA");
        rejects([&] { require_setup_complete(app); });
        write(app / "interrupted.txt", "partial");
        check_setup_destination(app);
        fs::remove(app / "interrupted.txt");
        package(app);
        register_setup_install(app, "CommunityDivers-QA");
        verify_setup_files(app);
        require_setup_complete(app);
        require(json::parse(read_text(app / ".community-divers-installed.json")).at("state") ==
                    "installed",
                "Setup was not finalized");
        std::cout << "PASS setup ownership, Unicode paths and interrupted-install repair\n";
        auto installed = json::parse(read_text(app / ".community-divers-installed.json"));
        auto invalid = installed;
        invalid["state"] = "unknown";
        write(app / ".community-divers-installed.json", invalid.dump());
        rejects([&] { require_setup_complete(app); });
        write(app / ".community-divers-installed.json", installed.dump());
        require_setup_complete(app);
        std::cout << "PASS launcher refuses incomplete or unknown setup state\n";
        auto unknown = root / "unknown";
        write(unknown / "user.txt", "keep");
        rejects([&] { check_setup_destination(unknown); });
        rejects([&] { uninstall_managed_launcher(unknown); });
        require(read_text(unknown / "user.txt") == "keep", "Setup changed an unrelated folder");
        write(root / "game/bin/helldivers2.exe", "game");
        rejects([&] { check_setup_destination(root / "game"); });
        rejects([&] { check_setup_destination(user_data_path()); });
        std::cout << "PASS unrelated, game and launcher-data folders refused\n";
        {
            LauncherLock lock(app);
            rejects([&] { check_setup_destination(app); });
            rejects([&] { uninstall_managed_launcher(app); });
        }
        write(app / ".community-divers-update.json", "pending");
        rejects([&] { uninstall_managed_launcher(app); });
        fs::remove(app / ".community-divers-update.json");
        std::cout << "PASS setup/uninstall respect active launcher and update state\n";
        write(app / "docs/example.md", "custom content");
        write(app / "mods/example.txt", "keep mod");
        write(app / "profiles/player.json", "keep profile");
        write(app / "game-copy/data.bin", "keep game");
        auto result = uninstall_managed_launcher(app);
        require(result.deleted == 4 && result.preserved == 1, "Owned/modified file counts differ");
        require(!fs::exists(app / "CommunityDivers.exe") &&
                    !fs::exists(app / "install-inventory.json") &&
                    !fs::exists(app / ".community-divers-installed.json"),
                "Owned metadata or binary retained");
        require(read_text(app / "docs/example.md") == "custom content" &&
                    read_text(app / "mods/example.txt") == "keep mod" &&
                    read_text(app / "profiles/player.json") == "keep profile" &&
                    read_text(app / "game-copy/data.bin") == "keep game",
                "Uninstall removed custom/user/game files");
        std::cout << "PASS inventory uninstall preserves modified files, mods, profiles and game "
                     "copies\n";
        auto payload = root / "native-payload";
        package(payload);
        auto stub = root / "setup-stub.exe";
        write(stub, "native setup fixture");
        auto native = root / "native-install";
        rejects([&] {
            install_setup_payload(payload, native, stub, "CommunityDivers-QA",
                                  [](size_t done, size_t, const std::string &) {
                                      if (done == 1)
                                          throw std::runtime_error("Interrupted fixture");
                                  });
        });
        rejects([&] { require_setup_complete(native); });
        require(fs::exists(native / "CommunityDivers.exe"), "No partial native payload installed");
        write(native / "mods/keep.txt", "keep");
        install_setup_payload(payload, native, stub, "CommunityDivers-QA");
        require_setup_complete(native);
        require(setup_registration_key(native) == "CommunityDivers-QA", "Native identity lost");
        rejects([&] { install_setup_payload(payload, native, stub, "CommunityDivers"); });
        require_setup_complete(native);
        std::cout << "PASS native payload interruption, repair and registration identity\n";
        auto hardlink = root / "unrelated-hardlink.txt";
        fs::create_hard_link(native / "docs/example.md", hardlink);
        rejects([&] { install_setup_payload(payload, native, stub, "CommunityDivers-QA"); });
        require(read_text(hardlink) == "abc", "Native install wrote through a hard link");
        fs::remove(hardlink);
        install_setup_payload(payload, native, stub, "CommunityDivers-QA");
        write(native / "Uninstall.exe", "modified helper");
        auto native_result = uninstall_managed_launcher(native);
        require(native_result.preserved == 1 &&
                    read_text(native / "Uninstall.exe") == "modified helper" &&
                    read_text(native / "mods/keep.txt") == "keep",
                "Native uninstall removed modified/user files");
        std::cout
            << "PASS native installer refuses hardlinks and preserves modified uninstallers\n";
        require(fs::weakly_canonical(root.parent_path()) ==
                        fs::weakly_canonical(fs::temp_directory_path()) &&
                    to_utf8(root.filename()).starts_with("community-divers-setup-"),
                "Unsafe test cleanup root");
        fs::remove_all(root);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\nFixture: " << to_utf8(root) << '\n';
        return 1;
    }
}
