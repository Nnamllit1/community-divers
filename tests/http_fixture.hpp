#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include "community/platform.hpp"
#include <chrono>
#include <stdexcept>
#include <thread>
namespace cd::test {
struct Server {
    SOCKET listener = INVALID_SOCKET;
    std::thread worker;
    unsigned short port = 0;
    std::string request;
    Server(int status, std::string body, std::string extra = {},
           std::chrono::milliseconds delay = {}) {
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
        worker = std::thread([this, status, body = std::move(body), extra = std::move(extra),
                              delay] {
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
            std::this_thread::sleep_for(delay);
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
} // namespace cd::test
