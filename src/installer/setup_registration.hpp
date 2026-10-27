#pragma once
#include "community/setup.hpp"
namespace cd::setup {
fs::path registered_path(const std::string &key);
void check_registration(const fs::path &root, const std::string &key);
void register_application(const fs::path &root, const std::string &key, const std::string &label);
void unregister_application(const fs::path &root, const std::string &key, const std::string &label);
} // namespace cd::setup
