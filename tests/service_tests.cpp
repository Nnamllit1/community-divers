#include "community/service.hpp"
#include <httplib.h>
#ifdef _WIN32
#include "community/platform.hpp"
#include "profile_http_fixture.hpp"
#endif
#include <sqlite3.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using namespace cd;
using namespace cd::service;
namespace {
void require(bool ok, const std::string &m) {
    if (!ok)
        throw std::runtime_error(m);
}
template <class F> void rejects(F f, int status = 0) {
    bool failed = false;
    try {
        f();
    } catch (const Error &e) {
        failed = !status || status == e.status;
    } catch (const std::exception &) {
        failed = !status;
    }
    require(failed, "Expected rejection was not observed");
}
struct Live {
    Store store;
    Server server;
    std::thread worker;
    int port;
    Live(const fs::path &dir, const Config &c) : store(dir, c), server(store) {
        port = server.bind(0);
        require(port > 0, "Could not bind test service");
        worker = std::thread([this] { server.listen(); });
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!server.running() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        require(server.running(), "Service did not start");
    }
    ~Live() {
        server.stop();
        if (worker.joinable())
            worker.join();
    }
};
json response(const httplib::Result &r, int code = 200) {
    require(static_cast<bool>(r), "HTTP service request failed");
    require(r->status == code,
            "Unexpected HTTP status " + std::to_string(r->status) + ": " + r->body);
    return json::parse(r->body);
}
httplib::Headers auth(const std::string &token) {
    return {{"Authorization", "Bearer " + token}};
}
httplib::Headers versioned(const std::string &token) {
    auto h = auth(token);
    h.emplace("X-Community-Launcher", "test-launcher");
    h.emplace("X-Community-Release-Set", "test-set");
    return h;
}
void write(const fs::path &p, const std::string &body) {
    std::ofstream(p, std::ios::binary | std::ios::trunc) << body;
}
} // namespace
int main() {
    auto root =
        fs::temp_directory_path() /
        from_utf8("community-divers-service-tests-" +
                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                  "-\xc3\xa4");
    fs::create_directories(root);
    int passed = 0;
    auto test = [&](const char *name, auto body) {
        body();
        ++passed;
        std::cout << "PASS " << name << '\n';
    };
    try {
        Config config;
        config.id = "test-community";
        config.name = "Test Community";
        config.campaign = "Persistent war";
        config.game_builds = {"12345"};
        config.required_launcher_release = "test-launcher";
        config.required_release_set = "test-set";
        auto data = root / "state";
        initialize(data, config);
        auto admin = read_text(data / "operator.token");
        test("native service secrets and SHA-256", [&] {
            require(sha256_text("abc") ==
                        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                    "Service SHA-256 incorrect");
            require(admin.size() == 64 && random_hex(32) != admin, "Random service token invalid");
            rejects([&] { initialize(data, config); });
            auto bad = encode_config(config);
            bad["listen"]["host"] = "0.0.0.0";
            rejects([&] { parse_config(bad); });
        });
#ifndef _WIN32
        test("service state files are owner-only", [&] {
            auto private_bits = fs::perms::group_all | fs::perms::others_all;
            require((fs::status(data).permissions() & private_bits) == fs::perms::none &&
                        (fs::status(data / "operator.token").permissions() & private_bits) ==
                            fs::perms::none &&
                        (fs::status(data / "state.sqlite").permissions() & private_bits) ==
                            fs::perms::none,
                    "Service data permissions are too broad");
        });
#endif
        std::string player, other, pid, mission;
        json recorded;
        {
            Live live(data, config);
            httplib::Client client("127.0.0.1", live.port);
            client.set_connection_timeout(2, 0);
            client.set_read_timeout(5, 0);
            test("real HTTP status and empty persisted campaign", [&] {
                auto status = response(client.Get("/v1/status"));
                require(parse_status(status.dump()).id == config.id,
                        "Launcher status contract mismatch");
                auto campaign = response(client.Get("/v1/campaign"));
                require(campaign["name"] == config.campaign && campaign["planets"].empty(),
                        "Unconfigured content was invented");
                require(status.dump().find(admin) == std::string::npos,
                        "Operator token leaked into status");
            });
#ifdef _WIN32
            test("native launcher probe fetches the advertised campaign", [&] {
                auto pending = probe_service("http://127.0.0.1:" + std::to_string(live.port));
                auto result = pending.get();
                require(result.status && result.campaign &&
                            result.campaign->community_id == config.id &&
                            result.campaign_error.empty(),
                        "Native campaign probe failed");
            });
#endif
            test("profile and operator authorization", [&] {
                response(client.Get("/v1/me"), 401);
                response(
                    client.Post("/v1/admin/players", R"({"name":"Player"})", "application/json"),
                    403);
                auto issued = response(client.Post("/v1/admin/players", auth(admin),
                                                   R"({"name":"Player"})", "application/json"));
                player = issued["token"].get<std::string>();
                pid = issued["player_id"].get<std::string>();
                other = live.store.issue_player("Other")["token"].get<std::string>();
                require(response(client.Get("/v1/me", auth(player)))["id"] == pid,
                        "Profile ownership failed");
            });
#ifdef _WIN32
            test("native player sign-in reads only its authenticated profile", [&] {
                auto url = "http://127.0.0.1:" + std::to_string(live.port);
                PlayerCredential credential{config.id, pid, player};
                auto result = request_player_profile(url, credential);
                require(result.profile && result.credential && result.profile->id == pid &&
                            result.profile->name == "Player",
                        "Native authenticated profile request failed");
                credential.player_id = live.store.profile(other)["id"].get<std::string>();
                require(!request_player_profile(url, credential).profile,
                        "Another player's profile identity was accepted");
                credential.player_id = pid;
                credential.token = "cdp_" + std::string(64, '0');
                auto invalid = request_player_profile(url, credential);
                require(!invalid.profile && invalid.error.find("401") != std::string::npos,
                        "Invalid credential accepted");
            });
#endif
            json planet{{"id", "test-planet"},
                        {"name", "Configured planet"},
                        {"biome_id", "test-biome"},
                        {"owner_faction", "terminid"},
                        {"liberation", 0}};
            json templ{{"id", "test-template"},
                       {"name", "Configured mission"},
                       {"reward_xp", 100},
                       {"war_points", 250}};
            test("operator-owned planet and mission rules persist", [&] {
                response(client.Post("/v1/admin/planets", auth(player), planet.dump(),
                                     "application/json"),
                         403);
                response(client.Post("/v1/admin/planets", auth(admin), planet.dump(),
                                     "application/json"));
                response(client.Post("/v1/admin/templates", auth(admin), templ.dump(),
                                     "application/json"));
                auto campaign = response(client.Get("/v1/campaign"));
                require(campaign["planets"].size() == 1 && campaign["templates"].size() == 1 &&
                            campaign["revision"] == 2,
                        "Campaign changes were not recorded");
            });
            test("loadouts are isolated to the authenticated profile", [&] {
                json loadout{
                    {"armor", nullptr}, {"primary", "test-rifle"}, {"passives", {"one", "two"}}};
                require(response(client.Put("/v1/me/loadout", auth(player), loadout.dump(),
                                            "application/json"))["loadout"] == loadout,
                        "Loadout was not persisted");
                require(response(client.Get("/v1/me", auth(other)))["loadout"].empty(),
                        "Loadout changed another profile");
            });
            json request{{"planet_id", "test-planet"}, {"template_id", "test-template"}};
            test("mission admission checks declared community versions", [&] {
                response(
                    client.Post("/v1/missions", auth(player), request.dump(), "application/json"),
                    409);
                auto lease = response(client.Post("/v1/missions", versioned(player), request.dump(),
                                                  "application/json"));
                mission = lease["id"].get<std::string>();
                require(lease["player_id"] == pid, "Mission owner changed");
            });
            test("mission results reject foreign owners and client rewards", [&] {
                auto path = "/v1/missions/" + mission + "/result";
                response(
                    client.Post(path, auth(other), R"({"outcome":"success"})", "application/json"),
                    403);
                response(client.Post(path, auth(player), R"({"outcome":"success","xp":999999})",
                                     "application/json"),
                         422);
                require(live.store.profile(player)["xp"] == 0,
                        "Rejected result granted progression");
            });
            test("result receipt credits snapshotted rewards exactly once", [&] {
                templ["reward_xp"] = 777;
                templ["war_points"] = 50;
                live.store.put_template(admin, templ);
                auto path = "/v1/missions/" + mission + "/result";
                recorded = response(client.Post(path, auth(player), R"({"outcome":"success"})",
                                                "application/json"));
                require(recorded["xp_awarded"] == 100 && recorded["war_points"] == 250,
                        "Live rule edits changed an existing mission");
                auto revision = live.store.campaign()["revision"];
                auto duplicate = response(client.Post(
                    path, auth(player), R"({"outcome":"success"})", "application/json"));
                require(duplicate["replayed"] == true && live.store.profile(player)["xp"] == 100 &&
                            live.store.campaign()["revision"] == revision,
                        "Duplicate result changed progression/war");
                response(
                    client.Post(path, auth(player), R"({"outcome":"failure"})", "application/json"),
                    409);
            });
            test("liberation receipts report the actual capped contribution", [&] {
                planet["liberation"] = 999990;
                live.store.put_planet(admin, planet);
                auto lease = live.store.start_mission(player, request, "test-launcher", "test-set");
                auto receipt = live.store.finish_mission(player, lease["id"].get<std::string>(),
                                                         {{"outcome", "success"}});
                require(receipt["war_points"] == 10, "Capped war contribution was overstated");
                auto campaign = live.store.campaign();
                require(campaign["planets"][0]["liberation"] == 1000000 &&
                            campaign["planets"][0]["owner_faction"] == "human",
                        "Planet liberation was not applied atomically");
            });
            test("concurrent retries across connections cannot double credit", [&] {
                auto lease = live.store.start_mission(player, request, "test-launcher", "test-set");
                auto mid = lease["id"].get<std::string>();
                Store a(data, config), b(data, config);
                auto before = live.store.profile(player)["xp"].get<int64_t>();
                json left, right;
                std::exception_ptr error1, error2;
                std::thread one([&] {
                    try {
                        left = a.finish_mission(player, mid, {{"outcome", "success"}});
                    } catch (...) {
                        error1 = std::current_exception();
                    }
                });
                std::thread two([&] {
                    try {
                        right = b.finish_mission(player, mid, {{"outcome", "success"}});
                    } catch (...) {
                        error2 = std::current_exception();
                    }
                });
                one.join();
                two.join();
                if (error1)
                    std::rethrow_exception(error1);
                if (error2)
                    std::rethrow_exception(error2);
                require(left["replayed"] != right["replayed"] &&
                            live.store.profile(player)["xp"] == before + 777,
                        "Concurrent results credited twice");
            });
            test("failed missions do not grant XP or successful completion", [&] {
                auto before = live.store.profile(player);
                auto lease = live.store.start_mission(player, request, "test-launcher", "test-set");
                auto receipt = live.store.finish_mission(player, lease["id"].get<std::string>(),
                                                         {{"outcome", "failure"}});
                require(receipt["xp_awarded"] == 0 && receipt["profile"]["xp"] == before["xp"] &&
                            receipt["profile"]["missions_completed"] ==
                                before["missions_completed"],
                        "Failed mission granted progression");
            });
            test("HTTP JSON types, content type and body limits", [&] {
                response(client.Post("/v1/missions", versioned(player), "bad", "application/json"),
                         400);
                response(
                    client.Post("/v1/missions", versioned(player), request.dump(), "text/plain"),
                    415);
                response(client.Put("/v1/me/loadout", auth(player),
                                    "{\"nested\":" + std::string(2000, '[') + "0" +
                                        std::string(2000, ']') + "}",
                                    "application/json"),
                         400);
                auto oversized = client.Put("/v1/me/loadout", auth(player), std::string(70000, 'x'),
                                            "application/json");
                require(oversized && oversized->status == 413, "Oversized service body accepted");
            });
            test("online backup and independent restore preserve the campaign", [&] {
                live.store.backup(root / "backup");
                restore(root / "backup", root / "restored");
                Store copy(root / "restored", config);
                require(copy.campaign() == live.store.campaign() &&
                            copy.profile(player) == live.store.profile(player),
                        "Backup changed campaign/profile state");
                rejects([&] { restore(root / "backup", data); });
                auto original = read_text(root / "backup/config.json");
                write(root / "backup/config.json", original + " ");
                rejects([&] { restore(root / "backup", root / "corrupt-restore"); });
                require(!fs::exists(root / "corrupt-restore"),
                        "Corrupt restore wrote a data directory");
            });
        }
        test("service restart keeps ownership, progression and content", [&] {
            Store reopened(data, config);
            require(reopened.profile(player)["xp"] == 1654 && reopened.profile(other)["xp"] == 0,
                    "Progression did not persist across restart");
            require(reopened.campaign()["planets"][0]["liberation"] == 1000000,
                    "War state did not persist");
            auto wrong = config;
            wrong.id = "different-community";
            rejects([&] { Store bad(data, wrong); });
            require(read_text(data / "state.sqlite").find(player) == std::string::npos,
                    "Plaintext player token stored in SQLite");
        });
#ifdef _WIN32
        test("authenticated profile redirects, response bounds and diagnostics", [&] {
            cd::test::ProfileHttpFixture f;
            auto result = request_player_profile(f.url(), f.credential);
            require(!result.profile && result.error.find("302") != std::string::npos &&
                        f.trap_requests == 0,
                    "Authenticated redirect was followed");
            f.mode = 1;
            result = request_player_profile(f.url(), f.credential);
            require(!result.profile && result.error.find("32 KiB") != std::string::npos,
                    "Oversized player profile accepted");
            f.mode = 2;
            result = request_player_profile(f.url(), f.credential);
            require(!result.profile && result.error.find("identity") != std::string::npos,
                    "Different player identity accepted");
            f.mode = 3;
            result = request_player_profile(f.url(), f.credential);
            require(!result.profile && result.error.find("401") != std::string::npos &&
                        result.error.find(f.credential.token) == std::string::npos,
                    "Private error body leaked into diagnostics");
            require(!f.public_auth && f.correct_auth,
                    "Bearer attached to public status or wrong profile request");
        });
#endif
        test("unrelated and forward-version databases are rejected", [&] {
            private_directory(root / "foreign");
            private_write_new(root / "foreign/operator.token", admin);
            private_write_new(root / "foreign/state.sqlite", {});
            sqlite3 *db = nullptr;
            sqlite3_open(to_utf8(root / "foreign/state.sqlite").c_str(), &db);
            sqlite3_exec(
                db, "CREATE TABLE user_data(value TEXT); INSERT INTO user_data VALUES('keep');",
                nullptr, nullptr, nullptr);
            sqlite3_close(db);
            auto before = sha256_file(root / "foreign/state.sqlite");
            rejects([&] { Store bad(root / "foreign", config); });
            require(sha256_file(root / "foreign/state.sqlite") == before,
                    "Opening changed an unrelated database");
            sqlite3_open(to_utf8(root / "restored/state.sqlite").c_str(), &db);
            sqlite3_exec(db, "PRAGMA user_version=99", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            rejects([&] { Store future(root / "restored", config); });
        });
        require(fs::weakly_canonical(root.parent_path()) ==
                        fs::weakly_canonical(fs::temp_directory_path()) &&
                    to_utf8(root.filename()).starts_with("community-divers-service-tests-"),
                "Unsafe service test cleanup path");
        fs::remove_all(root);
        std::cout << passed << " service checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << "\nFixture: " << to_utf8(root) << '\n';
        return 1;
    }
}
