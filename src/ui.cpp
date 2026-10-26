#include "ui.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <set>
namespace cd {
namespace {
ImVec4 teal{.45f, .89f, .80f, 1}, muted{.57f, .65f, .70f, 1}, gold{.96f, .81f, .39f, 1};
void text(const std::string &value) {
    ImGui::TextUnformatted(value.c_str());
}
void wrap(const std::string &value) {
    ImGui::PushTextWrapPos(0);
    text(value);
    ImGui::PopTextWrapPos();
}
void subdued(const std::string &value) {
    ImGui::PushStyleColor(ImGuiCol_Text, muted);
    wrap(value);
    ImGui::PopStyleColor();
}
void gap(float n = 12) {
    ImGui::Dummy({0, n});
}
void label(const char *value) {
    ImGui::TextColored(teal, "%s", value);
    gap(5);
}
bool action(const char *value, ImVec2 size = {0, 38}) {
    ImGui::PushStyleColor(ImGuiCol_Button, {.21f, .67f, .59f, 1});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {.34f, .82f, .72f, 1});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, {.24f, .73f, .64f, 1});
    ImGui::PushStyleColor(ImGuiCol_Text, {.03f, .09f, .10f, 1});
    bool pressed = ImGui::Button(value, size);
    ImGui::PopStyleColor(4);
    return pressed;
}
void card(const char *id, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, {.075f, .11f, .14f, 1});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {22, 20});
    ImGui::BeginChild(id, size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
}
void end_card() {
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
std::string id(const char *prefix) {
    static unsigned serial = 0;
    return std::string(prefix) +
           std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "-" +
           std::to_string(serial++);
}
void buffer(char *target, size_t size, const std::string &value) {
    std::snprintf(target, size, "%s", value.c_str());
}
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
} // namespace
void theme() {
    auto &s = ImGui::GetStyle();
    s.WindowRounding = 0;
    s.ChildRounding = 12;
    s.FrameRounding = 7;
    s.PopupRounding = 9;
    s.GrabRounding = 6;
    s.ScrollbarRounding = 8;
    s.WindowBorderSize = 0;
    s.ChildBorderSize = 1;
    s.FrameBorderSize = 0;
    s.WindowPadding = {0, 0};
    s.FramePadding = {12, 9};
    s.ItemSpacing = {12, 10};
    s.ScrollbarSize = 12;
    auto c = s.Colors;
    c[ImGuiCol_Text] = {.92f, .95f, .96f, 1};
    c[ImGuiCol_TextDisabled] = muted;
    c[ImGuiCol_WindowBg] = {.04f, .065f, .09f, 1};
    c[ImGuiCol_ChildBg] = {.055f, .08f, .105f, 1};
    c[ImGuiCol_PopupBg] = {.075f, .11f, .14f, 1};
    c[ImGuiCol_Border] = {.15f, .21f, .25f, 1};
    c[ImGuiCol_Separator] = c[ImGuiCol_Border];
    c[ImGuiCol_FrameBg] = {.105f, .155f, .19f, 1};
    c[ImGuiCol_FrameBgHovered] = {.14f, .22f, .25f, 1};
    c[ImGuiCol_FrameBgActive] = {.16f, .28f, .29f, 1};
    c[ImGuiCol_Button] = {.12f, .19f, .23f, 1};
    c[ImGuiCol_ButtonHovered] = {.18f, .29f, .32f, 1};
    c[ImGuiCol_ButtonActive] = {.20f, .35f, .35f, 1};
    c[ImGuiCol_Header] = {.12f, .25f, .26f, 1};
    c[ImGuiCol_HeaderHovered] = {.17f, .31f, .32f, 1};
    c[ImGuiCol_HeaderActive] = {.18f, .36f, .35f, 1};
    c[ImGuiCol_CheckMark] = teal;
    c[ImGuiCol_SliderGrab] = teal;
    c[ImGuiCol_NavCursor] = teal;
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
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Community Divers", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {22, 24});
    ImGui::BeginChild("navigation", {228, 0}, ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar);
    if (logo) {
        ImGui::SetCursorPosX(24);
        ImGui::Image((ImTextureID)(intptr_t)logo, {180, 180 / logo_ratio});
    }
    gap(12);
    ImGui::PushFont(heading);
    text("COMMUNITY");
    text("DIVERS");
    ImGui::PopFont();
    gap(6);
    subdued("Your community.\nYour campaign.");
    gap(28);
    const char *pages[] = {"Overview", "Campaign", "Communities", "Mod profiles", "Settings"};
    for (int i = 0; i < 5; ++i) {
        ImGui::PushID(i);
        bool selected = page == i;
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, {.12f, .27f, .28f, 1});
        if (ImGui::Button(pages[i], {184, 43}))
            page = i;
        if (selected)
            ImGui::PopStyleColor();
        ImGui::PopID();
    }
    float footer = ImGui::GetWindowHeight() - 100;
    if (ImGui::GetCursorPosY() < footer)
        ImGui::SetCursorPosY(footer);
    ImGui::Separator();
    gap(7);
    ImGui::PushFont(caption_font);
    subdued("PREVIEW  " CD_VERSION);
    subdued("Community client in development");
    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::SameLine(0, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {32, 26});
    ImGui::BeginChild("content", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding);
    label(pages[page]);
    ImGui::PushFont(heading);
    const char *titles[] = {"A campaign of your own.", "Your shared frontline.",
                            "Find your community.", "Make it your own.", "Launcher settings."};
    text(titles[page]);
    ImGui::PopFont();
    gap(14);
    if (!settings_warning.empty()) {
        ImGui::TextColored(gold, "Settings need attention");
        wrap(settings_warning);
        gap();
    }
    if (!notice.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, notice_error ? ImVec4{1, .60f, .48f, 1} : teal);
        wrap(notice);
        ImGui::PopStyleColor();
        gap(8);
    }
    float width = ImGui::GetContentRegionAvail().x;
    if (page == 0) {
        card("hero", {0, 256});
        auto start = ImGui::GetCursorPos();
        float available = ImGui::GetContentRegionAvail().x;
        ImGui::PushFont(title);
        text("YOUR WORLD.\nYOUR RULES.");
        ImGui::PopFont();
        gap(8);
        ImGui::PushTextWrapPos(start.x + available * .57f);
        ImGui::PushStyleColor(ImGuiCol_Text, muted);
        text("Choose a community, build your mod profile, and prepare for a shared war shaped by "
             "its players.");
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
        gap(6);
        if (action("Choose a community"))
            page = 2;
        if (logo && available > 650) {
            ImGui::SetCursorPos({start.x + available * .65f, start.y + 12});
            ImGui::Image((ImTextureID)(intptr_t)logo,
                         {available * .32f, available * .32f / logo_ratio});
        }
        end_card();
        gap(4);
        float half = (width - 16) / 2;
        card("game", {half, 190});
        label(game.valid ? "GAME DETECTED" : "GAME SETUP");
        ImGui::PushFont(heading);
        text("HELLDIVERS 2");
        ImGui::PopFont();
        subdued(game.build.empty() ? "Build not identified" : "Steam build " + game.build);
        if (ImGui::Button("Installation settings"))
            page = 4;
        end_card();
        ImGui::SameLine(0, 16);
        card("community", {half, 190});
        label(service ? "SERVICE CONNECTED" : "COMMUNITY");
        ImGui::PushFont(heading);
        text(community() ? community()->name : "No community selected");
        ImGui::PopFont();
        subdued(service ? service->name : "Add a community endpoint to check its service.");
        if (ImGui::Button("Manage communities"))
            page = 2;
        end_card();
        gap(4);
        card("readiness", {0, 166});
        label("PLAY READINESS");
        ImGui::BeginDisabled();
        ImGui::Button("Launch community game", {235, 38});
        ImGui::EndDisabled();
        gap(3);
        subdued(launch_blocker(game, service ? &*service : nullptr));
        end_card();
    } else if (page == 1) {
        card("campaign", {0, 315});
        if (service && !service->campaign.empty()) {
            label("CONNECTED CAMPAIGN");
            ImGui::PushFont(title);
            wrap(service->campaign);
            ImGui::PopFont();
            subdued("Reported by " + service->name + ".");
        } else {
            label("SHARED WAR CAMPAIGN");
            ImGui::PushFont(title);
            text("A world worth\nbuilding together.");
            ImGui::PopFont();
            gap();
            subdued("Connect to a community service to see its campaign identity. War state, "
                    "voting and mission contributions will arrive with the campaign backend.");
        }
        gap(18);
        if (action("Community connections"))
            page = 2;
        end_card();
        gap();
        subdued("Each community will operate its own war and progression. No live war data is "
                "available in this launcher preview.");
    } else if (page == 2) {
        card("servers", {width * .47f, 435});
        label("YOUR COMMUNITIES");
        if (settings.communities.empty())
            subdued("No servers added yet. Use the form to add a service you or your community "
                    "operate.");
        std::string remove;
        for (const auto &c : settings.communities) {
            ImGui::PushID(c.id.c_str());
            if (ImGui::Selectable(c.name.c_str(), settings.selected_community == c.id, 0,
                                  {0, 34})) {
                settings.selected_community = c.id;
                service.reset();
                connected_id.clear();
                save();
            }
            ImGui::PushFont(caption_font);
            subdued(c.url);
            ImGui::PopFont();
            if (settings.selected_community == c.id) {
                ImGui::BeginDisabled(pending.valid());
                if (ImGui::Button(pending.valid() ? "Checking..." : "Check connection")) {
                    pending_id = c.id;
                    pending_url = c.url;
                    pending = probe_service(c.url);
                    notice = "Checking community service...";
                    notice_error = false;
                    service.reset();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Remove"))
                    remove = c.id;
            }
            gap(5);
            ImGui::Separator();
            ImGui::PopID();
        }
        if (!remove.empty()) {
            std::erase_if(settings.communities, [&](const auto &c) { return c.id == remove; });
            settings.selected_community =
                settings.communities.empty() ? "" : settings.communities.front().id;
            service.reset();
            save();
        }
        end_card();
        ImGui::SameLine(0, 16);
        card("add", {0, 435});
        label("ADD A COMMUNITY");
        text("Display name");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##name", server_name, sizeof(server_name));
        text("Service URL");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##url", "https://community.example", server_url,
                                 sizeof(server_url));
        gap(8);
        ImGui::BeginDisabled(!server_name[0] || !server_url[0] ||
                             settings.communities.size() >= 100);
        if (action("Add community")) {
            try {
                auto endpoint = parse_endpoint(server_url);
                if (std::any_of(settings.communities.begin(), settings.communities.end(),
                                [&](const auto &c) { return c.url == endpoint.canonical; }))
                    throw std::runtime_error("This community URL is already saved.");
                auto key = id("community-");
                settings.communities.push_back({key, server_name, endpoint.canonical});
                settings.selected_community = key;
                service.reset();
                server_name[0] = server_url[0] = '\0';
                save();
            } catch (const std::exception &e) {
                notice = e.what();
                notice_error = true;
            }
        }
        ImGui::EndDisabled();
        gap(12);
        subdued("Use the base HTTPS address published by your community. The launcher checks "
                "/v1/status without sending account credentials.");
        gap();
        subdued("Local development services may use HTTP on localhost or loopback addresses.");
        end_card();
        gap();
        subdued("A service connection checks availability and advertised game builds. A working "
                "game adapter is also required before community play.");
    } else if (page == 3) {
        card("profiles", {0, 190});
        label("MOD PROFILE");
        ImGui::SetNextItemWidth(260);
        if (ImGui::BeginCombo("Active profile", profile().name.c_str())) {
            for (const auto &p : settings.profiles)
                if (ImGui::Selectable(p.name.c_str(), p.id == settings.selected_profile)) {
                    settings.selected_profile = p.id;
                    save();
                }
            ImGui::EndCombo();
        }
        ImGui::SetNextItemWidth(260);
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
        subdued("Selections are saved per profile. Mod deployment is awaiting the isolated client "
                "adapter.");
        end_card();
        gap(6);
        if (ImGui::Button("Open mods folder"))
            open_folder(data / "mods");
        ImGui::SameLine();
        if (ImGui::Button("Refresh manifests"))
            scan();
        gap(4);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##search", "Search local mods", mod_search, sizeof(mod_search));
        if (mods.empty()) {
            gap(20);
            subdued("No local mod manifests found. Add a folder containing mod.json to your mods "
                    "directory; discovery reads metadata only.");
        }
        std::set<std::string> present;
        for (const auto &mod : mods) {
            present.insert(mod.id);
            if (lower(mod.name + " " + mod.id).find(lower(mod_search)) == std::string::npos)
                continue;
            card(mod.directory.string().c_str(), {0, 135});
            auto &enabled = profile().enabled_mods;
            bool selected = std::find(enabled.begin(), enabled.end(), mod.id) != enabled.end();
            ImGui::BeginDisabled(!mod.error.empty());
            if (ImGui::Checkbox(mod.name.c_str(), &selected)) {
                if (selected)
                    enabled.push_back(mod.id);
                else
                    std::erase(enabled, mod.id);
                save();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextColored(muted, "%s", mod.version.c_str());
            subdued(mod.error.empty() ? mod.description : mod.error);
            end_card();
            gap(3);
        }
        for (const auto &key : profile().enabled_mods)
            if (!present.contains(key)) {
                ImGui::TextColored(gold, "Missing enabled mod: %s", key.c_str());
            }
    } else if (page == 4) {
        card("installation", {0, 310});
        label("GAME INSTALLATION");
        subdued("Choose the existing HELLDIVERS 2 folder. Installation discovery only reads files "
                "and Steam metadata.");
        gap(8);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##gamepath", game_path, sizeof(game_path));
        if (action("Use this folder")) {
            settings.game_path = game_path;
            scan();
            save();
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse...", {0, 38})) {
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
        if (ImGui::Button("Find through Steam", {0, 38})) {
            auto found = discover_steam(steam_roots());
            auto valid =
                std::find_if(found.begin(), found.end(), [](const auto &g) { return g.valid; });
            if (valid != found.end()) {
                settings.game_path = path_text(valid->root);
                scan();
                save();
            } else {
                notice = "No valid HELLDIVERS 2 installation was found in the registered Steam "
                         "libraries.";
                notice_error = true;
            }
        }
        gap(7);
        ImGui::TextColored(game.valid ? teal : gold, "%s",
                           game.valid ? "Installation detected" : "Installation needs attention");
        subdued(game.detail);
        if (!game.build.empty())
            text("Steam build " + game.build);
        end_card();
        gap(6);
        card("storage", {0, 184});
        label("LAUNCHER STORAGE");
        ImGui::PushFont(caption_font);
        subdued(path_text(data));
        ImGui::PopFont();
        if (ImGui::Button("Open launcher data"))
            open_folder(data);
        ImGui::SameLine();
        if (ImGui::Button("Save current settings"))
            save();
        subdued("Settings and mod profiles belong to this launcher. Invalid settings are preserved "
                "during recovery.");
        end_card();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
}
} // namespace cd
