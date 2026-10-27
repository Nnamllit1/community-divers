#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include "setup_registration.hpp"
#include "community/platform.hpp"
#include "community/version.hpp"
namespace cd::setup {
namespace {
using Microsoft::WRL::ComPtr;
void need(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
std::wstring registry_path(const std::string &key) {
    need(key == "CommunityDivers" || key == "CommunityDivers-QA", "Invalid setup registration");
    return wide("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + key);
}
fs::path shortcut_path(const std::string &label) {
    PWSTR raw = nullptr;
    need(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Programs, 0, nullptr, &raw)),
         "Cannot find Start menu");
    fs::path directory(raw);
    CoTaskMemFree(raw);
    return directory / from_utf8(label + ".lnk");
}
bool matching_shortcut(const fs::path &path, const fs::path &root) {
    if (!fs::exists(path))
        return true;
    ComPtr<IShellLinkW> link;
    ComPtr<IPersistFile> persist;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&link))) ||
        FAILED(link.As(&persist)) || FAILED(persist->Load(path.c_str(), STGM_READ)))
        return false;
    wchar_t target[32768]{};
    if (FAILED(link->GetPath(target, 32768, nullptr, SLGP_RAWPATH)))
        return false;
    return _wcsicmp(fs::path(target).lexically_normal().c_str(),
                    (root / "CommunityDivers.exe").c_str()) == 0;
}
} // namespace
fs::path registered_path(const std::string &key) {
    wchar_t text[32768]{};
    DWORD bytes = sizeof(text);
    auto status = RegGetValueW(HKEY_CURRENT_USER, registry_path(key).c_str(), L"InstallLocation",
                               RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, text, &bytes);
    if (status == ERROR_FILE_NOT_FOUND)
        return {};
    need(status == ERROR_SUCCESS, "Cannot read installed application location");
    return text;
}
void check_registration(const fs::path &root, const std::string &key) {
    auto existing = registered_path(key);
    need(existing.empty() ||
             _wcsicmp(existing.lexically_normal().c_str(), root.lexically_normal().c_str()) == 0,
         "Community Divers is already installed elsewhere. Repair that installation or uninstall "
         "it first.");
    const auto label = key == "CommunityDivers-QA" ? "Community Divers QA" : "Community Divers";
    need(matching_shortcut(shortcut_path(label), root),
         "An unrelated Start menu shortcut has the same name");
}
void register_application(const fs::path &root, const std::string &key, const std::string &label) {
    check_registration(root, key);
    ComPtr<IShellLinkW> link;
    ComPtr<IPersistFile> persist;
    need(SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&link))) &&
             SUCCEEDED(link->SetPath((root / "CommunityDivers.exe").c_str())) &&
             SUCCEEDED(link->SetWorkingDirectory(root.c_str())) && SUCCEEDED(link.As(&persist)) &&
             SUCCEEDED(persist->Save(shortcut_path(label).c_str(), TRUE)),
         "Cannot create Start menu shortcut");
    HKEY handle = nullptr;
    need(RegCreateKeyExW(HKEY_CURRENT_USER, registry_path(key).c_str(), 0, nullptr, 0,
                         KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &handle,
                         nullptr) == ERROR_SUCCESS,
         "Cannot register the launcher in Windows");
    auto put = [&](const wchar_t *name, const std::wstring &value) {
        return RegSetValueExW(
                   handle, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(value.c_str()),
                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    };
    auto uninstall = L"\"" + (root / "Uninstall.exe").native() + L"\"";
    bool ok = put(L"DisplayName", wide(label)) &&
              put(L"DisplayVersion", wide(std::string(version::release_id))) &&
              put(L"InstallLocation", root.native()) &&
              put(L"Publisher", L"Community Divers contributors") &&
              put(L"DisplayIcon", (root / "CommunityDivers.exe").native()) &&
              put(L"UninstallString", uninstall) &&
              put(L"QuietUninstallString", uninstall + L" --silent") &&
              put(L"URLInfoAbout", L"https://github.com/Nnamllit1/community-divers");
    DWORD one = 1;
    ok = ok && RegSetValueExW(handle, L"NoModify", 0, REG_DWORD, reinterpret_cast<BYTE *>(&one),
                              4) == ERROR_SUCCESS;
    RegCloseKey(handle);
    need(ok, "Windows application registration is incomplete. Run setup again to repair it.");
}
void unregister_application(const fs::path &root, const std::string &key,
                            const std::string &label) {
    auto existing = registered_path(key);
    if (!existing.empty() &&
        _wcsicmp(existing.lexically_normal().c_str(), root.lexically_normal().c_str()) == 0) {
        auto shortcut = shortcut_path(label);
        if (fs::exists(shortcut) && matching_shortcut(shortcut, root))
            need(DeleteFileW(shortcut.c_str()) != FALSE, "Cannot remove the launcher shortcut");
        auto result =
            RegDeleteKeyExW(HKEY_CURRENT_USER, registry_path(key).c_str(), KEY_WOW64_64KEY, 0);
        need(result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND,
             "Cannot remove Windows application registration");
    }
}
} // namespace cd::setup
