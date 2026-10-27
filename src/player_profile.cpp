#include "community/core.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
namespace cd {
namespace {
bool hex(const std::string &text, size_t count) {
    return text.size() == count && text.find_first_not_of("0123456789abcdef") == text.npos;
}
bool identifier(const std::string &id) {
    return !id.empty() && id.size() <= 96 && std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '-' || c == '_' || c == '.';
    });
}
} // namespace
PlayerCredential parse_player_credential(const std::string &body, const std::string &community_id) {
    // Do not let JSON diagnostics include fragments of a credential in UI/logs.
    try {
        if (body.size() > 16384 || !identifier(community_id))
            throw std::runtime_error("invalid");
        std::set<std::string> fields;
        auto j = json::parse(body, [&](int depth, json::parse_event_t event, json &value) {
            if (event == json::parse_event_t::array_start ||
                (event == json::parse_event_t::object_start && depth > 0))
                throw std::runtime_error("nested credential");
            if (depth == 1 && event == json::parse_event_t::key &&
                !fields.insert(value.get<std::string>()).second)
                throw std::runtime_error("duplicate");
            return true;
        });
        if (!j.is_object() || j.at("schema") != 1 || j.size() < 4 || j.size() > 5)
            throw std::runtime_error("invalid");
        for (const auto &[key, value] : j.items()) {
            (void)value;
            if (key != "schema" && key != "community_id" && key != "player_id" && key != "token" &&
                key != "name")
                throw std::runtime_error("invalid");
        }
        PlayerCredential c{j.at("community_id").get<std::string>(),
                           j.at("player_id").get<std::string>(), j.at("token").get<std::string>()};
        if (c.community_id != community_id || !hex(c.player_id, 32) || c.token.size() != 68 ||
            !c.token.starts_with("cdp_") || !hex(c.token.substr(4), 64))
            throw std::runtime_error("invalid");
        if (j.contains("name") &&
            (!j["name"].is_string() || j["name"].get<std::string>().size() > 128))
            throw std::runtime_error("invalid");
        return c;
    } catch (const std::exception &) {
        throw std::runtime_error(
            "The player credential is invalid or belongs to another community.");
    }
}
json encode_player_credential(const PlayerCredential &credential) {
    json out{{"schema", 1},
             {"community_id", credential.community_id},
             {"player_id", credential.player_id},
             {"token", credential.token}};
    parse_player_credential(out.dump(), credential.community_id);
    return out;
}
PlayerProfile parse_player_profile(const std::string &body, const std::string &player_id) {
    try {
        if (body.size() > 32768 || !hex(player_id, 32))
            throw std::runtime_error("invalid");
        auto j = json::parse(body, [](int depth, json::parse_event_t, json &) {
            if (depth > 40)
                throw std::runtime_error("nested profile");
            return true;
        });
        PlayerProfile p;
        p.id = j.at("id").get<std::string>();
        p.name = j.at("name").get<std::string>();
        if (p.id != player_id || p.name.empty() || p.name.size() > 128 ||
            std::any_of(p.name.begin(), p.name.end(),
                        [](unsigned char c) { return c < 32 || c == 127; }))
            throw std::runtime_error("invalid");
        for (const char *field : {"xp", "missions_completed"}) {
            if (!j.at(field).is_number_unsigned() ||
                j.at(field).get<uint64_t>() > uint64_t(INT64_MAX))
                throw std::runtime_error("invalid");
        }
        p.xp = j["xp"].get<uint64_t>();
        p.missions_completed = j["missions_completed"].get<uint64_t>();
        p.loadout = j.at("loadout");
        if (!p.loadout.is_object() || p.loadout.size() > 32 || p.loadout.dump().size() > 8192)
            throw std::runtime_error("invalid");
        return p;
    } catch (const std::exception &) {
        throw std::runtime_error(
            "The service returned an invalid profile or a different player identity.");
    }
}
} // namespace cd
