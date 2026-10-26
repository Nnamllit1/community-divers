#pragma once
#include "community/core.hpp"
#include <functional>
#include <future>
#include <optional>
namespace cd {
inline constexpr uint64_t update_package_limit = 512 * 1024 * 1024;
inline constexpr size_t update_manifest_limit = 64 * 1024;
inline constexpr std::string_view preview_feed =
    "https://raw.githubusercontent.com/Nnamllit1/community-divers/main/updates/preview.json";
struct UpdateRelease {
    std::string channel, release_id, source_commit, filename, url, sha256;
    uint64_t revision = 0, bytes = 0;
    int service_protocol = 0;
};
UpdateRelease parse_update_payload(const std::string &payload, const std::string &channel);
enum class UpdateRelation { current, available, local_newer, unknown };
UpdateRelation compare_update(const UpdateRelease &release, std::string_view current_commit,
                              uint64_t current_revision);
// Native crypto and transport. The launcher's trust root is compiled into its binary.
struct UpdateKeyPair {
    std::string private_hex, public_hex;
};
UpdateKeyPair generate_update_key();
std::string sign_update_payload(const std::string &payload, const std::string &private_hex);
UpdateRelease verify_update_envelope(const std::string &envelope, const std::string &channel,
                                     const std::string &public_hex);
std::string update_publisher_key();
struct UpdateCheck {
    std::optional<UpdateRelease> release;
    std::string error;
    std::string envelope;
};
// Custom transport is for deterministic component tests. Production uses HTTPS only.
using UpdateFetch = std::function<std::string(const std::string &, uint64_t)>;
UpdateCheck check_update(const std::string &url, const std::string &public_hex,
                         const UpdateFetch &fetch);
std::future<UpdateCheck> check_update_async();
void remember_update_channel(const UpdateRelease &release, const std::string &envelope,
                             const fs::path &data,
                             const std::string &trust_key = update_publisher_key());
using UpdateSink = std::function<void(std::string_view)>;
using UpdateStream = std::function<void(const std::string &, uint64_t, const UpdateSink &)>;
bool update_transport_url_allowed(const std::string &url);
void stream_update(const std::string &url, uint64_t limit, const UpdateSink &sink);
std::string fetch_update_text(const std::string &url, uint64_t limit);
struct UpdateDownload {
    fs::path package;
    std::string error;
};
UpdateDownload stage_update(const UpdateRelease &release, const fs::path &directory,
                            const UpdateStream &fetch);
std::future<UpdateDownload> stage_update_async(UpdateRelease release, fs::path directory);
} // namespace cd
