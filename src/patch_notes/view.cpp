#include "community/patch_notes_view.hpp"
#include "imgui_markdown.h"
namespace cd {
void draw_release_notes(ReleaseNotesView view) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {8, 5});
    if (view.body)
        ImGui::PushFont(view.body, 18);
    ImGui::MarkdownConfig config;
    config.formatFlags = ImGuiMarkdownFormatFlags_GithubStyle;
    config.userData = &view;
    config.headingFormats[0] = {view.heading, true, 28};
    config.headingFormats[1] = {view.strong ? view.strong : view.heading, true, 22};
    config.headingFormats[2] = {view.strong ? view.strong : view.body, false, 18};
    config.imageCallback = [](ImGui::MarkdownLinkCallbackData data) {
        ImGui::MarkdownImageData image;
        auto &view = *static_cast<ReleaseNotesView *>(data.userData);
        std::string path(data.link, data.linkLength);
        if (!release_note_image_path(path))
            return image;
        auto bytes = view.notes.images.find(path);
        if (bytes == view.notes.images.end() || !view.load_image)
            return image;
        auto it = view.textures.find(path);
        if (it == view.textures.end()) {
            try {
                it = view.textures.emplace(path, view.load_image(bytes->second)).first;
            } catch (const std::exception &) {
                it = view.textures.emplace(path, NoteTexture{}).first;
            }
        }
        if (!it->second.id)
            return image;
        image.isValid = true;
        image.user_texture_id = it->second.id;
        image.size = it->second.size;
        float width = ImGui::GetContentRegionAvail().x;
        if (image.size.x > width && width > 0) {
            image.size.y *= width / image.size.x;
            image.size.x = width;
        }
        return image;
    };
    ImGui::Markdown(view.notes.markdown.data(), view.notes.markdown.size(), config);
    if (view.body)
        ImGui::PopFont();
    ImGui::PopStyleVar();
}
} // namespace cd
