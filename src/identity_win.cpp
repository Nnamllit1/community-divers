#include <windows.h>
#include <bcrypt.h>
#include "community/platform.hpp"
#include <memory>
#include <thread>
namespace cd {
namespace {
void crypto(NTSTATUS status) {
    if (status < 0)
        throw std::runtime_error("Windows SHA-256 operation failed");
}
struct File {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit File(const fs::path &p) {
        value = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (value == INVALID_HANDLE_VALUE)
            throw std::runtime_error("File could not be opened for reading (Windows " +
                                     std::to_string(GetLastError()) + ")");
    }
    ~File() {
        if (value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
};
struct Algorithm {
    BCRYPT_ALG_HANDLE value = nullptr;
    ~Algorithm() {
        if (value)
            BCryptCloseAlgorithmProvider(value, 0);
    }
};
struct Hash {
    BCRYPT_HASH_HANDLE value = nullptr;
    ~Hash() {
        if (value)
            BCryptDestroyHash(value);
    }
};
} // namespace
FileFingerprint fingerprint_file(const fs::path &path, std::string relative_path) {
    FileFingerprint result;
    result.path = std::move(relative_path);
    try {
        File file(path);
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file.value, &size) || size.QuadPart < 0)
            throw std::runtime_error("Cannot read file size");
        result.bytes = static_cast<uint64_t>(size.QuadPart);
        Algorithm algorithm;
        crypto(BCryptOpenAlgorithmProvider(&algorithm.value, BCRYPT_SHA256_ALGORITHM, nullptr, 0));
        DWORD object_size = 0, returned = 0;
        crypto(BCryptGetProperty(algorithm.value, BCRYPT_OBJECT_LENGTH,
                                 reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size),
                                 &returned, 0));
        std::vector<unsigned char> object(object_size);
        Hash hash;
        crypto(BCryptCreateHash(algorithm.value, &hash.value, object.data(), object_size, nullptr,
                                0, 0));
        std::vector<unsigned char> buffer(1024 * 1024);
        uint64_t total = 0;
        for (;;) {
            DWORD bytes = 0;
            if (!ReadFile(file.value, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes,
                          nullptr))
                throw std::runtime_error("Cannot read client file");
            if (!bytes)
                break;
            crypto(BCryptHashData(hash.value, buffer.data(), bytes, 0));
            total += bytes;
        }
        if (total != result.bytes)
            throw std::runtime_error("Client file changed during inspection");
        unsigned char digest[32];
        crypto(BCryptFinishHash(hash.value, digest, sizeof(digest), 0));
        constexpr char hex[] = "0123456789abcdef";
        for (auto byte : digest) {
            result.sha256 += hex[byte >> 4];
            result.sha256 += hex[byte & 15];
        }
    } catch (const std::exception &e) {
        result.sha256.clear();
        result.error = e.what();
    }
    return result;
}
ClientIdentity inspect_client(const fs::path &path) {
    ClientIdentity result;
    result.installation = inspect_installation(path);
    result.files.resize(client_binary_paths.size());
    std::vector<std::unique_ptr<File>> guards(client_binary_paths.size());
    for (size_t i = 0; i < client_binary_paths.size(); ++i) {
        auto relative = std::string(client_binary_paths[i]);
        result.files[i].path = relative;
        try {
            guards[i] = std::make_unique<File>(path / from_utf8(relative));
        } catch (const std::exception &e) {
            result.files[i].error = e.what();
        }
    }
    for (size_t i = 0; i < client_binary_paths.size(); ++i) {
        if (guards[i])
            result.files[i] =
                fingerprint_file(path / from_utf8(result.files[i].path), result.files[i].path);
    }
    auto after = inspect_installation(path);
    if (after.build != result.installation.build || after.valid != result.installation.valid) {
        for (auto &file : result.files)
            if (file.error.empty())
                file.error =
                    "Installation metadata changed during inspection; run the check again.";
    }
    return result;
}
std::future<ClientIdentity> inspect_client_async(fs::path path) {
    auto promise = std::make_shared<std::promise<ClientIdentity>>();
    auto future = promise->get_future();
    std::thread([promise, path = std::move(path)] {
        try {
            promise->set_value(inspect_client(path));
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }).detach();
    return future;
}
} // namespace cd
