#include "community/preparation.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace cd {
std::string prepared_identity_build(std::string_view body,
                                    const std::vector<FileFingerprint> &files) {
    json receipt;
    try {
        if (body.size() > 32 * 1024 * 1024)
            throw std::runtime_error("size");
        std::vector<std::set<std::string>> keys;
        receipt = json::parse(body, [&](int depth, json::parse_event_t event, json &value) {
            if (depth > 16)
                throw std::runtime_error("depth");
            if (event == json::parse_event_t::object_start)
                keys.emplace_back();
            else if (event == json::parse_event_t::object_end)
                keys.pop_back();
            else if (event == json::parse_event_t::key &&
                     !keys.back().insert(value.get<std::string>()).second)
                throw std::runtime_error("duplicate key");
            return true;
        });
        if (receipt.at("schema") != 1 || receipt.at("state") != "copied" ||
            receipt.at("integration_available") != false ||
            receipt.at("profile_isolation_verified") != false ||
            receipt.at("official_network_isolation_verified") != false ||
            !receipt.at("files").is_array() || receipt.at("files").empty() ||
            receipt.at("files").size() > 100000 || !receipt.at("depots").is_array() ||
            receipt.at("depots").empty() || receipt.at("depots").size() > 128 ||
            !receipt.at("bytes").is_number_unsigned())
            throw std::runtime_error("schema");
        const auto build = receipt.at("steam_build").get<std::string>();
        if (build.empty() || build.size() > 20 ||
            build.find_first_not_of("0123456789") != build.npos)
            throw std::runtime_error("build");
        const auto &expected = receipt.at("binary_fingerprints");
        if (!expected.is_object() || expected.size() != client_binary_paths.size())
            throw std::runtime_error("fingerprints");
        for (auto path : client_binary_paths) {
            const auto &entry = expected.at(std::string(path));
            const auto hash = entry.at("sha256").get<std::string>();
            if (!entry.at("bytes").is_number_unsigned() || hash.size() != 64 ||
                hash.find_first_not_of("0123456789abcdef") != hash.npos)
                throw std::runtime_error("fingerprint");
        }
    } catch (const std::exception &) {
        throw std::runtime_error(
            "Invalid or incomplete preparation receipt. Verify or prepare the copy again.");
    }
    std::set<std::string> seen;
    if (files.size() != client_binary_paths.size())
        throw std::runtime_error("Prepared-copy identity requires all four binary fingerprints.");
    for (const auto &file : files) {
        if (!file.error.empty() || !seen.insert(file.path).second ||
            std::find(client_binary_paths.begin(), client_binary_paths.end(), file.path) ==
                client_binary_paths.end() ||
            receipt["binary_fingerprints"][file.path]["sha256"] != file.sha256 ||
            receipt["binary_fingerprints"][file.path]["bytes"] != file.bytes)
            throw std::runtime_error("Prepared binaries differ from the receipt or could not be "
                                     "read. Verify the copy again.");
    }
    return receipt["steam_build"].get<std::string>();
}
} // namespace cd
