#include "http_fixture.hpp"
#include "ui.hpp"
#include "imgui_internal.h"
#include <algorithm>
#include <cfloat>
#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>

namespace {
struct Item {
    ImGuiID id = 0;
    ImRect rect;
    std::string label, window;
    bool disabled = false;
};
std::vector<Item> items;
std::unordered_map<ImGuiID, size_t> item_slots;
void require(bool ok, const std::string &message) {
    if (!ok)
        throw std::runtime_error(message);
}
void write(const cd::fs::path &p, const std::string &value) {
    cd::fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << value;
}
struct Workspace {
    cd::fs::path root, data, game;
    Workspace() {
        root = cd::fs::temp_directory_path() /
               ("community-divers-ui-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(cd::fs::create_directory(root), "Could not create a unique UI fixture");
        data = root / "launcher";
        game = root / "steamapps" / "common" / cd::from_utf8("Game-\xc3\x9c");
        write(game / "bin/helldivers2.exe", "");
        cd::fs::create_directories(game / "data");
        write(root / "steamapps/appmanifest_553850.acf",
              "AppState { appid 553850 buildid 12345 installdir \"Game-\xc3\x9c\" }");
        write(game / "data/game/game.dll", "engine fixture");
        write(game / "bin/PartyWin.dll", "party fixture");
        write(game / "bin/PlayFabMultiplayerWin.dll", "lobby fixture");
        cd::Settings settings;
        settings.game_path = cd::path_text(root / "missing");
        cd::save_settings(data / "launcher.json", settings);
        write(
            data / "mods" / cd::from_utf8("camera-\xc3\xa4") / "mod.json",
            R"({"schema":1,"id":"example.camera","name":"Camera test","version":"0.1.0","description":"Metadata-only UI fixture"})");
        write(data / "mods/broken/mod.json", "invalid manifest");
    }
    ~Workspace() {
        std::error_code error;
        auto parent = cd::fs::weakly_canonical(root.parent_path(), error);
        auto expected = cd::fs::weakly_canonical(cd::fs::temp_directory_path(), error);
        if (!error && parent == expected &&
            root.filename().string().starts_with("community-divers-ui-"))
            cd::fs::remove_all(root, error);
    }
};
struct UI {
    std::unique_ptr<cd::App> app;
    explicit UI(const cd::fs::path &data) {
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.DisplaySize = {1440, 900};
        io.DeltaTime = 1.f / 60;
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        wchar_t windows[32768]{};
        GetWindowsDirectoryW(windows, 32768);
        auto fonts = cd::fs::path(windows) / L"Fonts";
        auto font = [&](const wchar_t *name, float size) {
            auto p = fonts / name;
            if (cd::fs::exists(p))
                return io.Fonts->AddFontFromFileTTF(cd::path_text(p).c_str(), size);
            ImFontConfig cfg;
            cfg.SizePixels = size;
            return io.Fonts->AddFontDefault(&cfg);
        };
        app = std::make_unique<cd::App>(data);
        app->body = font(L"segoeui.ttf", 18);
        app->heading = font(L"bahnschrift.ttf", 28);
        app->title = font(L"bahnschrift.ttf", 36);
        app->caption_font = font(L"segoeui.ttf", 14);
        io.FontDefault = app->body;
        unsigned char *pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        io.Fonts->SetTexID((ImTextureID)1);
        ImGui::GetCurrentContext()->TestEngineHookItems = true;
        cd::theme();
        io.AddFocusEvent(true);
        frame();
        frame();
    }
    ~UI() {
        app.reset();
        ImGui::DestroyContext();
    }
    void frame() {
        items.clear();
        item_slots.clear();
        ImGui::NewFrame();
        app->draw(nullptr);
        ImGui::Render();
    }
    Item find(const std::string &label, size_t occurrence = 0) const {
        for (const auto &item : items)
            if (item.label == label) {
                if (occurrence-- == 0)
                    return item;
            }
        // BeginCombo registers a rectangle but does not emit an ItemInfo label.
        // Resolve that widget from its own recorded window ID, without coordinates.
        for (const auto &item : items) {
            if (!item.label.empty() || item.window.empty())
                continue;
            auto *window = ImGui::FindWindowByName(item.window.c_str());
            if (window && window->GetID(label.c_str()) == item.id && occurrence-- == 0)
                return item;
        }
        std::string known;
        for (const auto &item : items)
            if (!item.label.empty())
                known += item.label + " | ";
        throw std::runtime_error("Missing widget '" + label + "'. Visible labels: " + known);
    }
    void click(const std::string &label, size_t occurrence = 0) {
        auto item = find(label, occurrence);
        require(!item.disabled, "Attempted to activate disabled widget: " + label);
        auto center = item.rect.GetCenter();
        require(center.x >= 0 && center.x < ImGui::GetIO().DisplaySize.x && center.y >= 0 &&
                    center.y < ImGui::GetIO().DisplaySize.y,
                "Widget is outside the viewport: " + label);
        auto &io = ImGui::GetIO();
        io.AddMousePosEvent(center.x, center.y);
        frame();
        io.AddMouseButtonEvent(0, true);
        frame();
        io.AddMouseButtonEvent(0, false);
        frame();
        frame();
    }
    void key(ImGuiKey code) {
        auto &io = ImGui::GetIO();
        io.AddKeyEvent(code, true);
        frame();
        io.AddKeyEvent(code, false);
        frame();
        frame();
    }
    void type(const std::string &label, const std::string &value) {
        click(label);
        auto &io = ImGui::GetIO();
        io.AddKeyEvent(ImGuiMod_Ctrl, true);
        io.AddKeyEvent(ImGuiKey_A, true);
        frame();
        io.AddKeyEvent(ImGuiKey_A, false);
        io.AddKeyEvent(ImGuiMod_Ctrl, false);
        frame();
        io.AddInputCharactersUTF8(value.c_str());
        frame();
        frame();
    }
    void page(int page) {
        click("##section", static_cast<size_t>(page));
        require(app->page == page, "Section navigation did not change the page");
    }
    void wait_for_probe() {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
        while (app->pending.valid() && std::chrono::steady_clock::now() < deadline) {
            frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        require(!app->pending.valid(), "UI probe did not finish");
        frame();
    }
};
} // namespace
void ImGuiTestEngineHook_ItemAdd(ImGuiContext *ctx, ImGuiID id, const ImRect &rect,
                                 const ImGuiLastItemData *) {
    if (!id)
        return;
    auto [slot, inserted] = item_slots.emplace(id, items.size());
    if (inserted)
        items.push_back({id, rect, {}, {}, false});
    auto &item = items[slot->second];
    item.rect = rect;
    if (ctx->CurrentWindow)
        item.window = ctx->CurrentWindow->Name;
}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext *ctx, ImGuiID id, const char *label,
                                  ImGuiItemStatusFlags) {
    auto found = item_slots.find(id);
    if (found == item_slots.end())
        return;
    auto &item = items[found->second];
    item.label = label ? label : "";
    item.disabled = (ctx->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
}
void ImGuiTestEngineHook_Log(ImGuiContext *, const char *, ...) {
}
const char *ImGuiTestEngine_FindItemDebugLabel(ImGuiContext *, ImGuiID id) {
    auto found = item_slots.find(id);
    return found == item_slots.end() ? nullptr : items[found->second].label.c_str();
}
int main() {
    WSADATA sockets{};
    if (WSAStartup(MAKEWORD(2, 2), &sockets))
        return 1;
    try {
        Workspace fixture;
        {
            UI ui(fixture.data);
            ui.key(ImGuiKey_E);
            require(ui.app->page == 1, "E section shortcut failed");
            ui.key(ImGuiKey_Q);
            require(ui.app->page == 0, "Q section shortcut failed");
            ui.key(ImGuiKey_F1);
            require(ui.app->page == 5, "F1 About shortcut failed");
            std::cout << "PASS section keyboard shortcuts\n";
            ui.page(4);
            ui.type("##path", cd::path_text(fixture.game));
            ui.click("Use folder");
            require(ui.app->game.valid, "Typed installation path was not applied");
            require(ui.app->settings.game_path == cd::path_text(fixture.game),
                    "Unicode game directory was not preserved");
            require(ui.app->game.build == "12345", "Unicode game directory lost its Steam build");
            std::cout << "PASS manual installation entry and build identification\n";
            ui.click("Inspect client files");
            auto inspection_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
            while (ui.app->pending_identity.valid() &&
                   std::chrono::steady_clock::now() < inspection_deadline) {
                ui.frame();
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            require(!ui.app->pending_identity.valid(), "Client inspection did not finish");
            auto identity = cd::json::parse(cd::read_text(fixture.data / "client-inspection.json"));
            require(identity["files"].size() == 4 && identity["integration"]["available"] == false,
                    "Client inspection report is incomplete or grants launch");
            std::cout << "PASS asynchronous client inspection and report export\n";
            ui.page(2);
            ui.type("##name", "Local widget test");
            auto &io = ImGui::GetIO();
            io.AddKeyEvent(ImGuiKey_Q, true);
            io.AddInputCharactersUTF8("q");
            ui.frame();
            io.AddKeyEvent(ImGuiKey_Q, false);
            ui.frame();
            require(ui.app->page == 2, "Typing Q switched sections");
            ui.type("##name", "Local widget test");
            ui.key(ImGuiKey_Tab);
            require(ImGui::GetActiveID() == ui.find("##url").id,
                    "Tab did not move text focus to the URL field");
            ui.type("##url", "http://example.com");
            ui.click("Add community");
            require(ui.app->settings.communities.empty() && ui.app->notice_error,
                    "Invalid community URL was accepted");
            std::cout << "PASS form text entry and endpoint errors\n";
            cd::test::Server server(
                200,
                R"({"protocol":1,"id":"widget-test","name":"Widget service","game_builds":["12345"]})");
            ui.type("##url", server.url());
            ui.click("Add community");
            require(ui.app->settings.communities.size() == 1, "Add community button failed");
            require(cd::load_settings(fixture.data / "launcher.json").value.communities.size() == 1,
                    "Community was not saved");
            ui.click("Check selected");
            ui.wait_for_probe();
            require(ui.app->service && ui.app->service->name == "Widget service",
                    "Service result did not reach the UI");
            std::cout << "PASS add, save and check community through widgets\n";
            ui.page(3);
            ui.type("##profile", "Solo");
            ui.click("Create profile");
            require(ui.app->profile().name == "Solo", "Create profile button failed");
            require(ui.find("##enabled", 1).disabled,
                    "Malformed mod manifest remained interactive");
            ui.click("##enabled");
            require(ui.app->profile().enabled_mods == std::vector<std::string>{"example.camera"},
                    "Enable mod checkbox failed");
            auto disk = cd::load_settings(fixture.data / "launcher.json");
            require(disk.error.empty() &&
                        disk.value.profiles.back().enabled_mods == ui.app->profile().enabled_mods,
                    "Mod selection was not saved");
            ui.click("Profile");
            ui.click("Default");
            require(ui.app->profile().enabled_mods.empty(),
                    "Mod selection leaked into the default profile");
            std::cout << "PASS mod profiles, enable checkbox and profile isolation\n";
            ui.page(0);
            require(ui.find("LAUNCH GAME").disabled, "Unimplemented adapter can be launched");
            std::cout << "PASS game launch stays unavailable\n";
            ui.app->notice = "Shortcut not executed";
            io.AddKeyEvent(ImGuiMod_Ctrl, true);
            ui.key(ImGuiKey_S);
            io.AddKeyEvent(ImGuiMod_Ctrl, false);
            ui.frame();
            require(ui.app->notice == "Changes saved.", "Ctrl+S did not save settings");
            std::cout << "PASS keyboard save shortcut\n";
            ui.page(2);
            cd::test::Server slow(
                200,
                R"({"protocol":1,"id":"stale","name":"Stale service","game_builds":["12345"]})", {},
                std::chrono::milliseconds(250));
            ui.app->settings.communities.front().url = slow.url();
            ui.click("Check selected");
            ui.click("Remove selected");
            ui.wait_for_probe();
            require(ui.app->settings.communities.empty() && !ui.app->service,
                    "A stale service reply restored a removed community");
            std::cout << "PASS removed community discards in-flight status\n";
            ImGui::GetIO().DisplaySize = {1040, 730};
            ui.page(0);
            ui.frame();
            auto save = ui.find("CTRL+S  SAVE SETTINGS");
            require(save.rect.Max.y <= 730 && save.rect.Min.x >= 0,
                    "Footer is clipped at the minimum window size");
            for (size_t i = 0; i < 6; ++i)
                require(ui.find("##section", i).rect.Max.x <= 1040,
                        "Navigation exceeds minimum window width");
            std::cout << "PASS minimum-size navigation and footer layout\n";
        }
        {
            UI restarted(fixture.data);
            require(restarted.app->game.valid && restarted.app->game.build == "12345",
                    "Installation did not survive restart");
            require(restarted.app->settings.communities.empty() &&
                        restarted.app->settings.profiles.size() == 2,
                    "Saved launcher state did not survive restart");
            std::cout << "PASS launcher state after a fresh UI context\n";
        }
        WSACleanup();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        WSACleanup();
        return 1;
    }
}
