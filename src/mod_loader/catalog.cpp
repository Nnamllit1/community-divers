#include "community/mod_loader.hpp"
#include "community/core.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
namespace cd {
namespace {
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
} // namespace
std::vector<Mod> discover_mods(const fs::path &root) {
    std::vector<Mod> result;
    std::error_code ec;
    if (!fs::exists(root, ec))
        return result;
    for (const auto &entry : fs::directory_iterator(root)) {
        if (!entry.is_directory())
            continue;
        Mod m;
        m.directory = entry.path();
        m.name = to_utf8(entry.path().filename());
        try {
            auto j = json::parse(read_text(entry.path() / "mod.json", 65536));
            if (j.at("schema") != 1)
                throw std::runtime_error("Unsupported manifest schema");
            m.id = checked_string(j, "id");
            m.name = checked_string(j, "name", 128);
            m.version = checked_string(j, "version", 64);
            m.description = j.value("description", "");
            if (!identifier(m.id) || m.name.empty() || m.version.empty() ||
                m.description.size() > 4096)
                throw std::runtime_error("Invalid mod metadata");
        } catch (const std::exception &e) {
            m.error = e.what();
        }
        result.push_back(std::move(m));
    }
    for (auto &m : result)
        if (!m.id.empty() && std::count_if(result.begin(), result.end(),
                                           [&](const auto &n) { return m.id == n.id; }) > 1)
            m.error = "Duplicate mod ID; use one copy only";
    std::sort(result.begin(), result.end(),
              [](const auto &a, const auto &b) { return a.name < b.name; });
    return result;
}
} // namespace cd
