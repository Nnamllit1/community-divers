#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wincodec.h>
#include <wrl/client.h>
#include "setup_bundle.hpp"
#include "setup_registration.hpp"
#include "community/platform.hpp"
#include "community/version.hpp"
#include <array>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>

namespace {
using namespace cd;
using Microsoft::WRL::ComPtr;
constexpr int destination_id = 101, browse_id = 102, primary_id = 103, cancel_id = 104;
constexpr COLORREF background = RGB(10, 16, 23), panel = RGB(18, 28, 38), field = RGB(27, 39, 51),
                   foreground = RGB(235, 239, 243), muted = RGB(155, 171, 185),
                   accent = RGB(255, 222, 64);
struct Options {
    fs::path self, destination, capture, package, output;
    bool silent = false, uninstall = false, worker = false, run = false, qa = false,
         interrupt = false, dirty = false;
    DWORD parent = 0;
    uint64_t parent_created = 0;
};
struct State {
    Options options;
    setup::Bundle bundle;
    HWND window = nullptr, edit = nullptr, browse = nullptr, primary = nullptr, cancel = nullptr;
    HFONT body = nullptr, heading = nullptr, caption = nullptr;
    HBRUSH field_brush = CreateSolidBrush(field);
    HICON logo = nullptr;
    float scale = 1;
    bool busy = false, complete = false, failed = false, started = false;
    unsigned frames = 0;
    std::future<void> job;
    std::mutex mutex;
    size_t done = 0, total = 1;
    std::string status, error;
    ~State() {
        if (job.valid())
            job.wait();
        DeleteObject(body);
        DeleteObject(heading);
        DeleteObject(caption);
        DeleteObject(field_brush);
        if (logo)
            DestroyIcon(logo);
    }
    int px(int n) const { return static_cast<int>(n * scale); }
};
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
fs::path own_executable() {
    wchar_t path[32768]{};
    auto size = GetModuleFileNameW(nullptr, path, 32768);
    check(size > 0 && size < 32768, "Cannot locate setup executable");
    return path;
}
uint64_t creation(HANDLE process) {
    FILETIME start{}, exit{}, kernel{}, user{};
    check(GetProcessTimes(process, &start, &exit, &kernel, &user) != FALSE,
          "Cannot identify uninstall parent");
    return (uint64_t(start.dwHighDateTime) << 32) | start.dwLowDateTime;
}
void wait_parent(const Options &options) {
    HANDLE parent =
        OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, options.parent);
    if (!parent) {
        check(GetLastError() == ERROR_INVALID_PARAMETER, "Cannot inspect uninstall parent");
        return;
    }
    bool same = creation(parent) == options.parent_created;
    DWORD result = same ? WaitForSingleObject(parent, 15000) : WAIT_OBJECT_0;
    CloseHandle(parent);
    check(result == WAIT_OBJECT_0, "Previous uninstaller is still running");
}
void handoff_uninstall(const Options &options) {
    auto work = setup::create_setup_workspace();
    auto helper = work / L"UninstallHelper.exe";
    fs::copy_file(options.self, helper);
    auto command = L"\"" + helper.native() + L"\" --uninstall-dir \"" +
                   options.destination.native() + L"\" --uninstall-worker --wait-parent " +
                   std::to_wstring(GetCurrentProcessId()) + L" " +
                   std::to_wstring(creation(GetCurrentProcess()));
    if (options.silent)
        command += L" --silent";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    check(CreateProcessW(helper.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                         work.c_str(), &startup, &process) != FALSE,
          "Cannot start the separate uninstall helper");
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}
void perform(State &state) {
    auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    check(SUCCEEDED(initialized), "Cannot initialize Windows setup services");
    try {
        if (state.options.uninstall) {
            auto key = setup_registration_key(state.options.destination);
            check(key == state.bundle.key(),
                  "Uninstaller and installed application identities differ");
            auto report = uninstall_managed_launcher(state.options.destination);
            setup::unregister_application(state.options.destination, key, state.bundle.label());
            RemoveDirectoryW(state.options.destination.c_str());
            std::lock_guard lock(state.mutex);
            state.status = std::to_string(report.deleted) +
                           " launcher files removed. Profiles, mods and game copies are kept.";
            if (report.preserved)
                state.status += " Modified launcher files were also kept.";
            state.done = state.total = 1;
        } else {
            setup::check_registration(state.options.destination, state.bundle.key());
            auto staged = setup::stage_bundle(state.options.self);
            install_setup_payload(
                staged->payload, state.options.destination, state.options.self, state.bundle.key(),
                [&](size_t done, size_t total, const std::string &file) {
                    std::lock_guard lock(state.mutex);
                    state.done = done;
                    state.total = total;
                    state.status = "Installing " + file;
                },
                state.bundle.interrupt);
            setup::register_application(state.options.destination, state.bundle.key(),
                                        state.bundle.label());
            std::lock_guard lock(state.mutex);
            state.status =
                "Open the launcher from your Start menu. Updates are managed in the launcher.";
        }
        CoUninitialize();
    } catch (...) {
        CoUninitialize();
        throw;
    }
}
void begin(State &state) {
    if (state.busy)
        return;
    if (!state.options.uninstall) {
        std::array<wchar_t, 32768> text{};
        GetWindowTextW(state.edit, text.data(), static_cast<int>(text.size()));
        state.options.destination = fs::path(text.data());
    }
    state.error.clear();
    state.failed = false;
    state.busy = true;
    state.started = true;
    state.done = 0;
    state.total = 1;
    state.status = state.options.uninstall ? "Checking the installed launcher..."
                                           : "Verifying the installation package...";
    state.job = std::async(std::launch::async, [&state] { perform(state); });
    EnableWindow(state.edit, FALSE);
    EnableWindow(state.browse, FALSE);
    EnableWindow(state.primary, FALSE);
    EnableWindow(state.cancel, FALSE);
    InvalidateRect(state.window, nullptr, FALSE);
}
void fill(HDC dc, RECT rectangle, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rectangle, brush);
    DeleteObject(brush);
}
void text(State &state, HDC dc, const std::wstring &value, int x, int y, int width, int height,
          HFONT font, COLORREF color = foreground,
          UINT flags = DT_LEFT | DT_WORDBREAK | DT_NOPREFIX) {
    RECT r{state.px(x), state.px(y), state.px(x + width), state.px(y + height)};
    SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &r, flags);
}
void paint(State &s, HDC dc) {
    RECT rect{};
    GetClientRect(s.window, &rect);
    int width = static_cast<int>(rect.right / s.scale),
        height = static_cast<int>(rect.bottom / s.scale);
    fill(dc, rect, background);
    fill(dc, {0, 0, s.px(248), rect.bottom}, panel);
    fill(dc, {s.px(248), 0, s.px(250), rect.bottom}, RGB(45, 60, 73));
    DrawIconEx(dc, s.px(56), s.px(38), s.logo, s.px(136), s.px(136), 0, nullptr, DI_NORMAL);
    text(s, dc, L"COMMUNITY\nDIVERS", 28, 196, 200, 94, s.heading);
    text(s, dc, L"YOUR CAMPAIGN.\nYOUR COMMUNITY.", 28, 302, 200, 58, s.caption, muted);
    const wchar_t *steps[] = {L"01   DESTINATION", L"02   INSTALLATION", L"03   READY"};
    int selected = s.complete ? 2 : s.busy ? 1 : 0;
    for (int i = 0; i < 3; ++i) {
        if (i == selected)
            fill(dc, {s.px(24), s.px(390 + i * 42), s.px(224), s.px(424 + i * 42)}, field);
        auto label = s.options.uninstall ? (i == 0   ? L"01   REVIEW"
                                            : i == 1 ? L"02   REMOVAL"
                                                     : L"03   FINISHED")
                                         : steps[i];
        text(s, dc, label, 34, 398 + i * 42, 190, 28, s.caption, i == selected ? accent : muted);
    }
    text(s, dc, L"WINDOWS  /  PERSONAL", 28, height - 48, 205, 30, s.caption, muted);
    text(s, dc, wide("SETUP  /  " + std::string(version::release_id)), 290, 30, width - 330, 30,
         s.caption, muted);
    fill(dc, {s.px(290), s.px(76), rect.right - s.px(40), s.px(79)}, accent);
    const auto title =
        s.failed     ? L"Setup needs your attention"
        : s.complete ? (s.options.uninstall ? L"Launcher removed" : L"Ready for your next drop")
        : s.busy ? (s.options.uninstall ? L"Removing the launcher" : L"Installing Community Divers")
                 : (s.options.uninstall ? L"Remove Community Divers" : L"Install Community Divers");
    text(s, dc, title, 290, 106, width - 330, 88, s.heading);
    const auto description = s.options.uninstall
                                 ? L"Remove the launcher from this Windows account. Your profiles, "
                                   L"mods, campaigns and game copies stay on this PC."
                                 : L"Your starting point for community campaigns and mods. Install "
                                   L"for this Windows account. No administrator access is needed.";
    text(s, dc, description, 290, 200, width - 330, 85, s.body, muted);
    if (!s.busy && !s.complete && !s.failed) {
        text(s, dc, s.options.uninstall ? L"INSTALLED LOCATION" : L"INSTALLATION FOLDER", 290, 300,
             width - 330, 28, s.caption);
        if (s.options.uninstall)
            text(s, dc, s.options.destination.native(), 290, 339, width - 330, 70, s.body);
        text(s, dc,
             s.options.uninstall
                 ? L"Only unchanged launcher-owned files are removed."
                 : L"Run setup again in this folder to repair an existing installation.",
             290, 400, width - 330, 55, s.body, muted);
    } else {
        size_t done, total;
        std::string status;
        {
            std::lock_guard lock(s.mutex);
            done = s.done;
            total = s.total;
            status = s.status;
        }
        if (s.failed) {
            text(s, dc, wide(s.error), 290, 296, width - 330, height - 420, s.body,
                 RGB(255, 179, 132));
        } else {
            fill(dc, {s.px(290), s.px(312), rect.right - s.px(40), s.px(320)}, field);
            int progress =
                static_cast<int>((width - 330) * (double(done) / (std::max)(size_t(1), total)));
            fill(dc, {s.px(290), s.px(312), s.px(290 + progress), s.px(320)}, accent);
            text(s, dc, wide(status), 290, 344, width - 330, 120, s.body, muted);
        }
    }
    fill(dc, {s.px(290), rect.bottom - s.px(108), rect.right - s.px(40), rect.bottom - s.px(107)},
         RGB(45, 60, 73));
}
void layout(State &s) {
    RECT r{};
    GetClientRect(s.window, &r);
    MoveWindow(s.edit, s.px(290), s.px(338), r.right - s.px(470), s.px(42), TRUE);
    MoveWindow(s.browse, r.right - s.px(164), s.px(338), s.px(124), s.px(42), TRUE);
    MoveWindow(s.primary, r.right - s.px(224), r.bottom - s.px(82), s.px(184), s.px(48), TRUE);
    MoveWindow(s.cancel, s.px(290), r.bottom - s.px(82), s.px(132), s.px(48), TRUE);
    ShowWindow(s.edit,
               !s.options.uninstall && !s.busy && !s.complete && !s.failed ? SW_SHOW : SW_HIDE);
    ShowWindow(s.browse,
               !s.options.uninstall && !s.busy && !s.complete && !s.failed ? SW_SHOW : SW_HIDE);
}
void fonts(State &s) {
    s.scale = static_cast<float>(GetDpiForWindow(s.window)) / 96;
    DeleteObject(s.body);
    DeleteObject(s.heading);
    DeleteObject(s.caption);
    auto create = [&](int size, int weight, const wchar_t *face) {
        return CreateFontW(-s.px(size), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH, face);
    };
    s.body = create(17, FW_NORMAL, L"Segoe UI");
    s.caption = create(13, FW_SEMIBOLD, L"Segoe UI");
    s.heading = create(31, FW_SEMIBOLD, L"Bahnschrift");
    for (auto child : {s.edit, s.browse, s.primary, s.cancel})
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(s.body), TRUE);
}
void browse(State &s) {
    ComPtr<IFileDialog> dialog;
    check(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                     IID_PPV_ARGS(&dialog))),
          "Cannot open folder picker");
    DWORD flags = 0;
    dialog->GetOptions(&flags);
    dialog->SetOptions(flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dialog->SetTitle(L"Choose a folder for Community Divers");
    if (dialog->Show(s.window) == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return;
    ComPtr<IShellItem> item;
    PWSTR raw = nullptr;
    check(SUCCEEDED(dialog->GetResult(&item)) &&
              SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw)),
          "Cannot read selected folder");
    SetWindowTextW(s.edit, raw);
    CoTaskMemFree(raw);
}
void capture(State &s) {
    RECT r{};
    GetClientRect(s.window, &r);
    HDC window = GetDC(s.window), memory = CreateCompatibleDC(window);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = r.right;
    info.bmiHeader.biHeight = -r.bottom;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(window, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    check(bitmap && memory, "Cannot allocate setup capture");
    auto previous = SelectObject(memory, bitmap);
    paint(s, memory);
    // Render our own child controls in client coordinates. WM_PRINT on the top-level
    // window includes a nonclient offset on some Windows themes even without PRF_NONCLIENT.
    for (auto child : {s.edit, s.browse, s.primary, s.cancel}) {
        if (!IsWindowVisible(child))
            continue;
        RECT child_rect{};
        GetWindowRect(child, &child_rect);
        MapWindowPoints(nullptr, s.window, reinterpret_cast<POINT *>(&child_rect), 2);
        int saved = SaveDC(memory);
        SetViewportOrgEx(memory, child_rect.left, child_rect.top, nullptr);
        IntersectClipRect(memory, 0, 0, child_rect.right - child_rect.left,
                          child_rect.bottom - child_rect.top);
        SendMessageW(child, WM_PRINT, reinterpret_cast<WPARAM>(memory),
                     PRF_CLIENT | PRF_NONCLIENT | PRF_ERASEBKGND);
        RestoreDC(memory, saved);
    }
    auto *bytes = static_cast<BYTE *>(pixels);
    for (size_t i = 3; i < static_cast<size_t>(r.right) * r.bottom * 4; i += 4)
        bytes[i] = 255;
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    auto ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                         IID_PPV_ARGS(&factory))) &&
              SUCCEEDED(factory->CreateStream(&stream)) &&
              SUCCEEDED(stream->InitializeFromFilename(s.options.capture.c_str(), GENERIC_WRITE)) &&
              SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
              SUCCEEDED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) &&
              SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) &&
              SUCCEEDED(frame->Initialize(nullptr)) && SUCCEEDED(frame->SetSize(r.right, r.bottom));
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    ok = ok && SUCCEEDED(frame->SetPixelFormat(&format)) &&
         IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA) &&
         SUCCEEDED(frame->WritePixels(r.bottom, r.right * 4, r.right * r.bottom * 4, bytes)) &&
         SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
    SelectObject(memory, previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(s.window, window);
    check(ok, "Cannot save setup capture");
}
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto *s = reinterpret_cast<State *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        s = static_cast<State *>(reinterpret_cast<CREATESTRUCTW *>(lparam)->lpCreateParams);
        s->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
    }
    if (!s)
        return DefWindowProcW(window, message, wparam, lparam);
    try {
        switch (message) {
        case WM_CREATE: {
            auto child = [&](const wchar_t *type, const wchar_t *label, DWORD style, int id) {
                return CreateWindowExW(0, type, label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                                       0, 0, 0, 0, window,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                       GetModuleHandleW(nullptr), nullptr);
            };
            s->edit = child(L"EDIT", s->options.destination.c_str(), ES_AUTOHSCROLL | WS_BORDER,
                            destination_id);
            SendMessageW(s->edit, EM_SETLIMITTEXT, 32760, 0);
            s->browse = child(L"BUTTON", L"&Browse...", BS_OWNERDRAW, browse_id);
            s->primary =
                child(L"BUTTON", s->options.uninstall ? L"&Remove launcher" : L"&Install launcher",
                      BS_OWNERDRAW, primary_id);
            s->cancel = child(L"BUTTON", L"&Cancel", BS_OWNERDRAW, cancel_id);
            s->logo = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                                    IMAGE_ICON, 256, 256, 0));
            fonts(*s);
            layout(*s);
            SetTimer(window, 1, 50, nullptr);
            return 0;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
            SetTextColor(reinterpret_cast<HDC>(wparam), foreground);
            SetBkColor(reinterpret_cast<HDC>(wparam), field);
            return reinterpret_cast<LRESULT>(s->field_brush);
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            auto dc = BeginPaint(window, &ps);
            paint(*s, dc);
            EndPaint(window, &ps);
            return 0;
        }
        case WM_PRINTCLIENT:
            paint(*s, reinterpret_cast<HDC>(wparam));
            return 0;
        case WM_DRAWITEM: {
            auto *item = reinterpret_cast<DRAWITEMSTRUCT *>(lparam);
            bool primary = item->CtlID == primary_id,
                 disabled = (item->itemState & ODS_DISABLED) != 0;
            fill(item->hDC, item->rcItem, disabled ? field : primary ? accent : field);
            SetBkMode(item->hDC, TRANSPARENT);
            SetTextColor(item->hDC, disabled ? muted : primary ? background : foreground);
            SelectObject(item->hDC, s->body);
            wchar_t label[100]{};
            GetWindowTextW(item->hwndItem, label, 100);
            auto r = item->rcItem;
            if (item->itemState & ODS_SELECTED)
                OffsetRect(&r, 1, 1);
            DrawTextW(item->hDC, label, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (item->itemState & ODS_FOCUS) {
                InflateRect(&r, -4, -4);
                DrawFocusRect(item->hDC, &r);
            }
            return TRUE;
        }
        case WM_SIZE:
            layout(*s);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        case WM_DPICHANGED: {
            auto r = reinterpret_cast<RECT *>(lparam);
            fonts(*s);
            SetWindowPos(window, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            layout(*s);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto *info = reinterpret_cast<MINMAXINFO *>(lparam);
            info->ptMinTrackSize = {s->px(930), s->px(680)};
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wparam) == browse_id && !s->busy)
                browse(*s);
            else if (LOWORD(wparam) == cancel_id || LOWORD(wparam) == IDCANCEL)
                SendMessageW(window, WM_CLOSE, 0, 0);
            else if ((LOWORD(wparam) == primary_id || LOWORD(wparam) == IDOK) && !s->busy) {
                if (s->complete) {
                    if (!s->options.uninstall)
                        check(reinterpret_cast<INT_PTR>(ShellExecuteW(
                                  window, L"open",
                                  (s->options.destination / L"CommunityDivers.exe").c_str(),
                                  nullptr, s->options.destination.c_str(), SW_SHOWNORMAL)) > 32,
                              "Could not start the launcher");
                    DestroyWindow(window);
                } else
                    begin(*s);
            }
            return 0;
        case WM_TIMER:
            if (s->options.run && !s->started)
                begin(*s);
            if (s->busy && s->job.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                s->busy = false;
                try {
                    s->job.get();
                    s->complete = true;
                } catch (const std::exception &e) {
                    s->error = e.what();
                    s->failed = true;
                }
                EnableWindow(s->primary, TRUE);
                EnableWindow(s->cancel, TRUE);
                EnableWindow(s->edit, TRUE);
                EnableWindow(s->browse, TRUE);
                SetWindowTextW(s->primary, s->failed              ? L"&Retry"
                                           : s->options.uninstall ? L"&Close"
                                                                  : L"&Launch");
                SetWindowTextW(s->cancel, L"&Close");
                layout(*s);
                SetFocus(s->primary);
            }
            if (s->busy)
                layout(*s);
            InvalidateRect(window, nullptr, FALSE);
            if (!s->options.capture.empty() && ++s->frames >= 5 &&
                (!s->options.run || s->complete || s->failed)) {
                UpdateWindow(window);
                capture(*s);
                DestroyWindow(window);
            }
            return 0;
        case WM_CLOSE:
            if (!s->busy)
                DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            KillTimer(window, 1);
            PostQuitMessage(s->failed ? 1 : 0);
            return 0;
        }
    } catch (const std::exception &e) {
        s->error = e.what();
        s->failed = true;
        s->options.capture.clear();
        InvalidateRect(window, nullptr, FALSE);
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    bool silent = false;
    try {
        check(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)),
              "Cannot initialize setup");
        Options options;
        options.self = own_executable();
        int count = 0;
        auto args = CommandLineToArgvW(GetCommandLineW(), &count);
        for (int i = 1; i < count; ++i) {
            std::wstring arg = args[i];
            auto next = [&]() -> std::wstring {
                check(i + 1 < count, "Missing setup argument");
                return args[++i];
            };
            if (arg == L"--silent")
                options.silent = silent = true;
            else if (arg == L"--install-dir")
                options.destination = next();
            else if (arg == L"--uninstall-dir") {
                options.destination = next();
                options.uninstall = true;
            } else if (arg == L"--uninstall-worker")
                options.worker = true;
            else if (arg == L"--wait-parent") {
                options.parent = std::stoul(next());
                options.parent_created = std::stoull(next());
            } else if (arg == L"--capture")
                options.capture = next();
            else if (arg == L"--run")
                options.run = true;
            else if (arg == L"--pack") {
                options.package = next();
                silent = true;
            } else if (arg == L"--output")
                options.output = next();
            else if (arg == L"--qa")
                options.qa = true;
            else if (arg == L"--interrupt-fixture")
                options.interrupt = true;
            else if (arg == L"--allow-dirty")
                options.dirty = true;
            else
                throw std::runtime_error("Unrecognized setup option");
        }
        LocalFree(args);
        if (!options.package.empty()) {
            check(!options.output.empty(), "Missing setup output path");
            setup::pack_bundle(options.self, options.package, options.output, options.qa,
                               options.interrupt, options.dirty);
            return 0;
        }
        auto bundle = setup::inspect_bundle(options.self);
        if (_wcsicmp(options.self.filename().c_str(), L"Uninstall.exe") == 0) {
            options.uninstall = true;
            if (options.destination.empty())
                options.destination = options.self.parent_path();
        }
        if (options.destination.empty()) {
            options.destination = setup::registered_path(bundle.key());
            if (options.destination.empty())
                options.destination =
                    user_data_path().parent_path() / L"Programs" / from_utf8(bundle.key());
        }
        if (options.parent) {
            check(options.worker && options.uninstall, "Unexpected parent wait");
            wait_parent(options);
        }
        if (options.uninstall && fs::equivalent(options.self.parent_path(), options.destination)) {
            check(!options.worker, "Uninstall worker must run outside the installed application");
            handoff_uninstall(options);
            return 0;
        }
        State state;
        state.options = options;
        state.bundle = bundle;
        if (options.silent) {
            perform(state);
            return 0;
        }
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        WNDCLASSEXW cls{};
        cls.cbSize = sizeof(cls);
        cls.lpfnWndProc = procedure;
        cls.hInstance = instance;
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        cls.lpszClassName = L"CommunityDiversNativeSetup";
        cls.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
        RegisterClassExW(&cls);
        HWND window =
            CreateWindowExW(WS_EX_CONTROLPARENT, cls.lpszClassName, L"Community Divers — Setup",
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1000, 730, nullptr,
                            nullptr, instance, &state);
        check(window != nullptr, "Cannot create setup window");
        BOOL dark = TRUE;
        DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        ShowWindow(window, show);
        UpdateWindow(window);
        SetFocus(state.primary);
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            if (!IsDialogMessageW(window, &message)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        return static_cast<int>(message.wParam);
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        if (!silent)
            MessageBoxW(nullptr, cd::wide(e.what()).c_str(), L"Community Divers setup",
                        MB_OK | MB_ICONERROR);
        return 1;
    }
}
