// SPDX-License-Identifier: LicenseRef-Community-Divers-Service-1.0
// Copyright (c) 2026 Community Divers contributors
// Earlier MIT grants remain valid; see LICENSE and docs/licensing-plan.md.
#include "community/service.hpp"
#include "community/version.hpp"
#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>
namespace {
volatile std::sig_atomic_t stopping = 0;
void stop(int) {
    stopping = 1;
}
} // namespace
int run_service(int argc, char **argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--version") {
            std::cout << "CommunityDiversService " << cd::version::release_id << '\n';
            return 0;
        }
        if (argc < 3)
            throw std::runtime_error(
                "Usage: CommunityDiversService init|serve|player|backup|restore DATA [--config "
                "FILE] [--name NAME] [--output PATH]");
        std::string action = argv[1], name;
        cd::fs::path data = cd::from_utf8(argv[2]), config_path, output;
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (i + 1 >= argc)
                throw std::runtime_error("Option needs a value");
            if (arg == "--config")
                config_path = cd::from_utf8(argv[++i]);
            else if (arg == "--name")
                name = argv[++i];
            else if (arg == "--output")
                output = cd::from_utf8(argv[++i]);
            else
                throw std::runtime_error("Unknown service option");
        }
        if (action == "restore") {
            if (output.empty())
                throw std::runtime_error("Restore requires --output NEW_DATA_DIRECTORY");
            cd::service::restore(data, output);
            std::cout << "Backup restored to a new data directory.\n";
            return 0;
        }
        if (action == "init") {
            cd::service::Config config;
            if (!config_path.empty())
                config = cd::service::parse_config(cd::json::parse(cd::read_text(config_path)));
            else {
                config.id = "community-" + cd::service::random_hex(8);
                config.name = "Community Divers";
                config.campaign = "Community campaign";
            }
            cd::service::initialize(data, config);
            std::cout << "Community data initialized. Operator credential is in operator.token; "
                         "keep it private.\n";
            return 0;
        }
        auto config =
            cd::service::parse_config(cd::json::parse(cd::read_text(data / "config.json")));
        cd::service::Store store(data, config);
        if (action == "player") {
            if (name.empty() || output.empty())
                throw std::runtime_error(
                    "Player creation requires --name and --output CREDENTIAL_FILE");
            if (cd::fs::exists(output))
                throw std::runtime_error("Credential output already exists");
            auto credential = store.issue_player(name);
            cd::service::private_write_new(output, credential.dump(2) + "\n");
            std::cout << "Player credential written privately.\n";
            return 0;
        }
        if (action == "backup") {
            if (output.empty())
                throw std::runtime_error("Backup requires --output NEW_BACKUP_DIRECTORY");
            store.backup(output);
            std::cout << "Verified backup created.\n";
            return 0;
        }
        if (action != "serve")
            throw std::runtime_error("Unknown service action");
        if (cd::service::source_information(config)["url"].get<std::string>().empty())
            std::cerr << "Set source_url in config.json before serving remote users; "
                         "this build has no exact published source link.\n";
        cd::service::Server server(store);
        int port = server.bind();
        if (port < 1)
            throw std::runtime_error("Cannot bind the configured loopback service port");
        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);
        std::thread watcher([&] {
            while (!stopping) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            server.stop();
        });
        std::cout << "Community service listening on " << config.host << ":" << port
                  << "; data is local and independent.\n"
                  << std::flush;
        bool ok = server.listen();
        stopping = 1;
        watcher.join();
        return ok ? 0 : 1;
    } catch (const std::exception &e) {
        std::cerr << "Community service: " << e.what() << '\n';
        return 1;
    }
}

#ifdef _WIN32
#include <windows.h>
int wmain(int argc, wchar_t **args) {
    try {
        std::vector<std::string> values;
        std::vector<char *> pointers;
        for (int i = 0; i < argc; ++i) {
            auto length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, args[i], -1, nullptr,
                                              0, nullptr, nullptr);
            if (!length)
                throw std::runtime_error("Invalid Unicode command argument");
            std::string value(static_cast<size_t>(length), '\0');
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, args[i], -1, value.data(), length,
                                nullptr, nullptr);
            value.pop_back();
            values.push_back(std::move(value));
        }
        for (auto &value : values)
            pointers.push_back(value.data());
        return run_service(argc, pointers.data());
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
#else
int main(int argc, char **argv) {
    return run_service(argc, argv);
}
#endif
