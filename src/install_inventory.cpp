#include "community/installer.hpp"
#include <algorithm>
#include <cctype>
#include <set>
namespace cd {
namespace {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
bool digest(const std::string &s) {
    return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
} // namespace
bool managed_update_path(const std::string &path) {
    if (path.empty() || path.size() > 240 || path.front() == '/' || path.back() == '/')
        return false;
    for (unsigned char c : path)
        if (c < 33 || c > 126 || c == '\\' || c == ':' || c == '"' || c == '<' || c == '>' ||
            c == '|' || c == '?' || c == '*' || c == '~')
            return false;
    size_t start = 0;
    while (start < path.size()) {
        auto end = path.find('/', start);
        auto part = path.substr(start, end - start);
        if (part.empty() || part == "." || part == ".." || part.back() == '.')
            return false;
        auto base = lower(part.substr(0, part.find('.')));
        if (base == "con" || base == "prn" || base == "aux" || base == "nul" ||
            (base.size() == 4 && (base.starts_with("com") || base.starts_with("lpt")) &&
             base.back() >= '0' && base.back() <= '9'))
            return false;
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    if (path.starts_with("assets/brand/") || path.starts_with("docs/") ||
        path.starts_with("licenses/"))
        return true;
    const std::set<std::string> fixed = {"CommunityDivers.exe",
                                         "CommunityDiversInspect.exe",
                                         "CommunityDiversUpdate.exe",
                                         "build-info.json",
                                         "install-inventory.json",
                                         "LICENSE",
                                         "THIRD_PARTY.md",
                                         "README.md",
                                         "PROJECT.md",
                                         "CONTRIBUTING.md",
                                         "compatibility/client-builds.json",
                                         "updates/preview.json",
                                         "updates/publisher-key.txt"};
    return fixed.contains(path);
}
std::vector<ManagedFile> parse_install_inventory(const std::string &body) {
    if (body.size() > 2 * 1024 * 1024)
        throw std::runtime_error("Install inventory is too large");
    auto j = json::parse(body);
    if (j.at("schema") != 1 || !j.at("files").is_array() || j["files"].size() > 4096)
        throw std::runtime_error("Unsupported install inventory");
    std::vector<ManagedFile> out;
    std::set<std::string> names;
    uint64_t total = 0;
    for (const auto &entry : j["files"]) {
        ManagedFile f;
        f.path = entry.at("path").get<std::string>();
        f.sha256 = entry.at("sha256").get<std::string>();
        if (!entry.at("bytes").is_number_unsigned())
            throw std::runtime_error("Invalid inventory size");
        f.bytes = entry["bytes"].get<uint64_t>();
        if (!managed_update_path(f.path) || f.path == "install-inventory.json" ||
            !digest(f.sha256) || !names.insert(lower(f.path)).second ||
            f.bytes > update_package_limit || f.bytes > 1024 * 1024 * 1024ULL - total)
            throw std::runtime_error("Invalid, duplicate or oversized managed file");
        total += f.bytes;
        out.push_back(std::move(f));
    }
    for (const char *required :
         {"CommunityDivers.exe", "CommunityDiversUpdate.exe", "build-info.json"})
        if (!names.contains(lower(required)))
            throw std::runtime_error("Incomplete launcher inventory");
    return out;
}
} // namespace cd
