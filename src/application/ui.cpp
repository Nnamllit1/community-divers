#include "ui.hpp"
#include "community/version.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
namespace cd {
namespace {
const ImVec4 accent{1.f, .88f, .25f, 1}, muted{.64f, .69f, .74f, 1};
void text(const std::string &s) {
    ImGui::TextUnformatted(s.c_str());
}
void wrap(const std::string &s) {
    ImGui::PushTextWrapPos(0);
    text(s);
    ImGui::PopTextWrapPos();
}
void subdued(const std::string &s) {
    ImGui::PushStyleColor(ImGuiCol_Text, muted);
    wrap(s);
    ImGui::PopStyleColor();
}
void section_head(const char *s) {
    std::string label(s);
    std::transform(label.begin(), label.end(), label.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{.94f, .96f, .98f, 1});
    ImGui::SeparatorText(label.c_str());
    ImGui::PopStyleColor();
}
void gap(float n = 8) {
    ImGui::Dummy({0, n});
}
void buffer(char *out, size_t size, const std::string &s) {
    std::snprintf(out, size, "%s", s.c_str());
}
std::string id(const char *prefix) {
    static unsigned serial = 0;
    return std::string(prefix) +
           std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "-" +
           std::to_string(serial++);
}
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
} // namespace
void theme() {
    auto &s = ImGui::GetStyle();
    s.WindowRounding = s.ChildRounding = s.FrameRounding = s.PopupRounding = s.GrabRounding = 0;
    s.ScrollbarRounding = 0;
    s.WindowBorderSize = 0;
    s.ChildBorderSize = 0;
    s.FrameBorderSize = 1;
    s.WindowPadding = {30, 24};
    s.FramePadding = {14, 10};
    s.ItemSpacing = {12, 12};
    s.CellPadding = {12, 12};
    s.ScrollbarSize = 8;
    auto c = s.Colors;
    c[ImGuiCol_Text] = {.94f, .96f, .98f, 1};
    c[ImGuiCol_TextDisabled] = muted;
    c[ImGuiCol_WindowBg] = {.045f, .060f, .080f, 1};
    c[ImGuiCol_ChildBg] = {.025f, .035f, .050f, .72f};
    c[ImGuiCol_PopupBg] = {.055f, .075f, .10f, 1};
    c[ImGuiCol_Border] = {.32f, .36f, .40f, .7f};
    c[ImGuiCol_Separator] = {.30f, .34f, .39f, .65f};
    c[ImGuiCol_FrameBg] = {.015f, .025f, .04f, .9f};
    c[ImGuiCol_FrameBgHovered] = {.10f, .14f, .18f, 1};
    c[ImGuiCol_FrameBgActive] = {.18f, .19f, .12f, 1};
    c[ImGuiCol_Button] = {.045f, .065f, .09f, .95f};
    c[ImGuiCol_ButtonHovered] = {.21f, .22f, .13f, 1};
    c[ImGuiCol_ButtonActive] = {.37f, .34f, .12f, 1};
    c[ImGuiCol_Header] = {.32f, .30f, .11f, 1};
    c[ImGuiCol_HeaderHovered] = {.19f, .22f, .25f, 1};
    c[ImGuiCol_HeaderActive] = {.37f, .34f, .12f, 1};
    c[ImGuiCol_TableHeaderBg] = {.035f, .05f, .07f, .9f};
    c[ImGuiCol_TableBorderStrong] = c[ImGuiCol_Separator];
    c[ImGuiCol_TableBorderLight] = {.12f, .16f, .20f, .6f};
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_SliderGrab] = accent;
}
App::App(fs::path data_path, bool allow_background_updates)
    : data(std::move(data_path)), background_updates(allow_background_updates) {
    portable_install = fs::exists(executable_directory() / "install-inventory.json");
    auto loaded = load_settings(data / "launcher.json");
    settings = std::move(loaded.value);
    settings_warning = loaded.error;
    fs::create_directories(data / "mods");
    scan();
    if (background_updates && settings.check_updates)
        check_for_updates();
}
void App::check_for_updates() {
    if (pending_update.valid())
        return;
    update_message = "Checking the signed preview channel...";
    update_error = false;
    next_update_check = std::chrono::steady_clock::now() + std::chrono::hours(1);
    pending_update = update_checker();
}
void App::scan() {
    if (identity_for_path != settings.game_path)
        identity_summary.clear();
    try {
        if (settings.game_path.empty()) {
            auto found = discover_steam(steam_roots());
            auto i =
                std::find_if(found.begin(), found.end(), [](const auto &g) { return g.valid; });
            if (i != found.end())
                settings.game_path = path_text(i->root);
        }
        game = inspect_installation(from_utf8(settings.game_path));
        buffer(game_path, sizeof(game_path), settings.game_path);
        mods = discover_mods(data / "mods");
    } catch (const std::exception &e) {
        notice = e.what();
        notice_error = true;
    }
}
void App::save() {
    try {
        save_settings(data / "launcher.json", settings);
        settings_warning.clear();
        notice = "Changes saved.";
        notice_error = false;
    } catch (const std::exception &e) {
        notice = e.what();
        notice_error = true;
    }
}
Community *App::community() {
    auto i = std::find_if(settings.communities.begin(), settings.communities.end(),
                          [&](const auto &c) { return c.id == settings.selected_community; });
    return i == settings.communities.end() ? nullptr : &*i;
}
Profile &App::profile() {
    auto i = std::find_if(settings.profiles.begin(), settings.profiles.end(),
                          [&](const auto &p) { return p.id == settings.selected_profile; });
    return i == settings.profiles.end() ? settings.profiles.front() : *i;
}
void App::sync_player_context() {
    auto selected = community();
    auto key = selected ? selected->id : "";
    auto url = selected ? selected->url : "";
    auto server = selected && service && connected_id == key && connected_url == url &&
                          std::find(service->capabilities.begin(), service->capabilities.end(),
                                    "profiles") != service->capabilities.end()
                      ? service->id
                      : "";
    if (key == player_context && url == player_context_url && server == player_context_server)
        return;
    ++player_generation;
    player.reset();
    player_credential.reset();
    player_remembered = false;
    player_options_open = true;
    player_context = key;
    player_context_url = url;
    player_context_server = server;
    player_error = false;
    player_message = server.empty() ? "Check a community connection to sign in."
                                    : "Import a player credential issued by this community.";
    player_restore_requested = !server.empty();
    if (player_restore_requested && !pending_player.valid())
        use_saved_player();
}
void App::begin_player_request(PlayerCredential credential, bool remember, bool forget) {
    if (pending_player.valid() || player_context_server.empty())
        return;
    if (credential.community_id != player_context_server)
        throw std::runtime_error("This player credential belongs to another community.");
    pending_player_generation = player_generation;
    pending_player_remember = remember;
    pending_player_forget = forget;
    try {
        pending_player = player_reader(player_context_url, std::move(credential));
        player_error = false;
        player_message = "Checking your community profile...";
    } catch (const std::exception &e) {
        player_message = e.what();
        player_error = true;
    }
}
void App::use_saved_player() {
    player_restore_requested = false;
    if (pending_player.valid() || player_context_server.empty())
        return;
    try {
        auto credential = player_loader(data, player_context_url, player_context_server);
        if (!credential) {
            player_message =
                "No saved sign-in. Import a credential from this community's operator.";
            return;
        }
        player_remembered = true;
        begin_player_request(std::move(*credential));
    } catch (const std::exception &e) {
        player_message = e.what();
        player_error = true;
    }
}
void App::sign_out_player() {
    player_restore_requested = false;
    player_options_open = true;
    try {
        if (!player_context_server.empty())
            player_forgetter(data, player_context_url, player_context_server);
        ++player_generation;
        player.reset();
        player_credential.reset();
        player_remembered = false;
        player_message = "Signed out. The saved credential for this community was forgotten.";
        player_error = false;
    } catch (const std::exception &e) {
        // Still invalidate in-flight sign-in and clear local authenticated state.
        ++player_generation;
        player.reset();
        player_credential.reset();
        player_message = e.what();
        player_error = true;
    }
}
void App::draw_player(void *owner) {
    section_head("Community profile");
    bool available = !player_context_server.empty() && service &&
                     std::find(service->capabilities.begin(), service->capabilities.end(),
                               "profiles") != service->capabilities.end();
    if (player) {
        ImGui::PushFont(heading);
        wrap(player->name);
        ImGui::PopFont();
        text(std::to_string(player->xp) + " XP / " + std::to_string(player->missions_completed) +
             (player->missions_completed == 1 ? " mission completed" : " missions completed"));
        text("Saved loadout: " + std::to_string(player->loadout.size()) + " entries");
        subdued(player_remembered ? "Sign-in remembered on this Windows account."
                                  : "Signed in for this launcher session.");
        gap(10);
    }
    if (player_error || !player) {
        if (player_error)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1, .58f, .48f, 1});
        wrap(player_message);
        if (player_error)
            ImGui::PopStyleColor();
        gap(8);
    }
    if (player_credential) {
        ImGui::BeginDisabled(!available || pending_player.valid());
        if (ImGui::Button("Refresh profile"))
            begin_player_request(*player_credential);
        ImGui::EndDisabled();
        if (ImGui::GetContentRegionAvail().x >= 290)
            ImGui::SameLine();
    }
    if (player || pending_player.valid() || player_remembered) {
        ImGui::BeginDisabled(!available);
        if (ImGui::Button("Sign out and forget"))
            sign_out_player();
        ImGui::EndDisabled();
        gap(10);
    }
    ImGui::SetNextItemOpen(player_options_open, ImGuiCond_Always);
    player_options_open = ImGui::CollapsingHeader("Sign-in options");
    if (player_options_open) {
        ImGui::BeginDisabled(!available || pending_player.valid());
        ImGui::Checkbox("Remember sign-in", &remember_sign_in);
        if (ImGui::Button("Import player credential...")) {
            try {
                if (auto file = credential_picker(owner)) {
                    auto credential =
                        parse_player_credential(read_text(*file, 16384), player_context_server);
                    begin_player_request(std::move(credential), remember_sign_in,
                                         !remember_sign_in);
                }
            } catch (const std::exception &e) {
                player_message = e.what();
                player_error = true;
            }
        }
        if (ImGui::Button("Use saved sign-in"))
            use_saved_player();
        ImGui::EndDisabled();
    }
    gap(10);
    subdued("Independent community progression.");
}
void App::show_local_notes() {
    ++notes_generation;
    notes =
        local_release_notes(executable_directory() / "docs", std::string(version::source_commit));
    note_textures.clear();
    notes_title = "This build / " + std::string(version::release_id);
    notes_requested = true;
}
void App::show_update_notes() {
    if (!update_release || pending_notes.valid())
        return;
    notes = {};
    note_textures.clear();
    notes_title = "Update / " + update_release->release_id;
    pending_notes_generation = ++notes_generation;
    pending_notes = notes_reader(update_release->source_commit);
    notes_requested = true;
}
void App::draw_notes() {
    if (notes_requested) {
        ImGui::OpenPopup("Release notes");
        notes_open = true;
        notes_requested = false;
    }
    auto area = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize({std::min(1000.f, area.x - 60), std::min(760.f, area.y - 60)},
                             ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Release notes", &notes_open, ImGuiWindowFlags_NoCollapse)) {
        text(notes_title);
        if (ImGui::Button("Close notes")) {
            notes_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::Separator();
        ImGui::BeginChild("note-content", {0, 0});
        if (pending_notes.valid() && pending_notes_generation == notes_generation)
            wrap("Loading release notes...");
        if (!notes.error.empty())
            subdued(notes.error);
        draw_release_notes({notes, note_textures, note_image_loader, body, strong, heading});
        ImGui::EndChild();
        ImGui::EndPopup();
    }
}
void App::tick() {
    const auto ready = [](auto &future) {
        return future.valid() &&
               future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    };
    if (ready(pending_notes)) {
        try {
            auto result = pending_notes.get();
            if (pending_notes_generation == notes_generation) {
                notes = std::move(result);
                note_textures.clear();
            }
        } catch (const std::exception &e) {
            if (pending_notes_generation == notes_generation)
                notes.error = e.what();
        }
    }
    if (ready(pending_update)) {
        try {
            auto result = pending_update.get();
            update_error = !result.release;
            if (result.release) {
                update_recorder(*result.release, result.envelope, data);
                update_release = std::move(result.release);
                update_envelope = std::move(result.envelope);
                auto relation = compare_update(*update_release, version::source_commit,
                                               version::source_revision);
                update_message =
                    relation == UpdateRelation::current ? "You have the published release."
                    : relation == UpdateRelation::local_newer
                        ? "This development build is newer than the published release."
                    : relation == UpdateRelation::available
                        ? "A newer verified release is available."
                        : "A verified release is available; this build's ordering is unknown.";
            } else
                update_message = result.error;
        } catch (const std::exception &e) {
            update_message = e.what();
            update_error = true;
        }
    }
    if (ready(pending_download)) {
        try {
            auto result = pending_download.get();
            update_error = !result.error.empty();
            if (update_error)
                update_message = result.error;
            else {
                downloaded_update = std::move(result.package);
                update_message =
                    "Package downloaded and SHA-256 verified. Ready to install and restart.";
            }
        } catch (const std::exception &e) {
            update_message = e.what();
            update_error = true;
        }
    }
    if (ready(pending_install)) {
        try {
            auto handoff = pending_install.get();
            update_error = !handoff.error.empty();
            if (update_error)
                update_message = handoff.error;
            else
                exit_for_update = true;
        } catch (const std::exception &e) {
            update_message = e.what();
            update_error = true;
        }
    }
    if (background_updates && settings.check_updates && !pending_update.valid() &&
        !pending_download.valid() && !pending_install.valid() &&
        std::chrono::steady_clock::now() >= next_update_check)
        check_for_updates();

    if (pending.valid() && pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto result = pending.get();
        auto selected = community();
        if (selected && selected->id == pending_id && selected->url == pending_url) {
            service = std::move(result.status);
            campaign = std::move(result.campaign);
            connected_id = service ? selected->id : "";
            connected_url = service ? selected->url : "";
            notice_error = !service;
            notice = service ? "Connected to " + service->name + "." : result.error;
            if (service && !result.campaign_error.empty()) {
                notice += " Campaign: " + result.campaign_error;
                notice_error = true;
            }
        }
    }
    sync_player_context();
    if (ready(pending_player)) {
        try {
            auto result = pending_player.get();
            if (pending_player_generation == player_generation) {
                if (result.profile && result.credential &&
                    result.credential->community_id == player_context_server &&
                    result.profile->id == result.credential->player_id) {
                    if (!player || player->id != result.profile->id || pending_player_remember ||
                        pending_player_forget)
                        player_options_open = false;
                    player = std::move(result.profile);
                    player_credential = std::move(result.credential);
                    player_error = false;
                    player_message = "Signed in to " + service->name + ".";
                    try {
                        if (pending_player_remember) {
                            player_saver(data, player_context_url, *player_credential);
                            player_remembered = true;
                        } else if (pending_player_forget) {
                            player_forgetter(data, player_context_url, player_context_server);
                            player_remembered = false;
                        }
                    } catch (const std::exception &e) {
                        player_message += " " + std::string(e.what());
                        player_error = true;
                        player_remembered = false;
                    }
                } else {
                    player.reset();
                    player_credential.reset();
                    player_options_open = true;
                    player_message = result.error.empty()
                                         ? "The community sign-in could not be verified."
                                         : result.error;
                    player_error = true;
                }
            }
        } catch (const std::exception &e) {
            if (pending_player_generation == player_generation) {
                player_message = e.what();
                player_error = true;
            }
        }
    }
    if (player_restore_requested && !pending_player.valid())
        use_saved_player();
    if (pending_identity.valid() &&
        pending_identity.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto identity = pending_identity.get();
            if (settings.game_path == identity_for_path) {
                json catalog = {{"schema", 1}, {"profiles", json::array()}};
                auto path = executable_directory() / "compatibility/client-builds.json";
                if (fs::exists(path))
                    catalog = json::parse(read_text(path));
                auto report = identity_report(identity, catalog);
                std::ofstream file(data / "client-inspection.json", std::ios::binary);
                file.exceptions(std::ios::failbit | std::ios::badbit);
                file << report.dump(2) << '\n';
                file.close();
                game = identity.installation;
                bool errors = !identity.metadata_error.empty() ||
                              std::any_of(identity.files.begin(), identity.files.end(),
                                          [](const auto &f) { return !f.error.empty(); }) ||
                              std::any_of(identity.startup_files.begin(), identity.startup_files.end(),
                                          [](const auto &f) { return !f.error.empty(); });
                identity_summary = !identity.metadata_error.empty() ? identity.metadata_error
                                   : errors ? "Inspection recorded missing or unreadable files."
                                   : report["identity"]["known"].get<bool>()
                                       ? "Client and startup fingerprints match an observed build. Game "
                                         "integration is not implemented."
                                       : "Client and startup fingerprints recorded; this build is not in the "
                                         "identity catalog.";
                if (!errors && identity.build_source == "preparation_receipt")
                    identity_summary = "Prepared copy / build " + game.build + ". " + identity_summary;
                notice = identity_summary + " Report saved as client-inspection.json.";
                notice_error = errors;
            }
        } catch (const std::exception &e) {
            notice = e.what();
            notice_error = true;
        }
    }
}
void App::draw(void *owner) {
    tick();
    auto &io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
        save();
    if (!notes_open && !io.WantTextInput && !io.KeyCtrl && !io.KeyAlt) {
        if (ImGui::IsKeyPressed(ImGuiKey_Q))
            page = (page + 5) % 6;
        if (ImGui::IsKeyPressed(ImGuiKey_E))
            page = (page + 1) % 6;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F1))
        page = 5;
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Community Divers", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    auto origin = ImGui::GetWindowPos();
    auto size = ImGui::GetWindowSize();
    auto *background = ImGui::GetWindowDrawList();
    background->AddRectFilledMultiColor(origin, {origin.x + size.x, origin.y + size.y},
                                        IM_COL32(25, 34, 44, 255), IM_COL32(15, 23, 33, 255),
                                        IM_COL32(6, 10, 16, 255), IM_COL32(10, 15, 22, 255));
    if (logo) {
        ImGui::Image((ImTextureID)(intptr_t)logo, {90, 90 / logo_ratio});
        ImGui::SameLine(0, 20);
    }
    ImGui::BeginGroup();
    ImGui::PushFont(title);
    text("COMMUNITY DIVERS");
    ImGui::PopFont();
    ImGui::PushFont(caption_font);
    subdued("COMMUNITY LAUNCHER  /  " + std::string(version::release_id));
    ImGui::PopFont();
    ImGui::EndGroup();
    ImGui::SameLine(ImGui::GetWindowWidth() - 300);
    ImGui::BeginGroup();
    ImGui::TextColored(accent, "%s", game.valid ? "GAME FILES DETECTED" : "GAME SETUP REQUIRED");
    ImGui::PushFont(caption_font);
    subdued(game.build.empty() ? "Build not identified" : "STEAM BUILD  " + game.build);
    ImGui::PopFont();
    ImGui::EndGroup();
    gap(22);
    const char *pages[] = {"DEPLOYMENT",   "CAMPAIGN", "COMMUNITIES",
                           "MOD PROFILES", "SETTINGS", "ABOUT"};
    const float tab_width = (ImGui::GetContentRegionAvail().x - 5 * 8) / 6;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {8, 0});
    for (int i = 0; i < 6; ++i) {
        ImGui::PushID(i);
        bool selected = page == i;
        ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImVec4{.12f, .16f, .20f, .95f}
                                                        : ImVec4{.025f, .04f, .06f, .8f});
        ImGui::PushStyleColor(ImGuiCol_Border, selected ? ImVec4{.92f, .94f, .96f, 1}
                                                        : ImVec4{.35f, .40f, .46f, .5f});
        if (ImGui::Button("##section", {tab_width, 62}))
            page = i;
        auto a = ImGui::GetItemRectMin();
        auto b = ImGui::GetItemRectMax();
        auto *draw = ImGui::GetWindowDrawList();
        draw->AddText(body, 18, {a.x + 12, a.y + 11},
                      selected ? IM_COL32(255, 232, 82, 255) : IM_COL32(225, 230, 236, 255),
                      pages[i]);
        if (selected) {
            draw->PushClipRect({a.x + 12, b.y - 18}, {b.x - 36, b.y - 7}, true);
            for (float x = a.x + 7; x < b.x - 25; x += 12)
                draw->AddLine({x, b.y - 7}, {x + 11, b.y - 19}, IM_COL32(208, 216, 225, 150), 4);
            draw->PopClipRect();
        }
        char number[4];
        std::snprintf(number, sizeof(number), "%02d", i + 1);
        draw->AddText(caption_font, 13, {b.x - 27, b.y - 22}, IM_COL32(145, 156, 169, 255), number);
        ImGui::PopStyleColor(2);
        ImGui::PopID();
        if (i != 5)
            ImGui::SameLine();
    }
    ImGui::PopStyleVar();
    gap(22);
    if (!settings_warning.empty()) {
        ImGui::TextColored(accent, "SETTINGS NEED ATTENTION");
        wrap(settings_warning);
        gap(8);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {20, 20});
    ImGui::BeginChild("workspace", {0, -84}, ImGuiChildFlags_AlwaysUseWindowPadding);
    float available = ImGui::GetContentRegionAvail().x;
    if (page == 0) {
        const float left_width = available * .55f;
        ImGui::BeginChild("deployment-options", {left_width, 0});
        section_head("Session configuration");
        gap(10);
        auto option = [&](const char *name, const std::string &value, int destination) {
            ImGui::PushID(name);
            ImGui::PushStyleColor(ImGuiCol_Button, {.025f, .035f, .05f, .95f});
            bool clicked = ImGui::Button("##option", {ImGui::GetContentRegionAvail().x, 86});
            auto a = ImGui::GetItemRectMin();
            auto b = ImGui::GetItemRectMax();
            auto *d = ImGui::GetWindowDrawList();
            bool focus = ImGui::IsItemHovered() || ImGui::IsItemFocused();
            if (focus) {
                d->AddRectFilled(a, b, IM_COL32(105, 96, 31, 100));
                d->AddRectFilled(a, {a.x + 3, b.y}, IM_COL32(255, 230, 68, 255));
            }
            d->AddText(body, 19, {a.x + 18, a.y + 15}, IM_COL32(240, 243, 247, 255), name);
            d->PushClipRect({a.x + 18, a.y + 43}, {b.x - 40, b.y - 8}, true);
            d->AddText(caption_font, 15, {a.x + 18, a.y + 48}, IM_COL32(158, 173, 187, 255),
                       value.c_str());
            d->PopClipRect();
            d->AddTriangleFilled({b.x - 24, a.y + 35}, {b.x - 16, a.y + 43}, {b.x - 24, a.y + 51},
                                 focus ? IM_COL32(255, 230, 68, 255)
                                       : IM_COL32(175, 186, 198, 255));
            if (clicked)
                page = destination;
            ImGui::PopStyleColor();
            ImGui::PopID();
            gap(6);
        };
        option("GAME INSTALLATION",
               game.valid ? "HELLDIVERS 2 / BUILD " + game.build : "Choose an installed copy", 4);
        option("COMMUNITY", community() ? community()->name : "Choose a community service", 2);
        option("MOD PROFILE",
               profile().name + " / " + std::to_string(profile().enabled_mods.size()) + " selected",
               3);
        ImGui::EndChild();
        ImGui::SameLine(0, 30);
        ImGui::BeginChild("deployment-detail", {0, 0});
        section_head("Deployment details");
        gap(8);
        ImGui::PushFont(heading);
        text("COMMUNITY CLIENT");
        ImGui::PopFont();
        ImGui::TextColored(accent, "IN DEVELOPMENT");
        gap(12);
        wrap("The launcher can prepare your community connection and mod profile. The isolated "
             "game adapter is not available yet.");
        gap(14);
        subdued(launch_blocker(game, service ? &*service : nullptr));
        gap(18);
        ImGui::BeginDisabled();
        ImGui::Button("LAUNCH GAME", {ImGui::GetContentRegionAvail().x, 48});
        ImGui::EndDisabled();
        gap(8);
        text(service ? "Service: " + service->name : "Service connection: not checked");
        if (service && !service->required_launcher_release.empty())
            wrap("Required launcher: " + service->required_launcher_release);
        if (service && !service->required_release_set.empty())
            wrap("Community release set: " + service->required_release_set);

        ImGui::EndChild();
    } else if (page == 1) {
        ImGui::BeginChild("campaign-state", {available * .63f, 0});
        section_head("Community campaign");
        ImGui::BeginDisabled(!community() || pending.valid());
        if (ImGui::Button(pending.valid() ? "Refreshing campaign..." : "Refresh campaign")) {
            auto selected = community();
            pending_id = selected->id;
            pending_url = selected->url;
            pending = community_checker(selected->url);
        }
        ImGui::EndDisabled();
        gap(8);

        if (campaign) {
            ImGui::PushFont(heading);
            text(campaign->name);
            ImGui::PopFont();
            subdued((service ? service->name : campaign->community_id) + " / campaign revision " +
                    std::to_string(campaign->revision));
            gap(10);
            if (campaign->planets.empty())
                wrap("The operator has not configured any planets yet.");
            else if (ImGui::BeginTable(
                         "campaign-planets", 4,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                         {0, std::min(280.f, 46.f + static_cast<float>(campaign->planets.size()) *
                                                        42.f)})) {
                for (const char *label : {"Planet", "Biome", "Owner", "Liberation"})
                    ImGui::TableSetupColumn(label);
                ImGui::TableHeadersRow();
                for (const auto &planet : campaign->planets) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    text(planet.name);
                    ImGui::TableNextColumn();
                    text(planet.biome_id);
                    ImGui::TableNextColumn();
                    text(planet.owner_faction);
                    ImGui::TableNextColumn();
                    ImGui::Text("%.2f%%", static_cast<double>(planet.liberation) / 10000.0);
                }
                ImGui::EndTable();
            }
            gap(12);
            section_head("Configured mission templates");
            if (campaign->templates.empty())
                subdued("No mission templates configured.");
            for (const auto &mission : campaign->templates) {
                char contribution[64];
                std::snprintf(contribution, sizeof(contribution), "%.4f%% liberation",
                              static_cast<double>(mission.war_points) / 10000.0);
                wrap(mission.name + " / " + std::to_string(mission.reward_xp) + " XP / " +
                     contribution);
            }
            gap(12);
            subdued("This is the service's persisted campaign snapshot. Game launch and "
                    "multiplayer integration remain in development.");
        } else if (service) {
            text("Community: " + service->name);
            wrap("No campaign snapshot is available. Check the community connection again, or use "
                 "a service advertising campaign.read.");
        } else
            wrap("Select a community and check its connection to load its campaign.");
        ImGui::EndChild();
        ImGui::SameLine(0, 24);
        ImGui::BeginChild("community-player", {0, 0});
        draw_player(owner);
        ImGui::EndChild();
    } else if (page == 2) {
        float left = available * .58f;
        ImGui::BeginChild("community-list", {left, 0});
        if (ImGui::Button("Check selected")) {
            if (auto c = community(); c && !pending.valid()) {
                pending_id = c->id;
                pending_url = c->url;
                pending = community_checker(c->url);
                service.reset();
                campaign.reset();
                notice = "Checking community service...";
                notice_error = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove selected")) {
            if (auto c = community()) {
                auto key = c->id;
                std::erase_if(settings.communities, [&](const auto &v) { return v.id == key; });
                settings.selected_community =
                    settings.communities.empty() ? "" : settings.communities.front().id;
                service.reset();
                campaign.reset();
                save();
            }
        }
        gap(4);
        if (ImGui::BeginTable("servers", 2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                              {0, -5})) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, .42f);
            ImGui::TableSetupColumn("Service URL", ImGuiTableColumnFlags_WidthStretch, .58f);
            ImGui::TableHeadersRow();
            for (const auto &c : settings.communities) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(c.id.c_str());
                if (ImGui::Selectable(c.name.c_str(), settings.selected_community == c.id,
                                      ImGuiSelectableFlags_SpanAllColumns)) {
                    settings.selected_community = c.id;
                    service.reset();
                    campaign.reset();
                    save();
                }
                ImGui::TableNextColumn();
                text(c.url);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("community-form", {0, 0});
        section_head("Add community");
        text("Name");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##name", server_name, sizeof(server_name));
        text("Base URL");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##url", "https://community.example", server_url,
                                 sizeof(server_url));
        ImGui::BeginDisabled(!server_name[0] || !server_url[0] ||
                             settings.communities.size() >= 100);
        if (ImGui::Button("Add community")) {
            try {
                auto e = parse_endpoint(server_url);
                if (std::any_of(settings.communities.begin(), settings.communities.end(),
                                [&](const auto &c) { return c.url == e.canonical; }))
                    throw std::runtime_error("This URL is already saved.");
                auto key = id("community-");
                settings.communities.push_back({key, server_name, e.canonical});
                settings.selected_community = key;
                service.reset();
                campaign.reset();
                server_name[0] = server_url[0] = 0;
                save();
            } catch (const std::exception &e) {
                notice = e.what();
                notice_error = true;
            }
        }
        ImGui::EndDisabled();
        gap(18);
        subdued("Enter a community's base HTTPS address. Public status checks send no account "
                "credentials. "
                "A remembered sign-in can then refresh your profile separately.");
        gap();
        subdued("HTTP is available only for local development on loopback addresses.");
        ImGui::EndChild();
    } else if (page == 3) {
        ImGui::SetNextItemWidth(240);
        if (ImGui::BeginCombo("Profile", profile().name.c_str())) {
            for (const auto &p : settings.profiles)
                if (ImGui::Selectable(p.name.c_str(), p.id == settings.selected_profile)) {
                    settings.selected_profile = p.id;
                    save();
                }
            ImGui::EndCombo();
        }
        ImGui::SetNextItemWidth(240);
        ImGui::InputTextWithHint("##profile", "New profile name", profile_name,
                                 sizeof(profile_name));
        ImGui::SameLine();
        ImGui::BeginDisabled(!profile_name[0] || settings.profiles.size() >= 32);
        if (ImGui::Button("Create profile")) {
            auto key = id("profile-");
            settings.profiles.push_back({key, profile_name, {}});
            settings.selected_profile = key;
            profile_name[0] = 0;
            save();
        }
        ImGui::EndDisabled();
        gap(4);
        if (ImGui::Button("Open mods folder"))
            open_folder(data / "mods");
        ImGui::SameLine();
        if (ImGui::Button("Refresh"))
            scan();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(240);
        ImGui::InputTextWithHint("##search", "Search mods", mod_search, sizeof(mod_search));
        subdued(
            "Profile selections are saved. Deployment is awaiting the isolated client adapter.");
        for (const auto &selected : profile().enabled_mods) {
            if (std::none_of(mods.begin(), mods.end(),
                             [&](const auto &mod) { return mod.id == selected; })) {
                ImGui::TextColored(accent, "Missing selected mod: %s", selected.c_str());
            }
        }
        if (ImGui::BeginTable("mods", 4,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                              {0, -8})) {
            ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 65);
            ImGui::TableSetupColumn("Mod");
            ImGui::TableSetupColumn("Version", ImGuiTableColumnFlags_WidthFixed, 90);
            ImGui::TableSetupColumn("Details");
            ImGui::TableHeadersRow();
            for (const auto &mod : mods) {
                if (lower(mod.name + " " + mod.id).find(lower(mod_search)) == std::string::npos)
                    continue;
                ImGui::PushID(path_text(mod.directory).c_str());
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                auto &enabled = profile().enabled_mods;
                bool selected = std::find(enabled.begin(), enabled.end(), mod.id) != enabled.end();
                ImGui::BeginDisabled(!mod.error.empty());
                if (ImGui::Checkbox("##enabled", &selected)) {
                    if (selected)
                        enabled.push_back(mod.id);
                    else
                        std::erase(enabled, mod.id);
                    save();
                }
                ImGui::EndDisabled();
                ImGui::TableNextColumn();
                text(mod.name);
                ImGui::TableNextColumn();
                text(mod.version);
                ImGui::TableNextColumn();
                wrap(mod.error.empty() ? mod.description : mod.error);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    } else if (page == 4) {
        section_head("Game installation");
        text("HELLDIVERS 2 directory");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##path", game_path, sizeof(game_path));
        if (ImGui::Button("Use folder")) {
            settings.game_path = game_path;
            scan();
            save();
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            try {
                if (auto selected = choose_game_folder(owner)) {
                    settings.game_path = path_text(*selected);
                    scan();
                    save();
                }
            } catch (const std::exception &e) {
                notice = e.what();
                notice_error = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Find in Steam libraries")) {
            auto found = discover_steam(steam_roots());
            auto valid =
                std::find_if(found.begin(), found.end(), [](const auto &g) { return g.valid; });
            if (valid != found.end()) {
                settings.game_path = path_text(valid->root);
                scan();
                save();
            } else {
                notice = "No valid game installation was found in the Steam libraries.";
                notice_error = true;
            }
        }
        gap(8);
        wrap(game.detail);
        gap(18);
        section_head("Client compatibility");
        ImGui::BeginDisabled(!game.valid || pending_identity.valid());
        if (ImGui::Button(pending_identity.valid() ? "Inspecting client..."
                                                   : "Inspect client files")) {
            identity_for_path = settings.game_path;
            identity_summary.clear();
            pending_identity = inspect_client_async(game.root);
            notice = "Inspecting executable, engine and network-library fingerprints...";
            notice_error = false;
        }
        ImGui::EndDisabled();
        subdued(identity_summary.empty()
                    ? "Create a read-only file-identity report for client integration analysis."
                    : identity_summary);
        gap(18);
        section_head("Launcher data");
        text(path_text(data));
        if (ImGui::Button("Open data folder"))
            open_folder(data);
        ImGui::SameLine();
        if (ImGui::Button("Save settings"))
            save();
        gap(8);
        subdued("Installation discovery reads game files and Steam metadata. Launcher settings and "
                "mod profiles are stored separately from the game.");
    } else if (page == 5) {
        section_head("Launcher updates");
        if (ImGui::Checkbox("Check for updates automatically", &settings.check_updates)) {
            save();
            if (settings.check_updates && background_updates)
                check_for_updates();
        }
        ImGui::BeginDisabled(pending_update.valid() || pending_download.valid() ||
                             pending_install.valid());
        if (ImGui::Button(pending_update.valid() ? "Checking updates..." : "Check for updates"))
            check_for_updates();
        ImGui::EndDisabled();
        if (update_error)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1, .58f, .48f, 1});
        wrap(update_message);
        if (update_error)
            ImGui::PopStyleColor();
        if (ImGui::Button("What's new in this build"))
            show_local_notes();
        if (update_release) {
            ImGui::BeginDisabled(pending_notes.valid());
            if (ImGui::Button("Read update notes"))
                show_update_notes();
            ImGui::EndDisabled();
            subdued("Published " + update_release->release_id + " / " +
                    std::to_string(update_release->bytes / 1024) + " KiB");
            auto relation =
                compare_update(*update_release, version::source_commit, version::source_revision);
            ImGui::BeginDisabled(pending_download.valid() || pending_update.valid() ||
                                 pending_install.valid() || relation == UpdateRelation::current ||
                                 relation == UpdateRelation::local_newer);
            if (ImGui::Button(pending_download.valid() ? "Downloading update..."
                                                       : "Download verified package")) {
                downloaded_update.clear();
                downloaded_envelope = update_envelope;
                update_error = false;
                update_message = "Downloading the signed release package...";
                pending_download = update_downloader(*update_release, data / "updates");
            }
            ImGui::EndDisabled();
        }
        if (!downloaded_update.empty()) {
            ImGui::BeginDisabled(!portable_install || downloaded_envelope.empty() ||
                                 pending_install.valid() || pending_update.valid() ||
                                 pending_download.valid());
            if (ImGui::Button(pending_install.valid() ? "Preparing installation..."
                                                      : "Install and restart")) {
                update_error = false;
                update_message = "Verifying files and preparing a recoverable update...";
                pending_install = update_installer(downloaded_update, downloaded_envelope,
                                                   executable_directory(), data);
            }
            ImGui::EndDisabled();
            if (ImGui::Button("Open downloaded package folder"))
                open_folder(downloaded_update.parent_path());
        }
        if (!portable_install)
            subdued("Use a portable installation to enable Install and restart.");
        subdued("Preview channel. Installation restarts the launcher and keeps a verified backup. "
                "A failed startup restores the previous version.");
        gap(18);
        section_head("COMMUNITY DIVERS");
        text("Launcher " + std::string(version::release_id));
        if (version::source_dirty)
            subdued("Local development build with uncommitted changes.");
        gap(10);
        ImGui::TextColored(accent, "Mostly AI-generated project");
        wrap("Most of this project's code and artwork is AI-generated. Review and validation are "
             "ongoing. Available features, test coverage and remaining limitations are documented "
             "in the repository.");
        gap(18);
        section_head("Project");
        text("https://github.com/Nnamllit1/community-divers");
        text("Launcher and shared code: MIT");
        wrap("Independent service: Community Divers Service License 1.0. "
             "Standalone HellForge loader: GPLv3 with its mod exception. "
             "See docs/licensing-plan.md for component terms and earlier MIT grants.");
        gap(10);
        subdued("An independent community project, unaffiliated with Arrowhead Game Studios or "
                "Sony Interactive Entertainment.");
    }
    ImGui::EndChild();
    draw_notes();
    ImGui::PopStyleVar();
    gap(10);
    ImGui::Separator();
    if (ImGui::Button("CTRL+S  SAVE SETTINGS"))
        save();
    ImGui::SameLine();
    if (ImGui::Button("F1  ABOUT"))
        page = 5;
    ImGui::SameLine();
    ImGui::PushFont(caption_font);
    if (!notice.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, notice_error ? ImVec4{1, .58f, .48f, 1} : muted);
        text(notice);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", notice.c_str());
    } else
        subdued("Q / E  SWITCH SECTION");
    ImGui::PopFont();
    ImGui::End();
}
} // namespace cd
