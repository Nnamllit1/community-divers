#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "ui.hpp"
#include <algorithm>
#include <dwmapi.h>
#include <fstream>
#include <shellapi.h>
#include <stdexcept>
#include <wincodec.h>
#include <windows.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
namespace {
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> graphics_context;
ComPtr<IDXGISwapChain> swapchain;
ComPtr<ID3D11RenderTargetView> target;
UINT pending_width = 0, pending_height = 0;
void check(HRESULT result) {
    if (FAILED(result))
        throw std::runtime_error("Windows graphics error " +
                                 std::to_string(static_cast<unsigned long>(result)));
}
void render_target() {
    ComPtr<ID3D11Texture2D> back;
    check(swapchain->GetBuffer(0, IID_PPV_ARGS(&back)));
    check(device->CreateRenderTargetView(back.Get(), nullptr, &target));
}
void create_device(HWND window) {
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               levels, 2, D3D11_SDK_VERSION, &desc, &swapchain,
                                               &device, &level, &graphics_context);
    if (FAILED(hr))
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                                           D3D11_SDK_VERSION, &desc, &swapchain, &device, &level,
                                           &graphics_context);
    check(hr);
    render_target();
}
ComPtr<ID3D11ShaderResourceView> load_logo(const cd::fs::path &path, float &ratio) {
    ComPtr<IWICImagingFactory> factory;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&factory)));
    ComPtr<IWICBitmapDecoder> decoder;
    check(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                             WICDecodeMetadataCacheOnLoad, &decoder));
    ComPtr<IWICBitmapFrameDecode> frame;
    check(decoder->GetFrame(0, &frame));
    UINT w = 0, h = 0;
    check(frame->GetSize(&w, &h));
    if (!w || !h || w > 8192 || h > 8192)
        throw std::runtime_error("Invalid launcher logo size");
    ratio = static_cast<float>(w) / h;
    ComPtr<IWICFormatConverter> converter;
    check(factory->CreateFormatConverter(&converter));
    check(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                nullptr, 0, WICBitmapPaletteTypeCustom));
    std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 4);
    check(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(pixels.size()), pixels.data()));
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{pixels.data(), w * 4, 0};
    ComPtr<ID3D11Texture2D> texture;
    check(device->CreateTexture2D(&desc, &data, &texture));
    ComPtr<ID3D11ShaderResourceView> view;
    check(device->CreateShaderResourceView(texture.Get(), nullptr, &view));
    return view;
}
void capture(const cd::fs::path &path) {
    ComPtr<ID3D11Texture2D> back;
    check(swapchain->GetBuffer(0, IID_PPV_ARGS(&back)));
    D3D11_TEXTURE2D_DESC desc{};
    back->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    check(device->CreateTexture2D(&desc, nullptr, &staging));
    graphics_context->CopyResource(staging.Get(), back.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    check(graphics_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    std::vector<BYTE> pixels(static_cast<size_t>(desc.Width) * desc.Height * 4);
    for (UINT row = 0; row < desc.Height; ++row)
        memcpy(pixels.data() + static_cast<size_t>(row) * desc.Width * 4,
               static_cast<const BYTE *>(mapped.pData) + static_cast<size_t>(row) * mapped.RowPitch,
               desc.Width * 4);
    graphics_context->Unmap(staging.Get(), 0);
    if (!path.parent_path().empty())
        cd::fs::create_directories(path.parent_path());
    ComPtr<IWICImagingFactory> factory;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&factory)));
    ComPtr<IWICStream> stream;
    check(factory->CreateStream(&stream));
    check(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;
    check(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder));
    check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;
    check(encoder->CreateNewFrame(&frame, nullptr));
    check(frame->Initialize(nullptr));
    check(frame->SetSize(desc.Width, desc.Height));
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
    check(frame->SetPixelFormat(&format));
    if (IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA)) {
        for (size_t i = 0; i < pixels.size(); i += 4)
            std::swap(pixels[i], pixels[i + 2]);
    } else if (!IsEqualGUID(format, GUID_WICPixelFormat32bppRGBA))
        throw std::runtime_error("Unsupported PNG pixel format");
    check(frame->WritePixels(desc.Height, desc.Width * 4, static_cast<UINT>(pixels.size()),
                             pixels.data()));
    check(frame->Commit());
    check(encoder->Commit());
}
LRESULT WINAPI window_proc(HWND window, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(window, msg, wparam, lparam))
        return TRUE;
    switch (msg) {
    case WM_SIZE:
        if (wparam != SIZE_MINIMIZED) {
            pending_width = LOWORD(lparam);
            pending_height = HIWORD(lparam);
        }
        return 0;
    case WM_GETMINMAXINFO: {
        auto p = reinterpret_cast<MINMAXINFO *>(lparam);
        p->ptMinTrackSize = {1040, 730};
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wparam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED: {
        auto rect = reinterpret_cast<RECT *>(lparam);
        SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left,
                     rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    }
    return DefWindowProcW(window, msg, wparam, lparam);
}
} // namespace
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    cd::fs::path capture_path, data;
    int page = 0;
    int window_width = 1320, window_height = 930;
    try {
        check(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
        int count = 0;
        auto args = CommandLineToArgvW(GetCommandLineW(), &count);
        for (int i = 1; i < count; ++i) {
            std::wstring arg = args[i];
            if (arg == L"--capture" && i + 1 < count)
                capture_path = args[++i];
            else if (arg == L"--data-dir" && i + 1 < count)
                data = args[++i];
            else if (arg == L"--window-size" && i + 2 < count) {
                window_width = std::clamp(_wtoi(args[++i]), 1040, 3840);
                window_height = std::clamp(_wtoi(args[++i]), 730, 2160);
            } else if (arg == L"--page" && i + 1 < count)
                page = std::clamp(_wtoi(args[++i]), 0, 5);
        }
        LocalFree(args);
        if (data.empty())
            data = cd::user_data_path();
        ImGui_ImplWin32_EnableDpiAwareness();
        WNDCLASSEXW wc{sizeof(wc),
                       CS_CLASSDC,
                       window_proc,
                       0,
                       0,
                       instance,
                       LoadIconW(instance, MAKEINTRESOURCEW(1)),
                       LoadCursorW(nullptr, IDC_ARROW),
                       nullptr,
                       nullptr,
                       L"CommunityDiversLauncher",
                       nullptr};
        if (!RegisterClassExW(&wc))
            throw std::runtime_error("Cannot register launcher window");
        HWND window = CreateWindowW(wc.lpszClassName, L"Community Divers", WS_OVERLAPPEDWINDOW,
                                    CW_USEDEFAULT, CW_USEDEFAULT, window_width, window_height,
                                    nullptr, nullptr, instance, nullptr);
        if (!window)
            throw std::runtime_error("Cannot create launcher window");
        BOOL dark = TRUE;
        DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
        create_device(window);
        ShowWindow(window, capture_path.empty() ? show : SW_SHOWNOACTIVATE);
        UpdateWindow(window);
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;
        cd::theme();
        cd::App app(data);
        app.page = page;
        wchar_t windows[32768]{};
        GetWindowsDirectoryW(windows, 32768);
        auto fonts = cd::fs::path(windows) / L"Fonts";
        auto font = [&](const wchar_t *file, float size) {
            auto path = fonts / file;
            auto encoded = cd::path_text(path);
            if (cd::fs::exists(path))
                return io.Fonts->AddFontFromFileTTF(encoded.c_str(), size);
            ImFontConfig config;
            config.SizePixels = size;
            return io.Fonts->AddFontDefault(&config);
        };
        app.body = font(L"bahnschrift.ttf", 18);
        app.heading = font(L"bahnschrift.ttf", 28);
        app.title = font(L"seguisb.ttf", 38);
        app.caption_font = font(L"consola.ttf", 14);
        io.FontDefault = app.body;
        auto logo =
            load_logo(cd::executable_directory() / L"assets/brand/logo.png", app.logo_ratio);
        app.logo = logo.Get();
        if (!ImGui_ImplWin32_Init(window) ||
            !ImGui_ImplDX11_Init(device.Get(), graphics_context.Get()))
            throw std::runtime_error("Cannot initialize launcher renderer");
        bool done = false;
        unsigned frames = 0;
        while (!done) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
                if (msg.message == WM_QUIT)
                    done = true;
            }
            if (done)
                break;
            if (pending_width && pending_height) {
                target.Reset();
                check(swapchain->ResizeBuffers(0, pending_width, pending_height,
                                               DXGI_FORMAT_UNKNOWN, 0));
                pending_width = pending_height = 0;
                render_target();
            }
            if (IsIconic(window)) {
                Sleep(20);
                continue;
            }
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            app.draw(window);
            ImGui::Render();
            float clear[] = {.04f, .065f, .09f, 1};
            ID3D11RenderTargetView *view = target.Get();
            graphics_context->OMSetRenderTargets(1, &view, nullptr);
            graphics_context->ClearRenderTargetView(target.Get(), clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            if (!capture_path.empty() && ++frames >= 4) {
                capture(capture_path);
                done = true;
            }
            check(swapchain->Present(1, 0));
        }
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        logo.Reset();
        target.Reset();
        swapchain.Reset();
        graphics_context.Reset();
        device.Reset();
        DestroyWindow(window);
        UnregisterClassW(wc.lpszClassName, instance);
        CoUninitialize();
        return 0;
    } catch (const std::exception &e) {
        try {
            if (data.empty())
                data = cd::user_data_path();
            cd::fs::create_directories(data);
            std::ofstream(data / "launcher-error.log", std::ios::app) << e.what() << '\n';
        } catch (...) {
        }
        if (capture_path.empty())
            MessageBoxW(nullptr, cd::wide(e.what()).c_str(), L"Community Divers could not start",
                        MB_ICONERROR | MB_OK);
        return 1;
    }
}
