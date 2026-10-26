#include "community/core.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace cd;
void require(bool v, const char *message) {
    if (!v)
        throw std::runtime_error(message);
}
template <class F> void rejects(F f) {
    bool bad = false;
    try {
        f();
    } catch (const std::exception &) {
        bad = true;
    }
    require(bad, "Expected invalid input to be rejected");
}
void write(const fs::path &p, const std::string &text) {
    fs::create_directories(p.parent_path());
    std::ofstream(p) << text;
}
int main() {
    auto root = fs::temp_directory_path() /
                ("community-divers-core-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int passed = 0;
    auto test = [&](const char *name, auto body) {
        body();
        ++passed;
        std::cout << "PASS " << name << '\n';
    };
    try {
        test("nested Valve KeyValues and escaped paths", [] {
            auto kv =
                parse_vdf("// comment\n\"libraryfolders\" { \"0\" { \"path\" \"D:\\\\Steam\" } }");
            require(kv.get("libraryfolders")->get("0")->text("path") == "D:\\Steam",
                    "Path decode failed");
        });
        test("reject malformed or duplicate KeyValues", [] {
            rejects([] { parse_vdf("a { b c"); });
            rejects([] { parse_vdf("a b a c"); });
            rejects([] { parse_vdf("a \"unterminated"); });
        });
        test("endpoint normalization", [] {
            auto e = parse_endpoint("HTTPS://Example.COM:443/community/");
            require(e.canonical == "https://example.com/community", "Canonical endpoint failed");
            require(parse_endpoint("http://[::1]:8080").port == 8080, "IPv6 loopback failed");
        });
        test("reject unsafe or ambiguous endpoints", [] {
            for (const auto &s : {"http://example.com", "https://name:secret@example.com",
                                  "https://example.com?x=1", "https://host:0",
                                  "https://host:", "https://host:65536", "https://host/a/../b",
                                  "https://host\\bad", "http://localhost.evil", "https://[:::1]",
                                  "https://[1::2::3]", "https://bad..host", "https://-bad.host"})
                rejects([&] { parse_endpoint(s); });
        });
        test("discover separate Steam library and its game build", [&] {
            auto steam = root / "steam";
            auto library = root / "library";
            fs::create_directories(steam / "steamapps");
            auto escaped = library.generic_string();
            write(steam / "steamapps/libraryfolders.vdf",
                  "\"libraryfolders\" { \"1\" { \"path\" \"" + escaped + "\" } }");
            write(library / "steamapps/appmanifest_553850.acf",
                  "\"AppState\" { \"appid\" \"553850\" \"buildid\" \"12345\" \"installdir\" "
                  "\"Helldivers 2\" }");
            auto game = library / "steamapps/common/Helldivers 2";
            write(game / "bin/helldivers2.exe", "");
            fs::create_directories(game / "data");
            auto found = discover_steam({steam, library});
            require(found.size() == 1 && found[0].valid && found[0].build == "12345",
                    "Library or build discovery failed");
        });
        test("manifest directory cannot escape Steam common", [&] {
            auto l = root / "badlib";
            write(l / "steamapps/appmanifest_553850.acf",
                  "AppState { appid 553850 installdir ../elsewhere }");
            require(discover_steam({l}).empty(), "Traversal was accepted");
        });
        test("settings roundtrip and persistent profile", [&] {
            Settings s;
            s.communities.push_back({"local", "Local", "http://localhost:8080"});
            s.selected_community = "local";
            s.profiles[0].enabled_mods = {"first-person"};
            auto p = root / "config/launcher.json";
            save_settings(p, s);
            auto loaded = load_settings(p);
            require(loaded.error.empty() &&
                        loaded.value.profiles[0].enabled_mods == s.profiles[0].enabled_mods,
                    "Settings roundtrip failed");
            save_settings(p, s);
            require(fs::exists(fs::path(p.string() + ".previous")),
                    "Prior settings were not preserved");
        });
        test("corrupt settings are preserved before recovery", [&] {
            auto p = root / "broken/launcher.json";
            write(p, "broken json");
            require(!load_settings(p).error.empty(), "Invalid settings silently accepted");
            require(read_text(p) == "broken json", "Loading changed the original");
            save_settings(p, Settings{});
            require(read_text(fs::path(p.string() + ".invalid")) == "broken json",
                    "Recovery lost original contents");
        });
        test("invalid selected profile and duplicate communities", [] {
            auto j = encode_settings(Settings{});
            j["selected_profile"] = "missing";
            rejects([&] { decode_settings(j); });
            j = encode_settings(Settings{});
            j["communities"] = {{{"id", "same"}, {"name", "A"}, {"url", "https://a.example"}},
                                {{"id", "same"}, {"name", "B"}, {"url", "https://b.example"}}};
            rejects([&] { decode_settings(j); });
        });
        test("mod metadata and duplicate IDs", [&] {
            auto mods = root / "mods";
            for (auto name : {"a", "b"})
                write(mods / name / "mod.json",
                      R"({"schema":1,"id":"duplicate","name":"A","version":"1"})");
            auto found = discover_mods(mods);
            require(found.size() == 2 && !found[0].error.empty() && !found[1].error.empty(),
                    "Duplicate mod IDs not flagged");
        });
        test("remote readiness cannot enable unimplemented client integration", [] {
            auto service = parse_status(
                R"({"protocol":1,"id":"test","name":"Test","game_builds":["12345"],"launch_available":true})");
            Installation game{{}, "12345", true, {}};
            require(launch_blocker(game, &service).find("adapter") != std::string::npos,
                    "Remote status bypassed local adapter requirement");
            rejects([] { parse_status(R"({"protocol":2})"); });
        });
        fs::remove_all(root);
        std::cout << passed << " core checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\nFixture: " << root << '\n';
        return 1;
    }
}
