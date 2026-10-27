#pragma once
#include "community/patch_notes.hpp"
#include "imgui.h"
#include <memory>
namespace cd {
struct NoteTexture {
    ImTextureID id{};
    ImVec2 size{};
    std::shared_ptr<void> owner;
};
struct ReleaseNotesView {
    const ReleaseNotes &notes;
    std::map<std::string, NoteTexture> &textures;
    const std::function<NoteTexture(const std::string &)> &load_image;
    ImFont *body = nullptr, *strong = nullptr, *heading = nullptr;
};
// Call inside a caller-owned ImGui window or child. The caller owns textures and lifetime.
void draw_release_notes(ReleaseNotesView view);
} // namespace cd
