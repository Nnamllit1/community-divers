#include <winsock2.h>
#include <ws2tcpip.h>
#include "community/platform.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace cd;
struct Server {
    SOCKET listener = INVALID_SOCKET;
    std::thread worker;
    unsigned short port = 0;
    std::string request;
    Server(int status, std::string body, std::string extra = {}) {
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == INVALID_SOCKET)
            throw std::runtime_error("socket failed");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) ||
            listen(listener, 1))
            throw std::runtime_error("bind/listen failed");
        int size = sizeof(address);
        getsockname(listener, reinterpret_cast<sockaddr *>(&address), &size);
        port = ntohs(address.sin_port);
        worker = std::thread([this, status, body = std::move(body), extra = std::move(extra)] {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(listener, &set);
            timeval timeout{8, 0};
            if (select(0, &set, nullptr, nullptr, &timeout) <= 0)
                return;
            auto connection = accept(listener, nullptr, nullptr);
            if (connection == INVALID_SOCKET)
                return;
            DWORD receive_timeout = 2000;
            setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO,
                       reinterpret_cast<const char *>(&receive_timeout), sizeof(receive_timeout));
            char buffer[4096];
            while (request.find("\r\n\r\n") == std::string::npos && request.size() < 16384) {
                int n = recv(connection, buffer, sizeof(buffer), 0);
                if (n <= 0)
                    break;
                request.append(buffer, n);
            }
            auto response = "HTTP/1.1 " + std::to_string(status) +
                            " Test\r\nContent-Type: application/json\r\nContent-Length: " +
                            std::to_string(body.size()) + "\r\nConnection: close\r\n" + extra +
                            "\r\n" + body;
            size_t sent = 0;
            while (sent < response.size()) {
                int n = send(connection, response.data() + sent,
                             static_cast<int>(response.size() - sent), 0);
                if (n <= 0)
                    break;
                sent += n;
            }
            shutdown(connection, SD_BOTH);
            closesocket(connection);
        });
    }
    ~Server() {
        if (worker.joinable())
            worker.join();
        if (listener != INVALID_SOCKET)
            closesocket(listener);
    }
    std::string url() const { return "http://127.0.0.1:" + std::to_string(port) + "/community"; }
    ProbeResult probe() {
        auto pending = probe_service(url());
        if (pending.wait_for(std::chrono::seconds(12)) != std::future_status::ready)
            throw std::runtime_error("Probe did not finish");
        auto result = pending.get();
        worker.join();
        return result;
    }
};
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data))
        return 1;
    try {
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
