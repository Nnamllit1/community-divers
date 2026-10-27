#pragma once
#include "community/installer.hpp"
namespace cd {
void check_setup_destination(const fs::path &root);
void require_setup_complete(const fs::path &root);
void begin_setup_install(const fs::path &root, const std::string &registry_key);
void verify_setup_files(const fs::path &root);
void register_setup_install(const fs::path &root, const std::string &registry_key);
using SetupProgress = std::function<void(size_t, size_t, const std::string &)>;
void install_setup_payload(const fs::path &payload, const fs::path &root,
                           const fs::path &uninstaller, const std::string &registry_key,
                           const SetupProgress &progress = {}, bool interrupt_fixture = false);
std::string setup_registration_key(const fs::path &root);
struct UninstallReport {
    size_t deleted = 0, preserved = 0;
};
UninstallReport uninstall_managed_launcher(const fs::path &root);
void refresh_setup_registration(const fs::path &root, const std::string &release) noexcept;
} // namespace cd
