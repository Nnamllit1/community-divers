#include "community/core.hpp"
#include "community/version.hpp"
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
        test("community release requirements are bounded and preserve the adapter gate", [] {
            json j{{"protocol", 1},
                   {"id", "test"},
                   {"name", "Test"},
                   {"game_builds", {"12345"}},
                   {"required_release_set", "campaign-preview-1"},
                   {"required_launcher_release", "future+abcdef12"}};
            auto status = parse_status(j.dump());
            require(status.required_release_set == "campaign-preview-1",
                    "Community release set lost");
            Installation game{{}, "12345", true, {}};
            require(launch_blocker(game, &status).find("Update the launcher") != std::string::npos,
                    "Mismatched launcher release was ignored");
            j["required_launcher_release"] = std::string(version::release_id);
            status = parse_status(j.dump());
            require(launch_blocker(game, &status).find("adapter") != std::string::npos,
                    "Matching launcher release enabled the unfinished adapter");
            for (const auto &value : {std::string{}, std::string(129, 'x'), std::string("bad/name"),
                                      std::string("line\nbreak")}) {
                j["required_release_set"] = value;
                rejects([&] { parse_status(j.dump()); });
            }
        });
        test("campaign snapshots enforce community identity and bounded content", [] {
            json campaign{{"schema", 1},
                          {"community_id", "test"},
                          {"name", "War"},
                          {"revision", 2u},
                          {"epoch", 1u},
                          {"planets", json::array({{{"id", "planet"},
                                                    {"name", "Planet"},
                                                    {"biome_id", "test-biome"},
                                                    {"owner_faction", "terminid"},
                                                    {"liberation", 500000u}}})},
                          {"templates", json::array()}};
            auto snapshot = parse_campaign(campaign.dump(), "test");
            require(snapshot.planets.size() == 1 && snapshot.planets[0].liberation == 500000,
                    "Campaign data lost");
            rejects([&] { parse_campaign(campaign.dump(), "another"); });
            campaign["planets"].push_back(campaign["planets"][0]);
            rejects([&] { parse_campaign(campaign.dump(), "test"); });
            campaign["planets"].erase(1);
            campaign["planets"][0]["liberation"] = 1000001;
            rejects([&] { parse_campaign(campaign.dump(), "test"); });
        });
        test("player credentials are community scoped and never expose parse fragments", [] {
            PlayerCredential c{"test-community", std::string(32, 'a'),
                               "cdp_" + std::string(64, 'b')};
            auto j = encode_player_credential(c);
            require(parse_player_credential(j.dump(), c.community_id).player_id == c.player_id,
                    "Credential fields lost");
            rejects([&] { parse_player_credential(j.dump(), "another"); });
            auto bad = j;
            bad["token"] = "cdp_inject\r\nAuthorization: bad";
            rejects([&] { parse_player_credential(bad.dump(), c.community_id); });
            rejects([&] { parse_player_credential(std::string(16385, 'x'), c.community_id); });
            auto duplicate = j.dump();
            duplicate.insert(1, "\"schema\":1,");
            rejects([&] { parse_player_credential(duplicate, c.community_id); });
            rejects([&] {
                parse_player_credential("{\"token\":" + std::string(2000, '[') + "0" +
                                            std::string(2000, ']') + "}",
                                        c.community_id);
            });
            auto malformed = "{\"token\":\"" + c.token + "\",bad";
            try {
                parse_player_credential(malformed, c.community_id);
                throw std::logic_error("Accepted malformed credential");
            } catch (const std::runtime_error &e) {
                require(std::string(e.what()).find(c.token) == std::string::npos,
                        "Credential leaked into diagnostics");
            }
        });
        test("player profile replies bind ownership and enforce bounded progression/loadouts", [] {
            auto pid = std::string(32, 'a');
            json j{{"id", pid},
                   {"name", "Profile"},
                   {"xp", 12},
                   {"missions_completed", 2},
                   {"loadout", {{"armor", "test"}}}};
            auto p = parse_player_profile(j.dump(), pid);
            require(p.xp == 12 && p.loadout.at("armor") == "test", "Player profile fields lost");
            rejects([&] { parse_player_profile(j.dump(), std::string(32, 'b')); });
            auto bad = j;
            bad["xp"] = -1;
            rejects([&] { parse_player_profile(bad.dump(), pid); });
            bad = j;
            bad["xp"] = uint64_t(INT64_MAX) + 1;
            rejects([&] { parse_player_profile(bad.dump(), pid); });
            bad = j;
            bad["loadout"] = {{"blob", std::string(8192, 'x')}};
            rejects([&] { parse_player_profile(bad.dump(), pid); });
            rejects([&] {
                parse_player_profile("{\"loadout\":" + std::string(2000, '[') + "0" +
                                         std::string(2000, ']') + "}",
                                     pid);
            });
            bad = j;
            bad["name"] = "hidden\nline";
            rejects([&] { parse_player_profile(bad.dump(), pid); });
        });
        test("exact client fingerprint catalog never grants integration", [] {
            ClientIdentity identity;
            identity.installation.valid = true;
            identity.installation.build = "12345";
            json files = json::object();
            for (auto path : client_binary_paths) {
                FileFingerprint f;
                f.path = std::string(path);
                f.sha256 = std::string(64, 'a');
                f.bytes = 10;
                identity.files.push_back(f);
                files[f.path] = {{"sha256", f.sha256}, {"bytes", f.bytes}};
            }
            json catalog = {{"schema", 1},
                            {"profiles", json::array({{{"id", "test-build"},
                                                       {"steam_build", "12345"},
                                                       {"files", files},
                                                       {"integration_status", "supported"},
                                                       {"available", true}}})}};
            auto report = identity_report(identity, catalog);
            require(report["identity"]["known"] == true &&
                        report["integration"]["available"] == false,
                    "An observed identity granted game launch");
            identity.files[0].sha256 = std::string(64, 'b');
            require(identity_report(identity, catalog)["identity"]["known"] == false,
                    "Build ID alone matched changed client files");
            identity.files.pop_back();
            require(identity_report(identity, catalog)["identity"]["known"] == false,
                    "Incomplete client inspection matched a profile");
        });
        fs::remove_all(root);
        std::cout << passed << " core checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\nFixture: " << root << '\n';
        return 1;
    }
}
