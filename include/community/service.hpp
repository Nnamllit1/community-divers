// SPDX-License-Identifier: LicenseRef-Community-Divers-Service-1.0
// Copyright (c) 2026 Community Divers contributors
// Earlier MIT grants remain valid; see LICENSE and docs/licensing-plan.md.
#pragma once
#include "community/core.hpp"
#include <memory>
#include <stdexcept>
namespace cd::service {
struct Error : std::runtime_error {
    int status;
    std::string code;
    Error(int s, std::string c, const std::string &m)
        : std::runtime_error(m), status(s), code(std::move(c)) {}
};
struct Config {
    std::string id, name, campaign, host = "127.0.0.1", required_release_set,
                                    required_launcher_release, source_url;
    int port = 8080;
    std::vector<std::string> game_builds;
};
Config parse_config(const json &value);
json encode_config(const Config &config);
json source_information(const Config &config);
std::string random_hex(size_t bytes);
std::string sha256_text(std::string_view body);
std::string sha256_file(const fs::path &file);
void private_directory(const fs::path &path);
void private_write_new(const fs::path &path, std::string_view body);
void initialize(const fs::path &directory, const Config &config);
class Store {
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    Store(const fs::path &directory, Config config, bool create = false);
    ~Store();
    Store(const Store &) = delete;
    const Config &config() const;
    json status();
    json campaign();
    void require_admin(const std::string &token);
    json issue_player(const std::string &name);
    json profile(const std::string &token);
    json save_loadout(const std::string &token, const json &body);
    json put_planet(const std::string &admin, const json &body);
    json put_template(const std::string &admin, const json &body);
    json start_mission(const std::string &token, const json &body,
                       const std::string &launcher_release, const std::string &release_set);
    json finish_mission(const std::string &token, const std::string &mission, const json &body);
    void backup(const fs::path &new_directory);
};
void restore(const fs::path &backup, const fs::path &new_directory);
class Server {
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    explicit Server(Store &store);
    ~Server();
    int bind(int port = -1);
    bool listen();
    void stop();
    bool running() const;
};
} // namespace cd::service
