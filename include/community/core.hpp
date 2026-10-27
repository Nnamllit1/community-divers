#pragma once
#include "community/mod_types.hpp"
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
    std::string build_source = "unknown", metadata_error;
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
struct Settings {
    bool check_updates = true;
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
struct ServiceStatus {
    std::string id, name, campaign, required_release_set, required_launcher_release;
    std::vector<std::string> game_builds, capabilities;
};
struct CampaignPlanet {
    std::string id, name, biome_id, owner_faction;
    uint32_t liberation = 0;
};
struct CampaignTemplate {
    std::string id, name;
    uint32_t reward_xp = 0, war_points = 0;
};
struct CampaignSnapshot {
    std::string community_id, name;
    uint64_t revision = 0, epoch = 0;
    std::vector<CampaignPlanet> planets;
    std::vector<CampaignTemplate> templates;
};
CampaignSnapshot parse_campaign(const std::string &body, const std::string &community_id);
struct PlayerCredential {
    std::string community_id, player_id, token;
};
struct PlayerProfile {
    std::string id, name;
    uint64_t xp = 0, missions_completed = 0;
    json loadout = json::object();
};
PlayerCredential parse_player_credential(const std::string &body, const std::string &community_id);
json encode_player_credential(const PlayerCredential &credential);
PlayerProfile parse_player_profile(const std::string &body, const std::string &player_id);
ServiceStatus parse_status(const std::string &body);
std::string launch_blocker(const Installation &game, const ServiceStatus *service);
} // namespace cd
