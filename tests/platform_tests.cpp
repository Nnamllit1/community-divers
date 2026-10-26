#include "http_fixture.hpp"
#include <iostream>
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
        WSACleanup();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        WSACleanup();
        return 1;
    }
}
