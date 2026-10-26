#pragma once
#include "community/updates.hpp"
namespace cd {
struct ManagedFile {
    std::string path, sha256;
    uint64_t bytes = 0;
};
bool managed_update_path(const std::string &path);
std::vector<ManagedFile> parse_install_inventory(const std::string &body);
std::vector<ManagedFile> extract_update_bundle(const fs::path &package,
                                               const UpdateRelease &release,
                                               const fs::path &destination);
// A lock handle is retained by the launcher for its entire lifetime and passed to its replacement.
class LauncherLock {
    uintptr_t value_ = 0;

  public:
    explicit LauncherLock(const fs::path &root, uintptr_t inherited = 0);
    ~LauncherLock();
    LauncherLock(const LauncherLock &) = delete;
    uintptr_t handle() const { return value_; }
};
void signal_update_health(uintptr_t handle);
struct InstallHandoff {
    fs::path job;
    std::string error;
};
InstallHandoff start_update_install(const fs::path &package, const std::string &envelope,
                                    const fs::path &root, const fs::path &data,
                                    const std::string &trust_key = update_publisher_key());
std::future<InstallHandoff> start_update_install_async(fs::path package, std::string envelope,
                                                       fs::path root, fs::path data);
InstallHandoff start_update_recovery(const fs::path &root, const fs::path &data);
struct InstallTiming {
    uint32_t parent_exit_ms = 30000, health_ms = 20000, settle_ms = 1500;
    std::function<void(size_t)> after_replace;
};
int run_update_installer(const fs::path &plan, const std::string &trust_key, bool recover = false,
                         const InstallTiming &timing = {});
} // namespace cd
