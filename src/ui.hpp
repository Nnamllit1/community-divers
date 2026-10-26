#pragma once
#include "community/platform.hpp"
#include "community/updates.hpp"
#include "community/installer.hpp"
#include <chrono>
#include "imgui.h"
#include <d3d11.h>
namespace cd {
struct App {
    fs::path data;
    Settings settings;
    bool background_updates;
    std::future<UpdateCheck> pending_update;
    std::future<UpdateDownload> pending_download;
    std::optional<UpdateRelease> update_release;
    fs::path downloaded_update;
    std::string update_envelope, downloaded_envelope;
    std::future<InstallHandoff> pending_install;
    bool exit_for_update = false;
    bool portable_install = false;
    std::function<void(const UpdateRelease &, const std::string &, const fs::path &)>
        update_recorder = [](const auto &release, const auto &envelope, const auto &data) {
            remember_update_channel(release, envelope, data);
        };
    std::function<std::future<InstallHandoff>(fs::path, std::string, fs::path, fs::path)>
        update_installer = start_update_install_async;
    std::string update_message = "Update check has not run.";
    bool update_error = false;
    std::chrono::steady_clock::time_point next_update_check{};
    std::function<std::future<UpdateCheck>()> update_checker = check_update_async;
    std::function<std::future<UpdateDownload>(UpdateRelease, fs::path)> update_downloader =
        stage_update_async;
    Installation game;
    std::vector<Mod> mods;
    std::optional<ServiceStatus> service;
    std::future<ProbeResult> pending;
    std::future<ClientIdentity> pending_identity;
    std::string identity_for_path, identity_summary;
    std::string pending_id, pending_url, connected_id, notice, settings_warning;
    int page = 0;
    bool notice_error = false;
    char server_name[129]{}, server_url[2049]{}, game_path[4097]{}, profile_name[129]{},
        mod_search[129]{};
    ImFont *body = nullptr, *heading = nullptr, *title = nullptr, *caption_font = nullptr;
    ID3D11ShaderResourceView *logo = nullptr;
    float logo_ratio = 1.667f;
    explicit App(fs::path data_path, bool allow_background_updates = true);
    void check_for_updates();
    void save();
    void scan();
    void tick();
    void draw(void *owner);
    Community *community();
    Profile &profile();
};
void theme();
} // namespace cd
