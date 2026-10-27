#pragma once
#include <string>
#include <vector>
namespace cd {
struct Profile {
    std::string id = "default", name = "Default";
    std::vector<std::string> enabled_mods;
};
} // namespace cd
