#pragma once
#include "community/updates.hpp"
namespace cd::test {
inline json update_payload() {
    const std::string filename = "CommunityDivers-0.1.0-preview.1-aaaaaaaa-windows-x64.zip";
    return {{"schema", 1},
            {"channel", "preview"},
            {"platform", "windows-x64"},
            {"release_id", "0.1.0-preview.1+aaaaaaaa"},
            {"source_commit", std::string(40, 'a')},
            {"source_revision", 6u},
            {"service_protocol", 1u},
            {"filename", filename},
            {"bytes", 3u},
            {"sha256", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
            {"url",
             "https://github.com/Nnamllit1/community-divers/releases/download/v0.1.0-preview.1/" +
                 filename}};
}
inline void update_require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void update_rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    update_require(rejected, "Invalid update was accepted");
}
} // namespace cd::test
