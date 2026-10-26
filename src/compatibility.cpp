#include "community/core.hpp"
#include "community/version.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace cd {
namespace {
bool complete(const ClientIdentity &identity) {
    if (!identity.installation.valid || identity.installation.build.empty() ||
        identity.files.size() != client_binary_paths.size())
        return false;
    std::set<std::string> seen;
    for (const auto &file : identity.files) {
        if (!file.error.empty() || file.sha256.size() != 64 ||
            file.sha256.find_first_not_of("0123456789abcdef") != std::string::npos ||
            !seen.insert(file.path).second ||
            std::find(client_binary_paths.begin(), client_binary_paths.end(), file.path) ==
                client_binary_paths.end())
            return false;
    }
    return true;
}
std::string match(const ClientIdentity &identity, const json &catalog) {
    if (catalog.at("schema") != 1 || !catalog.at("profiles").is_array() ||
        catalog.at("profiles").size() > 1000)
        throw std::runtime_error("Invalid build catalog schema");
    if (!complete(identity))
        return {};
    std::string found;
    for (const auto &profile : catalog.at("profiles")) {
        if (profile.at("steam_build").get<std::string>() != identity.installation.build)
            continue;
        const auto &files = profile.at("files");
        if (!files.is_object() || files.size() != identity.files.size())
            continue;
        bool equal = true;
        for (const auto &file : identity.files) {
            if (!files.contains(file.path) || files.at(file.path).at("sha256") != file.sha256 ||
                files.at(file.path).at("bytes") != file.bytes) {
                equal = false;
                break;
            }
        }
        if (equal) {
            if (!found.empty())
                throw std::runtime_error("Ambiguous build catalog match");
            found = profile.at("id").get<std::string>();
            if (found.empty() || found.size() > 128)
                throw std::runtime_error("Invalid build catalog identity");
        }
    }
    return found;
}
} // namespace
json identity_report(const ClientIdentity &identity, const json &catalog) {
    json out = {{"schema", 1},
                {"installation_detected", identity.installation.valid},
                {"steam_build", identity.installation.build},
                {"files", json::array()},
                {"integration",
                 {{"available", false},
                  {"state", "not_implemented"},
                  {"detail", "Native client startup, service routing and profile isolation have "
                             "not been verified."}}}};
    for (const auto &file : identity.files) {
        json item = {{"path", file.path}, {"bytes", file.bytes}, {"sha256", file.sha256}};
        if (!file.error.empty())
            item["error"] = file.error;
        out["files"].push_back(std::move(item));
    }
    out["launcher_release"] = std::string(version::release_id);
    out["identity"] = {{"known", false}, {"profile", ""}};
    try {
        auto profile = match(identity, catalog);
        out["identity"] = {{"known", !profile.empty()}, {"profile", profile}};
    } catch (const std::exception &e) {
        out["catalog_error"] = e.what();
    }
    return out;
}
} // namespace cd
