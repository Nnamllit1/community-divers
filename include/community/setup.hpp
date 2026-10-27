#pragma once
#include "community/installer.hpp"
namespace cd {
void check_setup_destination(const fs::path &root);
void require_setup_complete(const fs::path &root);
void begin_setup_install(const fs::path &root, const std::string &registry_key);
void verify_setup_files(const fs::path &root);
void register_setup_install(const fs::path &root, const std::string &registry_key);
struct UninstallReport {
    size_t deleted = 0, preserved = 0;
};
UninstallReport uninstall_managed_launcher(const fs::path &root);
void refresh_setup_registration(const fs::path &root, const std::string &release) noexcept;
} // namespace cd
