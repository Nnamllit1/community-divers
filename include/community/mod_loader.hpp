#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace cd {
struct Mod {
    std::string id, name, version, description, error;
    std::filesystem::path directory;
};
std::vector<Mod> discover_mods(const std::filesystem::path &directory);
} // namespace cd
