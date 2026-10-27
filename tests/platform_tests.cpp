#include "http_fixture.hpp"
#include <iostream>
#include <fstream>
using namespace cd;
using cd::test::Server;

void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data))
        return 1;
    try {
        if (argc == 3 && std::string(argv[1]) == "--https-status") {
            require(parse_endpoint(argv[2]).secure, "Online status check requires HTTPS");
            auto request = probe_service(argv[2]);
            require(request.wait_for(std::chrono::seconds(12)) == std::future_status::ready,
                    "HTTPS status request timed out");
            auto result = request.get();
            if (!result.status)
                throw std::runtime_error(result.error);
            require(result.status->id == "community-divers-protocol-fixture",
                    "Unexpected online status fixture");
            std::cout << "PASS real HTTPS service-status exchange\n";
            WSACleanup();
            return 0;
        }
        {
            auto scope =
                fs::temp_directory_path() /
                from_utf8(
                    "community-divers-vault-" +
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            PlayerCredential c{"vault-community", std::string(32, 'a'),
                               "cdp_" + std::string(64, 'b')};
            auto url = "http://127.0.0.1:8080/community";
            try {
                require(!saved_player(scope, url, c.community_id),
                        "Unexpected existing vault fixture");
                remember_player(scope, url, c);
                auto loaded = saved_player(scope, url, c.community_id);
                require(loaded && loaded->token == c.token && loaded->player_id == c.player_id,
                        "Native secure credential storage failed");
                require(
                    !saved_player(scope, url, "other-community") &&
                        !saved_player(scope, "http://127.0.0.1:8081/community", c.community_id) &&
                        !saved_player(scope / "separate-launcher", url, c.community_id),
                    "Credential crossed its scope");
                forget_player(scope, url, c.community_id);
                require(!saved_player(scope, url, c.community_id), "Sign-out retained credential");
                require(!fs::exists(scope), "Native credential was written into launcher files");
            } catch (...) {
                forget_player(scope, url, c.community_id);
                throw;
            }
            std::cout
                << "PASS Windows credential persistence, scope isolation and exact sign-out\n";
        }
        {
            auto file =
                fs::temp_directory_path() /
                ("community-divers-sha-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                 ".txt");
            {
                std::ofstream out(file, std::ios::binary);
                out << "abc";
            }
            auto fingerprint = fingerprint_file(file, "fixture");
            fs::remove(file);
            require(fingerprint.error.empty() && fingerprint.bytes == 3 &&
                        fingerprint.sha256 ==
                            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                    "SHA-256 fingerprint failed");
            require(!fingerprint_file(file, "missing").error.empty(),
                    "Missing file was silently fingerprinted");
            std::cout << "PASS native client-file fingerprinting\n";
        }
        {
            Server server(
                200,
                R"({"protocol":1,"id":"local-test","name":"Local test","game_builds":["12345"],"campaign":{"name":"Test campaign"}})");
            auto result = server.probe();
            require(result.status && result.status->campaign == "Test campaign",
                    "Valid status response failed");
            require(server.request.starts_with("GET /community/v1/status HTTP/1.1"),
                    "Incorrect base-path routing");
            require(server.request.find("Authorization:") == std::string::npos,
                    "Unexpected credentials");
            std::cout << "PASS status and base-path request\n";
        }
        {
            Server server(302, "", "Location: http://127.0.0.1:9/forbidden\r\n");
            auto result = server.probe();
            require(!result.status && result.error.find("302") != std::string::npos,
                    "Redirect was followed or accepted");
            std::cout << "PASS redirects are rejected\n";
        }
        {
            Server server(503, "unavailable");
            auto result = server.probe();
            require(!result.status && result.error.find("503") != std::string::npos,
                    "HTTP failure not reported");
            std::cout << "PASS HTTP errors\n";
        }
        {
            Server server(200, "not json");
            auto result = server.probe();
            require(!result.status && !result.error.empty(), "Malformed JSON accepted");
            std::cout << "PASS malformed service response\n";
        }
        {
            Server server(200, std::string(270000, 'x'));
            auto result = server.probe();
            require(!result.status && result.error.find("256 KiB") != std::string::npos,
                    "Oversized response accepted");
            std::cout << "PASS response size limit\n";
        }
        {
            PlayerCredential c{"profile-test", std::string(32, 'a'), "cdp_" + std::string(64, 'b')};
            Server changed(
                200,
                R"({"protocol":1,"id":"different-community","name":"Changed","game_builds":[],"capabilities":["profiles"]})");
            auto result = request_player_profile(changed.url(), c);
            changed.worker.join();
            require(!result.profile &&
                        changed.request.find("Authorization:") == std::string::npos &&
                        result.error.find(c.token) == std::string::npos,
                    "Changed community received a credential or exposed it in diagnostics");
            Server redirect(302, "", "Location: http://127.0.0.1:9/private\r\n");
            result = request_player_profile(redirect.url(), c);
            redirect.worker.join();
            require(!result.profile && result.error.find("302") != std::string::npos &&
                        redirect.request.find("Authorization:") == std::string::npos,
                    "Pre-auth redirect was accepted");
            std::cout << "PASS endpoint identity changes and redirects stop before credentials are "
                         "sent\n";
        }
        WSACleanup();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        WSACleanup();
        return 1;
    }
}
