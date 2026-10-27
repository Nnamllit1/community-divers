#include "community/core.hpp"
#include "community/version.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <charconv>
#include <chrono>
#include <fstream>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
namespace cd {
const KV *KV::get(const std::string &key) const {
    auto i = children.find(key);
    return i == children.end() ? nullptr : &i->second;
}
std::string KV::text(const std::string &key) const {
    auto p = get(key);
    return p ? p->value : "";
}
namespace {
struct VdfParser {
    const std::string &input;
    size_t pos = 0, nodes = 0;
    void space() {
        while (pos < input.size()) {
            if (std::isspace(static_cast<unsigned char>(input[pos]))) {
                ++pos;
                continue;
            }
            if (input.compare(pos, 2, "//") == 0) {
                while (pos < input.size() && input[pos] != '\n')
                    ++pos;
                continue;
            }
            break;
        }
    }
    std::string token() {
        space();
        if (pos == input.size())
            throw std::runtime_error("Incomplete KeyValues file");
        std::string result;
        if (input[pos] == '"') {
            ++pos;
            while (pos < input.size()) {
                char c = input[pos++];
                if (c == '"')
                    return result;
                if (c == '\\' && pos < input.size() && (input[pos] == '\\' || input[pos] == '"'))
                    c = input[pos++];
                result += c;
            }
            throw std::runtime_error("Unterminated KeyValues string");
        }
        while (pos < input.size() && !std::isspace(static_cast<unsigned char>(input[pos])) &&
               input[pos] != '{' && input[pos] != '}')
            result += input[pos++];
        if (result.empty())
            throw std::runtime_error("Expected a KeyValues token");
        return result;
    }
    KV object(unsigned depth = 0) {
        if (depth > 32)
            throw std::runtime_error("KeyValues nesting is too deep");
        KV result;
        while (true) {
            space();
            if (pos == input.size()) {
                if (depth)
                    throw std::runtime_error("Unclosed KeyValues object");
                return result;
            }
            if (input[pos] == '}') {
                if (!depth)
                    throw std::runtime_error("Unexpected closing brace");
                ++pos;
                return result;
            }
            auto key = token();
            space();
            KV value;
            if (pos < input.size() && input[pos] == '{') {
                ++pos;
                value = object(depth + 1);
            } else
                value.value = token();
            if (++nodes > 50000 || !result.children.emplace(key, std::move(value)).second)
                throw std::runtime_error("Invalid or repeated KeyValues key");
        }
    }
};
std::string checked_string(const json &j, const char *key, size_t max = 1024) {
    auto s = j.at(key).get<std::string>();
    if (s.size() > max)
        throw std::runtime_error(std::string(key) + " is too long");
    return s;
}
bool identifier(const std::string &s) {
    return !s.empty() && s.size() <= 96 && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '-' || c == '_' || c == '.';
    });
}
bool valid_ipv6(const std::string &host) {
    auto compressed = host.find("::");
    if (host.find(":::") != std::string::npos)
        return false;
    if (host.empty() || (host.front() == ':' && !host.starts_with("::")) ||
        (host.back() == ':' && !host.ends_with("::")))
        return false;
    if (compressed != std::string::npos && host.find("::", compressed + 2) != std::string::npos)
        return false;
    unsigned groups = 0;
    for (size_t pos = 0; pos < host.size();) {
        if (host[pos] == ':') {
            ++pos;
            continue;
        }
        auto end = host.find(':', pos);
        if (end == std::string::npos)
            end = host.size();
        if (end - pos > 4 || !std::all_of(host.begin() + pos, host.begin() + end,
                                          [](unsigned char c) { return std::isxdigit(c) != 0; }))
            return false;
        ++groups;
        pos = end;
    }
    return compressed == std::string::npos ? groups == 8 : groups < 8;
}
bool is_loopback(const std::string &s) {
    return s == "localhost" || s == "127.0.0.1" || s == "::1";
}
} // namespace
fs::path from_utf8(const std::string &value) {
    return fs::path(std::u8string(reinterpret_cast<const char8_t *>(value.data()), value.size()));
}
std::string to_utf8(const fs::path &value) {
    auto encoded = value.u8string();
    return std::string(reinterpret_cast<const char *>(encoded.data()), encoded.size());
}
KV parse_vdf(const std::string &text) {
    return VdfParser{text}.object();
}
std::string read_text(const fs::path &path, size_t limit) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot read file");
    file.seekg(0, std::ios::end);
    auto n = file.tellg();
    if (n < 0 || static_cast<uint64_t>(n) > limit)
        throw std::runtime_error("File exceeds size limit");
    std::string text(static_cast<size_t>(n), '\0');
    file.seekg(0);
    if (!text.empty() && !file.read(text.data(), static_cast<std::streamsize>(text.size())))
        throw std::runtime_error("Incomplete file read");
    return text;
}
Installation inspect_installation(const fs::path &path) {
    Installation result;
    result.root = path;
    std::error_code ec;
    result.valid = fs::is_regular_file(path / "bin" / "helldivers2.exe", ec) &&
                   fs::is_directory(path / "data", ec);
    result.detail =
        result.valid ? "Game files detected. Community integration is not installed."
                     : "Choose the HELLDIVERS 2 folder containing bin/helldivers2.exe and data/.";
    try {
        auto manifest = path.parent_path().parent_path() / "appmanifest_553850.acf";
        auto doc = parse_vdf(read_text(manifest));
        auto app = doc.get("AppState");
        if (app && app->text("appid") == "553850") {
            auto directory = from_utf8(app->text("installdir"));
            std::error_code identity_error;
            if (!directory.empty() && !directory.is_absolute() && !directory.has_parent_path() &&
                directory != "." && directory != ".." &&
                fs::equivalent(path, path.parent_path() / directory, identity_error) &&
                !identity_error)
                result.build = app->text("buildid");
        }
    } catch (const std::exception &) {
    }
    return result;
}
std::vector<Installation> discover_steam(const std::vector<fs::path> &steam_roots) {
    std::vector<fs::path> libraries = steam_roots;
    for (const auto &steam : steam_roots) {
        try {
            auto doc = parse_vdf(read_text(steam / "steamapps" / "libraryfolders.vdf"));
            auto lib = doc.get("libraryfolders");
            if (lib)
                for (const auto &[key, value] : lib->children) {
                    if (!key.empty() && std::all_of(key.begin(), key.end(), [](unsigned char c) {
                            return std::isdigit(c);
                        })) {
                        auto path = value.children.empty() ? value.value : value.text("path");
                        if (!path.empty())
                            libraries.emplace_back(from_utf8(path));
                    }
                }
        } catch (const std::exception &) {
        }
    }
    std::vector<Installation> result;
    std::set<fs::path> seen;
    for (const auto &library : libraries) {
        try {
            auto doc = parse_vdf(read_text(library / "steamapps" / "appmanifest_553850.acf"));
            auto app = doc.get("AppState");
            if (!app || app->text("appid") != "553850")
                continue;
            auto name = from_utf8(app->text("installdir"));
            if (name.empty() || name.is_absolute() || name.has_parent_path() || name == ".." ||
                name == ".")
                continue;
            auto path = fs::weakly_canonical(library / "steamapps" / "common" / name);
            if (!seen.insert(path).second)
                continue;
            auto installation = inspect_installation(path);
            installation.build = app->text("buildid");
            result.push_back(std::move(installation));
        } catch (const std::exception &) {
        }
    }
    return result;
}
Endpoint parse_endpoint(std::string s) {
    if (s.empty() || s.size() > 2048 || std::any_of(s.begin(), s.end(), [](unsigned char c) {
            return c <= 32 || c >= 127 || c == '\\';
        }))
        throw std::runtime_error("Use an ASCII HTTPS URL without spaces");
    auto separator = s.find("://");
    if (separator == std::string::npos)
        throw std::runtime_error("Include https:// in the server URL");
    auto scheme = s.substr(0, separator);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (scheme != "https" && scheme != "http")
        throw std::runtime_error("Only HTTPS and loopback HTTP are supported");
    auto rest = s.substr(separator + 3);
    auto slash = rest.find('/');
    auto authority = rest.substr(0, slash);
    Endpoint result;
    result.secure = scheme == "https";
    result.port = result.secure ? 443 : 80;
    result.path = slash == std::string::npos ? "" : rest.substr(slash);
    if (authority.empty() || s.find_first_of("@?#%") != std::string::npos)
        throw std::runtime_error("Remove credentials, query parameters and fragments from the URL");
    std::string port;
    if (authority.front() == '[') {
        auto end = authority.find(']');
        if (end == std::string::npos)
            throw std::runtime_error("Invalid IPv6 host");
        result.host = authority.substr(1, end - 1);
        if (end + 1 < authority.size()) {
            if (authority[end + 1] != ':')
                throw std::runtime_error("Invalid host suffix");
            port = authority.substr(end + 2);
            if (port.empty())
                throw std::runtime_error("Missing port number");
        }
        if (result.host.empty() || result.host.find(':') == std::string::npos ||
            result.host.find_first_not_of("0123456789abcdefABCDEF:") != std::string::npos)
            throw std::runtime_error("Invalid IPv6 host");
        if (!valid_ipv6(result.host))
            throw std::runtime_error("Invalid IPv6 host");
    } else {
        auto colon = authority.find(':');
        result.host = authority.substr(0, colon);
        if (colon != std::string::npos) {
            port = authority.substr(colon + 1);
            if (port.empty())
                throw std::runtime_error("Missing port number");
        }
        if (result.host.empty() ||
            result.host.find_first_not_of(
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-") !=
                std::string::npos)
            throw std::runtime_error("Invalid hostname");
    }
    if (result.host.find(':') == std::string::npos) {
        for (size_t pos = 0; pos < result.host.size();) {
            auto end = result.host.find('.', pos);
            if (end == std::string::npos)
                end = result.host.size();
            if (end == pos || end - pos > 63 ||
                !std::isalnum(static_cast<unsigned char>(result.host[pos])) ||
                !std::isalnum(static_cast<unsigned char>(result.host[end - 1])))
                throw std::runtime_error("Invalid hostname label");
            pos = end + 1;
        }
        if (result.host.back() == '.')
            throw std::runtime_error("Remove the trailing hostname dot");
    }
    std::transform(result.host.begin(), result.host.end(), result.host.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!port.empty()) {
        unsigned value = 0;
        auto [end, error] = std::from_chars(port.data(), port.data() + port.size(), value);
        if (error != std::errc{} || end != port.data() + port.size() || !value || value > 65535)
            throw std::runtime_error("Port must be between 1 and 65535");
        result.port = static_cast<unsigned short>(value);
    }
    if (!result.secure && !is_loopback(result.host))
        throw std::runtime_error("HTTP is allowed only for localhost, 127.0.0.1 or [::1]");
    while (!result.path.empty() && result.path.back() == '/')
        result.path.pop_back();
    if (result.path.find("..") != std::string::npos)
        throw std::runtime_error("Remove parent-directory segments from the URL");
    auto host = result.host.find(':') != std::string::npos ? "[" + result.host + "]" : result.host;
    result.canonical =
        scheme + "://" + host +
        ((result.secure && result.port == 443) || (!result.secure && result.port == 80)
             ? ""
             : ":" + std::to_string(result.port)) +
        result.path;
    return result;
}
json encode_settings(const Settings &s) {
    json j = {{"schema", 1},
              {"check_updates", s.check_updates},
              {"game_path", s.game_path},
              {"selected_community", s.selected_community},
              {"selected_profile", s.selected_profile},
              {"communities", json::array()},
              {"profiles", json::array()}};
    for (const auto &c : s.communities)
        j["communities"].push_back({{"id", c.id}, {"name", c.name}, {"url", c.url}});
    for (const auto &p : s.profiles)
        j["profiles"].push_back({{"id", p.id}, {"name", p.name}, {"enabled_mods", p.enabled_mods}});
    return j;
}
Settings decode_settings(const json &j) {
    if (j.at("schema") != 1)
        throw std::runtime_error("Unsupported settings schema");
    Settings s;
    s.check_updates = j.value("check_updates", true);
    s.game_path = checked_string(j, "game_path", 4096);
    s.selected_community = checked_string(j, "selected_community");
    s.selected_profile = checked_string(j, "selected_profile");
    if (!j.at("communities").is_array() || j.at("communities").size() > 100 ||
        !j.at("profiles").is_array() || j.at("profiles").size() > 32)
        throw std::runtime_error("Invalid settings collections");
    std::set<std::string> ids;
    for (const auto &c : j.at("communities")) {
        Community item{checked_string(c, "id"), checked_string(c, "name", 128),
                       parse_endpoint(checked_string(c, "url", 2048)).canonical};
        if (!identifier(item.id) || item.name.empty() || !ids.insert(item.id).second)
            throw std::runtime_error("Invalid or duplicate community");
        s.communities.push_back(std::move(item));
    }
    s.profiles.clear();
    ids.clear();
    for (const auto &p : j.at("profiles")) {
        Profile item;
        item.id = checked_string(p, "id");
        item.name = checked_string(p, "name", 128);
        item.enabled_mods = p.at("enabled_mods").get<std::vector<std::string>>();
        if (!identifier(item.id) || item.name.empty() || !ids.insert(item.id).second ||
            item.enabled_mods.size() > 1000)
            throw std::runtime_error("Invalid profile");
        std::set<std::string> mods;
        for (const auto &id : item.enabled_mods)
            if (!identifier(id) || !mods.insert(id).second)
                throw std::runtime_error("Invalid or repeated mod identifier");
        s.profiles.push_back(std::move(item));
    }
    if (s.profiles.empty() || !ids.contains(s.selected_profile))
        throw std::runtime_error("The selected mod profile is missing");
    if (!s.selected_community.empty() &&
        std::none_of(s.communities.begin(), s.communities.end(),
                     [&](const auto &c) { return c.id == s.selected_community; }))
        throw std::runtime_error("The selected community is missing");
    return s;
}
SettingsLoad load_settings(const fs::path &path) {
    SettingsLoad result;
    try {
        if (fs::exists(path))
            result.value = decode_settings(json::parse(read_text(path)));
    } catch (const std::exception &e) {
        result.error =
            std::string("Settings could not be loaded. The original file is preserved. ") +
            e.what();
    }
    return result;
}
void save_settings(const fs::path &path, const Settings &s) {
    auto encoded = encode_settings(s);
    (void)decode_settings(encoded);
    fs::create_directories(path.parent_path());
    static std::atomic_uint serial{0};
    auto temp = path;
    temp += ".pending-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
            std::to_string(serial++);
    try {
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            file.exceptions(std::ios::failbit | std::ios::badbit);
            file << encoded.dump(2) << '\n';
            file.flush();
        }
        if (fs::exists(path)) {
            auto old = path;
            old += load_settings(path).error.empty() ? ".previous" : ".invalid";
            if (old.extension() == ".invalid" && fs::exists(old))
                throw std::runtime_error(
                    "An earlier invalid-settings backup exists; move it before saving again");
            fs::copy_file(path, old, fs::copy_options::overwrite_existing);
        }
#ifdef _WIN32
        if (!MoveFileExW(temp.c_str(), path.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not replace settings file");
#else
        fs::rename(temp, path);
#endif
    } catch (...) {
        std::error_code ec;
        fs::remove(temp, ec);
        throw;
    }
}
ServiceStatus parse_status(const std::string &body) {
    if (body.size() > 262144)
        throw std::runtime_error("Service response exceeds 256 KiB");
    auto j = json::parse(body);
    if (j.at("protocol") != 1)
        throw std::runtime_error("Unsupported community service protocol");
    ServiceStatus s;
    s.id = checked_string(j, "id", 96);
    s.name = checked_string(j, "name", 128);
    if (!identifier(s.id) || s.name.empty())
        throw std::runtime_error("Invalid service identity");
    s.game_builds = j.at("game_builds").get<std::vector<std::string>>();
    if (s.game_builds.size() > 100)
        throw std::runtime_error("Too many service build entries");
    for (const auto &build : s.game_builds)
        if (build.empty() || build.size() > 32 ||
            build.find_first_not_of("0123456789") != std::string::npos)
            throw std::runtime_error("Invalid game build ID");
    auto release_identifier = [&](const char *key) {
        if (!j.contains(key))
            return std::string{};
        auto value = checked_string(j, key, 128);
        if (value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                       c == '.' || c == '-' || c == '_' || c == '+';
            }))
            throw std::runtime_error("Invalid community release requirement");
        return value;
    };
    s.required_release_set = release_identifier("required_release_set");
    s.required_launcher_release = release_identifier("required_launcher_release");
    if (j.contains("capabilities")) {
        s.capabilities = j.at("capabilities").get<std::vector<std::string>>();
        if (s.capabilities.size() > 32)
            throw std::runtime_error("Too many service capabilities");
        for (const auto &capability : s.capabilities)
            if (!identifier(capability) || capability.size() > 64)
                throw std::runtime_error("Invalid service capability");
    }
    if (j.contains("campaign"))
        s.campaign = checked_string(j.at("campaign"), "name", 128);
    return s;
}
CampaignSnapshot parse_campaign(const std::string &body, const std::string &community_id) {
    if (body.size() > 1024 * 1024)
        throw std::runtime_error("Campaign snapshot exceeds 1 MiB");
    auto j = json::parse(body);
    if (j.at("schema") != 1)
        throw std::runtime_error("Unsupported campaign snapshot schema");
    CampaignSnapshot c;
    c.community_id = checked_string(j, "community_id", 96);
    c.name = checked_string(j, "name", 128);
    if (c.community_id != community_id || c.name.empty())
        throw std::runtime_error("Campaign belongs to a different community");
    auto number = [](const json &item, const char *key, uint64_t maximum) {
        if (!item.at(key).is_number_unsigned())
            throw std::runtime_error("Invalid campaign integer");
        auto n = item.at(key).get<uint64_t>();
        if (n > maximum)
            throw std::runtime_error("Campaign integer is out of range");
        return n;
    };
    c.revision = number(j, "revision", UINT64_MAX);
    c.epoch = number(j, "epoch", UINT64_MAX);
    if (!c.epoch || !j.at("planets").is_array() || !j.at("templates").is_array() ||
        j["planets"].size() > 1024 || j["templates"].size() > 1024)
        throw std::runtime_error("Invalid campaign collection size");
    std::set<std::string> ids;
    for (const auto &item : j["planets"]) {
        CampaignPlanet p;
        p.id = checked_string(item, "id", 96);
        p.name = checked_string(item, "name", 128);
        p.biome_id = checked_string(item, "biome_id", 96);
        p.owner_faction = checked_string(item, "owner_faction", 96);
        p.liberation = static_cast<uint32_t>(number(item, "liberation", 1000000));
        if (!identifier(p.id) || !identifier(p.biome_id) || !identifier(p.owner_faction) ||
            p.name.empty() || !ids.insert(p.id).second)
            throw std::runtime_error("Invalid or duplicate campaign planet");
        c.planets.push_back(std::move(p));
    }
    ids.clear();
    for (const auto &item : j["templates"]) {
        CampaignTemplate t;
        t.id = checked_string(item, "id", 96);
        t.name = checked_string(item, "name", 128);
        t.reward_xp = static_cast<uint32_t>(number(item, "reward_xp", 1000000));
        t.war_points = static_cast<uint32_t>(number(item, "war_points", 1000000));
        if (!identifier(t.id) || t.name.empty() || !ids.insert(t.id).second)
            throw std::runtime_error("Invalid or duplicate mission template");
        c.templates.push_back(std::move(t));
    }
    return c;
}
std::string launch_blocker(const Installation &game, const ServiceStatus *service) {
    if (!game.valid)
        return "Choose a valid game installation to begin setup.";
    if (!service)
        return "Select a community and check its connection.";
    if (game.build.empty())
        return "The game build could not be identified.";
    if (std::find(service->game_builds.begin(), service->game_builds.end(), game.build) ==
        service->game_builds.end())
        return "This community does not advertise your installed game build.";
    if (!service->required_launcher_release.empty() &&
        service->required_launcher_release != version::release_id)
        return "This community requires launcher release " + service->required_launcher_release +
               ". Update the launcher before joining.";
    return "Community game launch is not available yet. The isolated client adapter is still in "
           "development.";
}
} // namespace cd
