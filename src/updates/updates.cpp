#include "community/updates.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
namespace cd {
namespace {
bool hex(const std::string &s, size_t size) {
    return s.size() == size && std::all_of(s.begin(), s.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
uint64_t positive(const json &j, const char *key, uint64_t max) {
    if (!j.contains(key) || !j[key].is_number_unsigned())
        throw std::runtime_error(std::string("Missing update integer: ") + key);
    auto value = j[key].get<uint64_t>();
    if (!value || value > max)
        throw std::runtime_error(std::string("Invalid update integer: ") + key);
    return value;
}
std::string token(const json &j, const char *key, size_t max) {
    auto s = j.at(key).get<std::string>();
    if (s.empty() || s.size() > max || !std::all_of(s.begin(), s.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '-' || c == '.' || c == '+' || c == '_';
        }))
        throw std::runtime_error(std::string("Invalid update identifier: ") + key);
    return s;
}
} // namespace
UpdateRelease parse_update_payload(const std::string &payload, const std::string &channel) {
    if (payload.size() > update_manifest_limit)
        throw std::runtime_error("Update manifest is too large");
    auto j = json::parse(payload);
    if (!j.is_object() || j.at("schema") != 1 || j.at("platform") != "windows-x64")
        throw std::runtime_error("Unsupported update manifest or platform");
    UpdateRelease r;
    r.channel = j.at("channel").get<std::string>();
    if (channel != "preview" || r.channel != channel)
        throw std::runtime_error("Update channel mismatch");
    r.release_id = token(j, "release_id", 128);
    r.source_commit = j.at("source_commit").get<std::string>();
    r.sha256 = j.at("sha256").get<std::string>();
    if (!hex(r.source_commit, 40) || !hex(r.sha256, 64))
        throw std::runtime_error("Invalid update source identity or SHA-256 digest");
    r.revision = positive(j, "source_revision", UINT64_MAX);
    r.bytes = positive(j, "bytes", update_package_limit);
    r.service_protocol = static_cast<int>(positive(j, "service_protocol", 65535));
    r.filename = token(j, "filename", 180);
    if (!r.filename.starts_with("CommunityDivers-") || !r.filename.ends_with("-windows-x64.zip"))
        throw std::runtime_error("Unexpected update package filename");
    r.url = j.at("url").get<std::string>();
    const std::string prefix = "https://github.com/Nnamllit1/community-divers/releases/download/";
    if (!r.url.starts_with(prefix))
        throw std::runtime_error("Update package is outside the project releases");
    auto tail = r.url.substr(prefix.size());
    auto slash = tail.find('/');
    if (slash == std::string::npos || tail.substr(slash + 1) != r.filename)
        throw std::runtime_error("Update URL does not identify its package");
    auto tag = tail.substr(0, slash);
    if (tag.empty() || tag.size() > 128 ||
        !std::all_of(
            tag.begin(), tag.end(),
            [](unsigned char c) { return std::isalnum(c) || c == '-' || c == '.' || c == '_'; }) ||
        tag == "." || tag == "..")
        throw std::runtime_error("Invalid update release tag");
    return r;
}
UpdateRelation compare_update(const UpdateRelease &r, std::string_view commit, uint64_t revision) {
    if (r.source_commit == commit)
        return UpdateRelation::current;
    if (!revision || r.revision == revision)
        return UpdateRelation::unknown;
    return r.revision > revision ? UpdateRelation::available : UpdateRelation::local_newer;
}

} // namespace cd
