#pragma once
#include "community/core.hpp"
#include <future>
#include <optional>
namespace cd {
fs::path user_data_path();
fs::path executable_directory();
std::vector<fs::path> steam_roots();
std::optional<fs::path> choose_game_folder(void *owner);
std::string utf8(const std::wstring &value);
std::wstring wide(const std::string &value);
std::string path_text(const fs::path &value);
void open_folder(const fs::path &path);
struct ProbeResult {
    std::optional<ServiceStatus> status;
    std::string error;
    std::optional<CampaignSnapshot> campaign;
    std::string campaign_error;
};
std::future<ProbeResult> probe_service(std::string url);
FileFingerprint fingerprint_file(const fs::path &path, std::string relative_path);
ClientIdentity inspect_client(const fs::path &path);
std::future<ClientIdentity> inspect_client_async(fs::path path);
} // namespace cd
