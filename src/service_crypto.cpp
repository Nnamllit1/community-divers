#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <sddl.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif
#include "community/service.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <fstream>
#include <vector>
namespace cd::service {
namespace {
std::string hex(const unsigned char *p, size_t count) {
    std::string out;
    constexpr char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < count; ++i) {
        out += digits[p[i] >> 4];
        out += digits[p[i] & 15];
    }
    return out;
}
#ifdef _WIN32
struct Security {
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, FALSE};
    Security() {
        HANDLE token = nullptr;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
            throw std::runtime_error("Cannot identify service data owner");
        DWORD size = 0;
        GetTokenInformation(token, TokenUser, nullptr, 0, &size);
        std::vector<unsigned char> buffer(size);
        BOOL ok = GetTokenInformation(token, TokenUser, buffer.data(), size, &size);
        CloseHandle(token);
        LPWSTR sid = nullptr;
        if (!ok ||
            !ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(buffer.data())->User.Sid, &sid))
            throw std::runtime_error("Cannot protect service data");
        std::wstring acl = L"D:P(A;OICI;GA;;;" + std::wstring(sid) + L")";
        LocalFree(sid);
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(), SDDL_REVISION_1,
                                                                  &descriptor, nullptr))
            throw std::runtime_error("Cannot protect service data");
        attributes.lpSecurityDescriptor = descriptor;
    }
    ~Security() {
        if (descriptor)
            LocalFree(descriptor);
    }
};
#endif
class Hash {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
#else
    EVP_MD_CTX *hash = nullptr;
#endif
  public:
    Hash() {
#ifdef _WIN32
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
            BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0)
            throw std::runtime_error("Cannot create service hash");
#else
        hash = EVP_MD_CTX_new();
        if (!hash || !EVP_DigestInit_ex(hash, EVP_sha256(), nullptr))
            throw std::runtime_error("Cannot create service hash");
#endif
    }
    ~Hash() {
#ifdef _WIN32
        if (hash)
            BCryptDestroyHash(hash);
        if (algorithm)
            BCryptCloseAlgorithmProvider(algorithm, 0);
#else
        EVP_MD_CTX_free(hash);
#endif
    }
    void add(std::string_view body) {
#ifdef _WIN32
        if (body.size() > MAXDWORD ||
            BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char *>(body.data())),
                           static_cast<ULONG>(body.size()), 0) < 0)
#else
        if (!EVP_DigestUpdate(hash, body.data(), body.size()))
#endif
            throw std::runtime_error("Service hash operation failed");
    }
    std::string finish() {
        unsigned char out[32];
#ifdef _WIN32
        if (BCryptFinishHash(hash, out, 32, 0) < 0)
#else
        unsigned count = 0;
        if (!EVP_DigestFinal_ex(hash, out, &count) || count != 32)
#endif
            throw std::runtime_error("Service hash operation failed");
        return hex(out, 32);
    }
};
} // namespace
std::string random_hex(size_t bytes) {
    if (!bytes || bytes > 1024)
        throw std::runtime_error("Invalid random token size");
    std::vector<unsigned char> buffer(bytes);
#ifdef _WIN32
    if (BCryptGenRandom(nullptr, buffer.data(), static_cast<ULONG>(bytes),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
#else
    if (RAND_bytes(buffer.data(), static_cast<int>(bytes)) != 1)
#endif
        throw std::runtime_error("Cannot generate service token");
    return hex(buffer.data(), buffer.size());
}
std::string sha256_text(std::string_view text) {
    Hash h;
    h.add(text);
    return h.finish();
}
std::string sha256_file(const fs::path &file) {
    std::ifstream in(file, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot read service data file");
    Hash h;
    std::array<char, 65536> block;
    while (in) {
        in.read(block.data(), block.size());
        h.add(std::string_view(block.data(), static_cast<size_t>(in.gcount())));
    }
    if (!in.eof())
        throw std::runtime_error("Cannot hash service data file");
    return h.finish();
}
void private_directory(const fs::path &path) {
    if (fs::exists(path))
        throw std::runtime_error("Service output directory already exists");
    if (!path.parent_path().empty())
        fs::create_directories(path.parent_path());
#ifdef _WIN32
    Security security;
    if (!CreateDirectoryW(path.c_str(), &security.attributes))
        throw std::runtime_error("Cannot create private service directory");
#else
    if (::mkdir(path.c_str(), 0700))
        throw std::runtime_error("Cannot create private service directory");
#endif
}
void private_write_new(const fs::path &path, std::string_view body) {
#ifdef _WIN32
    Security security;
    auto file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, &security.attributes, CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot create private service file");
    size_t offset = 0;
    bool ok = true;
    while (offset < body.size()) {
        DWORD n = 0;
        auto count = static_cast<DWORD>(std::min<size_t>(65536, body.size() - offset));
        if (!WriteFile(file, body.data() + offset, count, &n, nullptr) || !n) {
            ok = false;
            break;
        }
        offset += n;
    }
    if (!FlushFileBuffers(file))
        ok = false;
    CloseHandle(file);
    if (!ok)
        throw std::runtime_error("Cannot flush private service file");
#else
    int file = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0)
        throw std::runtime_error("Cannot create private service file");
    size_t offset = 0;
    bool ok = true;
    while (offset < body.size()) {
        auto n = ::write(file, body.data() + offset, body.size() - offset);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            ok = false;
            break;
        }
        offset += static_cast<size_t>(n);
    }
    if (::fsync(file))
        ok = false;
    ::close(file);
    if (!ok)
        throw std::runtime_error("Cannot flush private service file");
#endif
}
} // namespace cd::service
