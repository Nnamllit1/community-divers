#include <windows.h>
#include "community/platform.hpp"
#include <algorithm>
#include <chrono>
#include <memory>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <thread>
#include <winhttp.h>
#include <wrl/client.h>
namespace cd {
using Microsoft::WRL::ComPtr;
std::wstring wide(const std::string &s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()),
                                nullptr, 0);
    if (!n)
        throw std::runtime_error("Invalid UTF-8 text");
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}
std::string utf8(const std::wstring &s) {
    if (s.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()),
                                nullptr, 0, nullptr, nullptr);
    if (!n)
        throw std::runtime_error("Invalid Unicode text");
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n, nullptr,
                        nullptr);
    return out;
}
std::string path_text(const fs::path &p) {
    return utf8(p.wstring());
}
fs::path user_data_path() {
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw)))
        throw std::runtime_error("Cannot locate LocalAppData");
    fs::path p(raw);
    CoTaskMemFree(raw);
    return p / L"CommunityDivers";
}
fs::path executable_directory() {
    std::wstring buffer(32768, L'\0');
    auto n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!n || n == buffer.size())
        throw std::runtime_error("Cannot locate the launcher executable");
    buffer.resize(n);
    return fs::path(buffer).parent_path();
}
std::vector<fs::path> steam_roots() {
    std::vector<fs::path> roots;
    for (const auto &key :
         {std::pair{HKEY_CURRENT_USER, L"Software\\Valve\\Steam"},
          std::pair{HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam"}}) {
        wchar_t data[32768]{};
        DWORD bytes = sizeof(data);
        const wchar_t *name = key.first == HKEY_CURRENT_USER ? L"SteamPath" : L"InstallPath";
        if (RegGetValueW(key.first, key.second, name, RRF_RT_REG_SZ, nullptr, data, &bytes) ==
            ERROR_SUCCESS)
            roots.emplace_back(data);
    }
    return roots;
}
std::optional<fs::path> choose_game_folder(void *owner) {
    ComPtr<IFileDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog))))
        throw std::runtime_error("Cannot open folder picker");
    DWORD flags = 0;
    dialog->GetOptions(&flags);
    dialog->SetOptions(flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(L"Choose your HELLDIVERS 2 installation");
    if (dialog->Show(static_cast<HWND>(owner)) == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return {};
    ComPtr<IShellItem> result;
    if (FAILED(dialog->GetResult(&result)))
        throw std::runtime_error("No game folder was selected");
    PWSTR raw = nullptr;
    if (FAILED(result->GetDisplayName(SIGDN_FILESYSPATH, &raw)))
        throw std::runtime_error("Folder is not a filesystem location");
    fs::path path(raw);
    CoTaskMemFree(raw);
    return path;
}
void open_folder(const fs::path &path) {
    fs::create_directories(path);
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
namespace {
struct HttpHandle {
    HINTERNET value = nullptr;
    explicit HttpHandle(HINTERNET h) : value(h) {
        if (!h)
            throw std::runtime_error("Windows HTTP error " + std::to_string(GetLastError()));
    }
    ~HttpHandle() {
        if (value)
            WinHttpCloseHandle(value);
    }
    HttpHandle(const HttpHandle &) = delete;
    operator HINTERNET() const { return value; }
};
void http_check(BOOL ok) {
    if (!ok)
        throw std::runtime_error("Connection failed (Windows HTTP " +
                                 std::to_string(GetLastError()) + ")");
}
std::string get_json(const std::string &url, const std::string &suffix, size_t limit) {
    auto e = parse_endpoint(url);
    HttpHandle session(WinHttpOpen(L"CommunityDivers/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    http_check(WinHttpSetTimeouts(session, 1500, 2000, 2000, 2000));
    HttpHandle connection(WinHttpConnect(session, wide(e.host).c_str(), e.port, 0));
    auto path = wide(e.path + suffix);
    HttpHandle query(WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        e.secure ? WINHTTP_FLAG_SECURE : 0));
    DWORD disabled = WINHTTP_DISABLE_AUTHENTICATION | WINHTTP_DISABLE_COOKIES;
    http_check(
        WinHttpSetOption(query, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)));
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    http_check(
        WinHttpSetOption(query, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)));
    http_check(WinHttpSendRequest(query, L"Accept: application/json\r\n", static_cast<DWORD>(-1),
                                  WINHTTP_NO_REQUEST_DATA, 0, 0, 0));
    http_check(WinHttpReceiveResponse(query, nullptr));
    DWORD status = 0, size = sizeof(status);
    http_check(WinHttpQueryHeaders(query, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                   WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                                   WINHTTP_NO_HEADER_INDEX));
    if (status != 200)
        throw std::runtime_error("Community returned HTTP " + std::to_string(status));
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
    std::string body;
    char buffer[8192];
    for (;;) {
        if (std::chrono::steady_clock::now() > deadline)
            throw std::runtime_error("Service response timed out");
        DWORD received = 0;
        http_check(WinHttpReadData(query, buffer, sizeof(buffer), &received));
        if (!received)
            break;
        if (body.size() + received > limit)
            throw std::runtime_error((limit == 262144 ? "Service response exceeds 256 KiB"
                                                      : "Campaign response exceeds 1 MiB"));
        body.append(buffer, received);
    }
    return body;
}
ProbeResult request(const std::string &url) {
    try {
        auto status = parse_status(get_json(url, "/v1/status", 262144));
        ProbeResult result{status, {}};
        if (std::find(status.capabilities.begin(), status.capabilities.end(), "campaign.read") !=
            status.capabilities.end()) {
            try {
                result.campaign =
                    parse_campaign(get_json(url, "/v1/campaign", 1024 * 1024), status.id);
            } catch (const std::exception &e) {
                result.campaign_error = e.what();
            }
        }
        return result;
    } catch (const std::exception &e) {
        return {{}, e.what()};
    }
}

} // namespace
std::future<ProbeResult> probe_service(std::string url) {
    auto promise = std::make_shared<std::promise<ProbeResult>>();
    auto future = promise->get_future();
    std::thread([promise, url = std::move(url)] {
        try {
            promise->set_value(request(url));
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }).detach();
    return future;
}
} // namespace cd
