#include "community/core.hpp"
#include "community/version.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace cd {
namespace {
template <size_t N>
bool complete_files(const std::vector<FileFingerprint> &files,
                    const std::array<std::string_view, N> &paths) {
    if (files.size() != paths.size())
        return false;
    std::set<std::string> seen;
    for (const auto &file : files) {
        if (!file.error.empty() || file.sha256.size() != 64 ||
            file.sha256.find_first_not_of("0123456789abcdef") != std::string::npos ||
            !seen.insert(file.path).second ||
            std::find(paths.begin(), paths.end(), file.path) == paths.end())
            return false;
    }
    return true;
}
bool complete(const ClientIdentity &identity) {
    return identity.metadata_error.empty() && identity.installation.valid &&
           !identity.installation.build.empty() &&
           complete_files(identity.files, client_binary_paths) &&
           complete_files(identity.startup_files, client_startup_paths);
}
bool same_files(const std::vector<FileFingerprint> &observed, const json &expected) {
    if (!expected.is_object() || expected.size() != observed.size())
        return false;
    for (const auto &file : observed)
        if (!expected.contains(file.path) || expected.at(file.path).at("sha256") != file.sha256 ||
            expected.at(file.path).at("bytes") != file.bytes)
            return false;
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
        if (!profile.contains("startup_files"))
            continue;
        if (same_files(identity.files, profile.at("files")) &&
            same_files(identity.startup_files, profile.at("startup_files"))) {
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
                {"build_source", identity.build_source},
                {"files", json::array()},
                {"startup_files", json::array()},
                {"integration",
                 {{"available", false},
                  {"state", "not_implemented"},
                  {"detail", "Native client startup, service routing and profile isolation have "
                             "not been verified."}}}};
    const auto append_files = [](json &target, const std::vector<FileFingerprint> &files) {
        for (const auto &file : files) {
            json item = {{"path", file.path}, {"bytes", file.bytes}, {"sha256", file.sha256}};
            if (!file.error.empty())
                item["error"] = file.error;
            target.push_back(std::move(item));
        }
    };
    append_files(out["files"], identity.files);
    append_files(out["startup_files"], identity.startup_files);
    if (!identity.metadata_error.empty())
        out["metadata_error"] = identity.metadata_error;
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
