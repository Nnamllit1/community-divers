#pragma once
#include <filesystem>
#include <array>
#include <cstdint>
#include <string_view>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace cd {
namespace fs = std::filesystem;
using json = nlohmann::json;
struct KV {
    std::string value;
    std::map<std::string, KV> children;
    const KV *get(const std::string &key) const;
    std::string text(const std::string &key) const;
};
fs::path from_utf8(const std::string &value);
std::string to_utf8(const fs::path &value);
KV parse_vdf(const std::string &text);
std::string read_text(const fs::path &path, size_t limit = 2 * 1024 * 1024);
struct Installation {
    fs::path root;
    std::string build;
    bool valid = false;
    std::string detail;
};
Installation inspect_installation(const fs::path &root);
std::vector<Installation> discover_steam(const std::vector<fs::path> &steam_roots);
inline constexpr std::array<std::string_view, 4> client_binary_paths{
    "bin/helldivers2.exe", "data/game/game.dll", "bin/PartyWin.dll",
    "bin/PlayFabMultiplayerWin.dll"};
struct FileFingerprint {
    std::string path, sha256, error;
    uint64_t bytes = 0;
};
struct ClientIdentity {
    Installation installation;
    std::vector<FileFingerprint> files;
};
json identity_report(const ClientIdentity &identity, const json &catalog);
struct Endpoint {
    std::string canonical, host, path;
    unsigned short port = 443;
    bool secure = true;
};
Endpoint parse_endpoint(std::string input);
struct Community {
    std::string id, name, url;
};
struct Profile {
    std::string id = "default", name = "Default";
    std::vector<std::string> enabled_mods;
};
struct Settings {
    std::string game_path, selected_community, selected_profile = "default";
    std::vector<Community> communities;
    std::vector<Profile> profiles{Profile{}};
};
json encode_settings(const Settings &settings);
Settings decode_settings(const json &value);
struct SettingsLoad {
    Settings value;
    std::string error;
};
SettingsLoad load_settings(const fs::path &path);
void save_settings(const fs::path &path, const Settings &settings);
struct Mod {
    std::string id, name, version, description, error;
    fs::path directory;
};
std::vector<Mod> discover_mods(const fs::path &directory);
struct ServiceStatus {
    std::string id, name, campaign;
    std::vector<std::string> game_builds;
};
ServiceStatus parse_status(const std::string &body);
std::string launch_blocker(const Installation &game, const ServiceStatus *service);
} // namespace cd
