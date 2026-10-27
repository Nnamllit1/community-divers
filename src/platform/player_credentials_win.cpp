#include <windows.h>
#include <bcrypt.h>
#include <wincred.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include "community/platform.hpp"
namespace cd {
namespace {
void checked(NTSTATUS status) {
    if (status < 0)
        throw std::runtime_error("Cannot identify the player credential scope");
}
std::wstring target(const fs::path &data, const std::string &url, const std::string &community_id) {
    if (data.empty())
        throw std::runtime_error("Launcher data directory is required");
    // Validate the community ID without accepting a credential or an arbitrary vault key.
    PlayerCredential dummy{community_id, std::string(32, '0'), "cdp_" + std::string(64, '0')};
    encode_player_credential(dummy);
    auto path = fs::weakly_canonical(fs::absolute(data)).wstring();
    CharLowerBuffW(path.data(), static_cast<DWORD>(path.size()));
    auto input = utf8(path) + '\0' + parse_endpoint(url).canonical + '\0' + community_id;
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::vector<unsigned char> object;
    try {
        checked(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0));
        DWORD count = 0, returned = 0;
        checked(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&count),
                                  sizeof(count), &returned, 0));
        object.resize(count);
        checked(BCryptCreateHash(algorithm, &hash, object.data(), count, nullptr, 0, 0));
        checked(BCryptHashData(hash, reinterpret_cast<PUCHAR>(input.data()),
                               static_cast<ULONG>(input.size()), 0));
        unsigned char digest[32];
        checked(BCryptFinishHash(hash, digest, sizeof(digest), 0));
        BCryptDestroyHash(hash);
        hash = nullptr;
        BCryptCloseAlgorithmProvider(algorithm, 0);
        algorithm = nullptr;
        constexpr wchar_t alphabet[] = L"0123456789abcdef";
        std::wstring key = L"CommunityDivers/Player/v1/";
        for (auto b : digest) {
            key += alphabet[b >> 4];
            key += alphabet[b & 15];
        }
        return key;
    } catch (...) {
        if (hash)
            BCryptDestroyHash(hash);
        if (algorithm)
            BCryptCloseAlgorithmProvider(algorithm, 0);
        throw;
    }
}
} // namespace
void remember_player(const fs::path &data, const std::string &url,
                     const PlayerCredential &credential) {
    auto body = encode_player_credential(credential).dump();
    auto key = target(data, url, credential.community_id), user = wide(credential.player_id);
    CREDENTIALW item{};
    item.Type = CRED_TYPE_GENERIC;
    item.TargetName = key.data();
    item.UserName = user.data();
    item.Persist = CRED_PERSIST_LOCAL_MACHINE;
    item.CredentialBlobSize = static_cast<DWORD>(body.size());
    item.CredentialBlob = reinterpret_cast<LPBYTE>(body.data());
    auto ok = body.size() <= CRED_MAX_CREDENTIAL_BLOB_SIZE && CredWriteW(&item, 0);
    SecureZeroMemory(body.data(), body.size());
    if (!ok)
        throw std::runtime_error("Windows could not remember this sign-in. It is available only "
                                 "for this launcher session.");
}
std::optional<PlayerCredential> saved_player(const fs::path &data, const std::string &url,
                                             const std::string &community_id) {
    auto key = target(data, url, community_id);
    PCREDENTIALW item = nullptr;
    if (!CredReadW(key.c_str(), CRED_TYPE_GENERIC, 0, &item)) {
        if (GetLastError() == ERROR_NOT_FOUND)
            return {};
        throw std::runtime_error("Windows could not read the saved community sign-in.");
    }
    try {
        if (!item->CredentialBlob || item->CredentialBlobSize > CRED_MAX_CREDENTIAL_BLOB_SIZE)
            throw std::runtime_error("Invalid stored community credential.");
        std::string body(reinterpret_cast<const char *>(item->CredentialBlob),
                         item->CredentialBlobSize);
        auto credential = parse_player_credential(body, community_id);
        SecureZeroMemory(body.data(), body.size());
        SecureZeroMemory(item->CredentialBlob, item->CredentialBlobSize);
        CredFree(item);
        return credential;
    } catch (...) {
        if (item->CredentialBlob)
            SecureZeroMemory(item->CredentialBlob, item->CredentialBlobSize);
        CredFree(item);
        throw;
    }
}
void forget_player(const fs::path &data, const std::string &url, const std::string &community_id) {
    auto key = target(data, url, community_id);
    if (!CredDeleteW(key.c_str(), CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND)
        throw std::runtime_error("Windows could not forget this community sign-in.");
}
std::optional<fs::path> choose_player_credential(void *owner) {
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog))))
        throw std::runtime_error("Cannot open credential file picker");
    DWORD flags = 0;
    dialog->GetOptions(&flags);
    dialog->SetOptions(flags | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST |
                       FOS_DONTADDTORECENT);
    dialog->SetTitle(L"Import a Community Divers player credential");
    COMDLG_FILTERSPEC filters[]{{L"Player credential JSON", L"*.json"}};
    dialog->SetFileTypes(1, filters);
    auto result = dialog->Show(static_cast<HWND>(owner));
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return {};
    if (FAILED(result))
        throw std::runtime_error("Credential file picker failed");
    Microsoft::WRL::ComPtr<IShellItem> file;
    if (FAILED(dialog->GetResult(&file)))
        throw std::runtime_error("No credential file selected");
    PWSTR name = nullptr;
    if (FAILED(file->GetDisplayName(SIGDN_FILESYSPATH, &name)))
        throw std::runtime_error("Credential must be a local file");
    fs::path path(name);
    CoTaskMemFree(name);
    return path;
}
} // namespace cd
