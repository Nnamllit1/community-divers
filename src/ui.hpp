#pragma once
#include "community/platform.hpp"
#include "imgui.h"
#include <d3d11.h>
namespace cd {
struct App {
    fs::path data;
    Settings settings;
    Installation game;
    std::vector<Mod> mods;
    std::optional<ServiceStatus> service;
    std::future<ProbeResult> pending;
    std::string pending_id, pending_url, connected_id, notice, settings_warning;
    int page = 0;
    int rendered_page = 0;
    bool notice_error = false;
    char server_name[129]{}, server_url[2049]{}, game_path[4097]{}, profile_name[129]{},
        mod_search[129]{};
    ImFont *body = nullptr, *heading = nullptr, *title = nullptr, *caption_font = nullptr;
    ID3D11ShaderResourceView *logo = nullptr;
    float logo_ratio = 1.667f;
    explicit App(fs::path data_path);
    void save();
    void scan();
    void tick();
    void draw(void *owner);
    Community *community();
    Profile &profile();
};
void theme();
} // namespace cd
