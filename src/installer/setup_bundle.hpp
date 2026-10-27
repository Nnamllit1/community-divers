#pragma once
#include "community/setup.hpp"
#include <memory>

namespace cd::setup {
struct Bundle {
    UpdateRelease release;
    uint64_t offset = 0;
    bool qa = false, interrupt = false, dirty = false;
    std::string label() const { return qa ? "Community Divers QA" : "Community Divers"; }
    std::string key() const { return qa ? "CommunityDivers-QA" : "CommunityDivers"; }
};
// Only locally built QA packages can contain a dirty source tree or interrupt hook.
Bundle inspect_bundle(const fs::path &executable);
void pack_bundle(const fs::path &stub, const fs::path &zip, const fs::path &output, bool qa,
                 bool interrupt, bool allow_dirty);
struct StagedBundle {
    fs::path work, payload;
    Bundle bundle;
    ~StagedBundle();
};
std::unique_ptr<StagedBundle> stage_bundle(const fs::path &executable);
fs::path create_setup_workspace();
} // namespace cd::setup
