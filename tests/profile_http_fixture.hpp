#pragma once
#include "community/platform.hpp"
#include <httplib.h>
#include <atomic>
#include <thread>
namespace cd::test {
struct ProfileHttpFixture {
    httplib::Server server;
    std::thread worker;
    std::atomic<int> mode{0}, trap_requests{0};
    std::atomic<bool> public_auth{false}, correct_auth{true};
    PlayerCredential credential{"profile-fixture", std::string(32, 'a'),
                                "cdp_" + std::string(64, 'b')};
    int port = 0;
    ProfileHttpFixture() {
        server.Get("/community/v1/status", [&](const auto &request, auto &response) {
            if (request.has_header("Authorization"))
                public_auth = true;
            response.set_content(json{{"protocol", 1},
                                      {"id", credential.community_id},
                                      {"name", "Profile fixture"},
                                      {"game_builds", json::array()},
                                      {"capabilities", {"profiles"}}}
                                     .dump(),
                                 "application/json");
        });
        server.Get("/community/v1/me", [&](const auto &request, auto &response) {
            if (request.get_header_value("Authorization") != "Bearer " + credential.token)
                correct_auth = false;
            if (mode == 0) {
                response.set_redirect("http://127.0.0.1:" + std::to_string(port) +
                                      "/credential-trap");
                return;
            }
            if (mode == 1) {
                response.set_content(std::string(33000, 'x'), "application/json");
                return;
            }
            if (mode == 3) {
                response.status = 401;
                response.set_content(credential.token, "application/json");
                return;
            }
            response.set_content(json{{"id", std::string(32, 'c')},
                                      {"name", "Wrong player"},
                                      {"xp", 0},
                                      {"missions_completed", 0},
                                      {"loadout", json::object()}}
                                     .dump(),
                                 "application/json");
        });
        server.Get("/credential-trap", [&](const auto &, auto &response) {
            ++trap_requests;
            response.set_content("{}", "application/json");
        });
        port = server.bind_to_any_port("127.0.0.1");
        if (port <= 0)
            throw std::runtime_error("Profile fixture bind failed");
        worker = std::thread([&] { server.listen_after_bind(); });
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!server.is_running() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (!server.is_running()) {
            server.stop();
            worker.join();
            throw std::runtime_error("Profile fixture failed to start");
        }
    }
    ~ProfileHttpFixture() {
        server.stop();
        if (worker.joinable())
            worker.join();
    }
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port) + "/community"; }
};
} // namespace cd::test
