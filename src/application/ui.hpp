#pragma once
#include "community/platform.hpp"
#include "note_transport.hpp"
#include "community/mod_loader.hpp"
#include "community/patch_notes_view.hpp"
#include "community/updates.hpp"
#include "community/installer.hpp"
#include <chrono>
#include "imgui.h"
#include <d3d11.h>
namespace cd {
struct App {
    fs::path data;
    ReleaseNotes notes;
    std::future<ReleaseNotes> pending_notes;
    std::map<std::string, NoteTexture> note_textures;
    std::function<NoteTexture(const std::string &)> note_image_loader;
    std::function<std::future<ReleaseNotes>(std::string)> notes_reader = fetch_launcher_notes;
    bool notes_open = false, notes_requested = false;
    uint64_t notes_generation = 0, pending_notes_generation = 0;
    std::string notes_title;
    void show_local_notes();
    void show_update_notes();
    void draw_notes();

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
    std::optional<CampaignSnapshot> campaign;
    std::future<ProbeResult> pending;
    std::function<std::future<ProbeResult>(std::string)> community_checker = probe_service;
    std::optional<PlayerProfile> player;
    std::optional<PlayerCredential> player_credential;
    std::future<PlayerResult> pending_player;
    std::function<std::future<PlayerResult>(std::string, PlayerCredential)> player_reader =
        request_player_profile_async;
    std::function<std::optional<fs::path>(void *)> credential_picker = choose_player_credential;
    std::function<std::optional<PlayerCredential>(const fs::path &, const std::string &,
                                                  const std::string &)>
        player_loader = saved_player;
    std::function<void(const fs::path &, const std::string &, const PlayerCredential &)>
        player_saver = remember_player;
    std::function<void(const fs::path &, const std::string &, const std::string &)>
        player_forgetter = forget_player;
    std::string connected_url, player_context, player_context_url, player_context_server;
    std::string player_message = "Check a community connection to sign in.";
    uint64_t player_generation = 0, pending_player_generation = 0;
    bool player_error = false, remember_sign_in = true, player_remembered = false;
    bool player_options_open = true;
    bool pending_player_remember = false, pending_player_forget = false,
         player_restore_requested = false;
    void sync_player_context();
    void begin_player_request(PlayerCredential credential, bool remember = false,
                              bool forget = false);
    void use_saved_player();
    void sign_out_player();
    void draw_player(void *owner);
    std::future<ClientIdentity> pending_identity;
    std::string identity_for_path, identity_summary;
    std::string pending_id, pending_url, connected_id, notice, settings_warning;
    int page = 0;
    bool notice_error = false;
    char server_name[129]{}, server_url[2049]{}, game_path[4097]{}, profile_name[129]{},
        mod_search[129]{};
    ImFont *body = nullptr, *strong = nullptr, *heading = nullptr, *title = nullptr,
           *caption_font = nullptr;
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
