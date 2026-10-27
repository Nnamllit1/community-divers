#pragma once
#include "community/core.hpp"
#include <functional>
namespace cd {
struct DepotFile {
    std::string path, sha1;
    uint64_t bytes = 0;
    bool directory = false;
};
struct DepotManifest {
    uint32_t depot = 0;
    uint64_t manifest = 0;
    std::vector<DepotFile> files;
};
// Reads an already decrypted local Steam cache manifest. No downloads or keys.
DepotManifest parse_depot_manifest(std::string_view bytes);
bool client_relative_path(std::string_view path);
struct PreparationPlan {
    fs::path source;
    std::string build;
    json depots = json::array();
    std::vector<DepotFile> files;
    uint64_t bytes = 0;
};
json preparation_report(const PreparationPlan &plan);
#ifdef _WIN32
PreparationPlan plan_client_copy(const fs::path &source, const fs::path &depot_cache);
using PreparationProgress = std::function<void(size_t, size_t, const std::string &)>;
void prepare_client_copy(const fs::path &source, const fs::path &depot_cache,
                         const fs::path &destination, PreparationProgress progress = {});
json verify_client_copy(const fs::path &destination, PreparationProgress progress = {});
#endif
} // namespace cd
