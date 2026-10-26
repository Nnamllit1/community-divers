#include "ui.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <set>
namespace cd {
namespace {
const ImVec4 accent{.87f, .79f, .43f, 1}, muted{.60f, .63f, .58f, 1};
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
    ImGui::PushStyleColor(ImGuiCol_Text, accent);
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
    s.WindowRounding = 0;
    s.ChildRounding = 0;
    s.FrameRounding = 0;
    s.PopupRounding = 0;
    s.GrabRounding = 0;
    s.ScrollbarRounding = 0;
    s.WindowBorderSize = 0;
    s.ChildBorderSize = 1;
    s.FrameBorderSize = 1;
    s.WindowPadding = {14, 12};
    s.FramePadding = {9, 5};
    s.ItemSpacing = {9, 8};
    s.CellPadding = {8, 7};
    auto c = s.Colors;
    c[ImGuiCol_Text] = {.9f, .9f, .9f, 1};
    c[ImGuiCol_TextDisabled] = muted;
    c[ImGuiCol_WindowBg] = {.065f, .075f, .065f, 1};
    c[ImGuiCol_ChildBg] = {.08f, .095f, .08f, 1};
    c[ImGuiCol_PopupBg] = {.10f, .12f, .10f, 1};
    c[ImGuiCol_Border] = {.26f, .29f, .23f, 1};
    c[ImGuiCol_Separator] = c[ImGuiCol_Border];
    c[ImGuiCol_FrameBg] = {.045f, .055f, .045f, 1};
    c[ImGuiCol_FrameBgHovered] = {.16f, .18f, .14f, 1};
    c[ImGuiCol_FrameBgActive] = {.20f, .22f, .16f, 1};
    c[ImGuiCol_Button] = {.17f, .19f, .14f, 1};
    c[ImGuiCol_ButtonHovered] = {.27f, .29f, .20f, 1};
    c[ImGuiCol_ButtonActive] = {.36f, .33f, .19f, 1};
    c[ImGuiCol_Header] = {.29f, .26f, .20f, 1};
    c[ImGuiCol_HeaderHovered] = {.28f, .28f, .29f, 1};
    c[ImGuiCol_HeaderActive] = {.37f, .32f, .22f, 1};
    c[ImGuiCol_Tab] = {.10f, .12f, .09f, 1};
    c[ImGuiCol_TabSelected] = {.24f, .25f, .17f, 1};
    c[ImGuiCol_TabHovered] = {.31f, .30f, .19f, 1};
    c[ImGuiCol_TabSelectedOverline] = accent;
    c[ImGuiCol_TableHeaderBg] = {.10f, .12f, .09f, 1};
    c[ImGuiCol_TableBorderStrong] = c[ImGuiCol_Border];
    c[ImGuiCol_TableBorderLight] = {.14f, .16f, .12f, 1};
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_SliderGrab] = accent;
}
App::App(fs::path data_path) : data(std::move(data_path)) {
    auto loaded = load_settings(data / "launcher.json");
    settings = std::move(loaded.value);
    settings_warning = loaded.error;
    fs::create_directories(data / "mods");
    scan();
}
void App::scan() {
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
void App::tick() {
    if (pending.valid() && pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto result = pending.get();
        auto selected = community();
        if (selected && selected->id == pending_id && selected->url == pending_url) {
            service = std::move(result.status);
            connected_id = service ? selected->id : "";
            notice_error = !service;
            notice = service ? "Connected to " + service->name + "." : result.error;
        }
    }
}
void App::draw(void *owner) {
    tick();
    auto &io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
        save();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Community Divers", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar);
    const char *pages[] = {"DEPLOYMENT", "CAMPAIGN", "NETWORK", "MODS", "CONFIGURATION", "ABOUT"};
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save settings", "Ctrl+S"))
                save();
            if (ImGui::MenuItem("Open launcher data"))
                open_folder(data);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            for (int i = 0; i < 6; ++i)
                if (ImGui::MenuItem(pages[i], nullptr, page == i))
                    page = i;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About Community Divers"))
                page = 5;
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    if (logo) {
        ImGui::Image((ImTextureID)(intptr_t)logo, {76, 76 / logo_ratio});
        ImGui::SameLine(0, 14);
    }
    ImGui::BeginGroup();
    ImGui::PushFont(this->heading);
    text("COMMUNITY DIVERS");
    ImGui::PopFont();
    subdued("LAUNCHER / PREVIEW " CD_VERSION);
    ImGui::EndGroup();
    ImGui::SameLine(ImGui::GetWindowWidth() - 340);
    ImGui::BeginGroup();
    text(game.valid ? "HELLDIVERS 2 detected" : "Game installation not selected");
    subdued(game.build.empty() ? "Build not identified" : "Steam build " + game.build);
    ImGui::EndGroup();
    gap(6);
    auto line = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        line, {line.x + ImGui::GetContentRegionAvail().x, line.y + 2},
        IM_COL32(222, 201, 110, 255));
    gap(6);
    if (!settings_warning.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, accent);
        wrap(settings_warning);
        ImGui::PopStyleColor();
    }
    const int requested_page = page;
    const bool programmatic_tab = page != rendered_page;
    if (ImGui::BeginTabBar("sections", ImGuiTabBarFlags_FittingPolicyScroll |
                                           ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {
        for (int i = 0; i < 6; ++i) {
            if (ImGui::BeginTabItem(
                    pages[i], nullptr,
                    programmatic_tab && requested_page == i ? ImGuiTabItemFlags_SetSelected : 0)) {
                if (!programmatic_tab || requested_page == i)
                    rendered_page = i;
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    if (programmatic_tab)
        rendered_page = requested_page;
    page = rendered_page;
    gap(6);
    ImGui::BeginChild("workspace", {0, -36}, ImGuiChildFlags_Borders);
    float available = ImGui::GetContentRegionAvail().x;
    if (page == 0) {
        section_head("Deployment configuration");
        if (ImGui::BeginTable("setup", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed, 150);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            text("Game installation");
            ImGui::TableNextColumn();
            text(game.valid ? "Detected" : "Not selected");
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            text("Community");
            ImGui::TableNextColumn();
            text(community() ? community()->name : "None selected");
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            text("Connection");
            ImGui::TableNextColumn();
            text(service ? service->name : "Not checked");
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            text("Mod profile");
            ImGui::TableNextColumn();
            text(profile().name + " (" + std::to_string(profile().enabled_mods.size()) +
                 " selected)");
            ImGui::EndTable();
        }
        gap(16);
        if (ImGui::Button("Choose community..."))
            page = 2;
        ImGui::SameLine();
        if (ImGui::Button("Configure mods..."))
            page = 3;
        ImGui::SameLine();
        if (ImGui::Button("Game settings..."))
            page = 4;
        gap(16);
        section_head("Deployment status");
        ImGui::BeginDisabled();
        ImGui::Button("Launch game", {150, 34});
        ImGui::EndDisabled();
        gap(3);
        wrap(launch_blocker(game, service ? &*service : nullptr));
        gap(16);
        subdued("This preview manages launcher settings and profiles. Community game launch is "
                "awaiting the isolated client adapter.");
    } else if (page == 1) {
        section_head("Campaign connection");
        if (service) {
            text("Community: " + service->name);
            text("Campaign: " + (service->campaign.empty() ? "Not advertised" : service->campaign));
        } else
            text("No community service connected.");
        gap(10);
        if (ImGui::Button("Manage connections..."))
            page = 2;
        gap(20);
        section_head("Campaign service status");
        wrap("War state, community decisions and mission contributions are not implemented in this "
             "preview. Each community will operate its own campaign and progression.");
    } else if (page == 2) {
        float left = available * .58f;
        ImGui::BeginChild("community-list", {left, 0});
        if (ImGui::Button("Check selected")) {
            if (auto c = community(); c && !pending.valid()) {
                pending_id = c->id;
                pending_url = c->url;
                pending = probe_service(c->url);
                service.reset();
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
                server_name[0] = server_url[0] = 0;
                save();
            } catch (const std::exception &e) {
                notice = e.what();
                notice_error = true;
            }
        }
        ImGui::EndDisabled();
        gap(18);
        subdued("Enter a community's base HTTPS address. Status checks use /v1/status and send no "
                "account credentials.");
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
                ImGui::PushID(mod.directory.string().c_str());
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
        section_head("COMMUNITY DIVERS");
        text("Launcher preview " CD_VERSION);
        gap(10);
        ImGui::TextColored(accent, "Mostly AI-generated project");
        wrap("Most of this project's code and artwork is AI-generated. Review and validation are "
             "ongoing. Available features, test coverage and remaining limitations are documented "
             "in the repository.");
        gap(18);
        section_head("Project");
        text("https://github.com/Nnamllit1/community-divers");
        text("Project code: MIT license");
        gap(10);
        subdued("An independent community project, unaffiliated with Arrowhead Game Studios or "
                "Sony Interactive Entertainment.");
    }
    ImGui::EndChild();
    gap(3);
    ImGui::Separator();
    if (!notice.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, notice_error ? ImVec4{1, .58f, .48f, 1} : muted);
        text(notice);
        ImGui::PopStyleColor();
    } else
        subdued(pending.valid() ? "Checking community service..."
                                : "Ready. Community game integration is in development.");
    ImGui::End();
}
} // namespace cd
