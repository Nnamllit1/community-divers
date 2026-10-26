#include <windows.h>
#include <bcrypt.h>
#include "community/updates.hpp"
#include "community/update_trust.hpp"
#include <cstring>
namespace cd {
namespace {
void checked(NTSTATUS s) {
    if (s < 0)
        throw std::runtime_error("Update cryptographic operation failed");
}
struct Algorithm {
    BCRYPT_ALG_HANDLE h = nullptr;
    explicit Algorithm(LPCWSTR name) { checked(BCryptOpenAlgorithmProvider(&h, name, nullptr, 0)); }
    ~Algorithm() {
        if (h)
            BCryptCloseAlgorithmProvider(h, 0);
    }
};
struct Key {
    BCRYPT_KEY_HANDLE h = nullptr;
    ~Key() {
        if (h)
            BCryptDestroyKey(h);
    }
};
std::string encode(const std::vector<unsigned char> &data) {
    constexpr char chars[] = "0123456789abcdef";
    std::string out;
    for (auto c : data) {
        out += chars[c >> 4];
        out += chars[c & 15];
    }
    return out;
}
std::vector<unsigned char> decode(const std::string &text, size_t size) {
    if (text.size() != size * 2)
        throw std::runtime_error("Invalid update key or signature length");
    auto digit = [](char c) -> unsigned char {
        if (c >= '0' && c <= '9')
            return static_cast<unsigned char>(c - '0');
        if (c >= 'a' && c <= 'f')
            return static_cast<unsigned char>(c - 'a' + 10);
        throw std::runtime_error("Invalid update key or signature encoding");
    };
    std::vector<unsigned char> out(size);
    for (size_t i = 0; i < size; ++i)
        out[i] = static_cast<unsigned char>((digit(text[i * 2]) << 4) | digit(text[i * 2 + 1]));
    return out;
}
std::vector<unsigned char> digest(const std::string &text) {
    Algorithm algorithm(BCRYPT_SHA256_ALGORITHM);
    std::vector<unsigned char> out(32);
    checked(BCryptHash(algorithm.h, nullptr, 0,
                       reinterpret_cast<PUCHAR>(const_cast<char *>(text.data())),
                       static_cast<ULONG>(text.size()), out.data(), 32));
    return out;
}
std::string export_key(BCRYPT_KEY_HANDLE key, LPCWSTR format) {
    ULONG size = 0;
    checked(BCryptExportKey(key, nullptr, format, nullptr, 0, &size, 0));
    std::vector<unsigned char> bytes(size);
    checked(BCryptExportKey(key, nullptr, format, bytes.data(), size, &size, 0));
    return encode(bytes);
}
} // namespace
UpdateKeyPair generate_update_key() {
    Algorithm algorithm(BCRYPT_ECDSA_P256_ALGORITHM);
    Key key;
    checked(BCryptGenerateKeyPair(algorithm.h, &key.h, 256, 0));
    checked(BCryptFinalizeKeyPair(key.h, 0));
    return {export_key(key.h, BCRYPT_ECCPRIVATE_BLOB), export_key(key.h, BCRYPT_ECCPUBLIC_BLOB)};
}
std::string sign_update_payload(const std::string &payload, const std::string &private_hex) {
    if (payload.size() > update_manifest_limit / 2)
        throw std::runtime_error("Update payload is too large");
    parse_update_payload(payload, "preview");
    auto bytes = decode(private_hex, sizeof(BCRYPT_ECCKEY_BLOB) + 96);
    Algorithm algorithm(BCRYPT_ECDSA_P256_ALGORITHM);
    Key key;
    checked(BCryptImportKeyPair(algorithm.h, nullptr, BCRYPT_ECCPRIVATE_BLOB, &key.h, bytes.data(),
                                static_cast<ULONG>(bytes.size()), 0));
    SecureZeroMemory(bytes.data(), bytes.size());
    auto hash = digest(payload);
    std::vector<unsigned char> signature(64);
    ULONG count = 0;
    checked(BCryptSignHash(key.h, nullptr, hash.data(), static_cast<ULONG>(hash.size()),
                           signature.data(), 64, &count, 0));
    if (count != 64)
        throw std::runtime_error("Unexpected update signature size");
    return json{{"schema", 1},
                {"algorithm", "ecdsa-p256-sha256"},
                {"payload", payload},
                {"signature", encode(signature)}}
               .dump(2) +
           "\n";
}
UpdateRelease verify_update_envelope(const std::string &body, const std::string &channel,
                                     const std::string &public_hex) {
    if (body.size() > update_manifest_limit)
        throw std::runtime_error("Signed update manifest is too large");
    auto j = json::parse(body);
    if (j.at("schema") != 1 || j.at("algorithm") != "ecdsa-p256-sha256")
        throw std::runtime_error("Unsupported update signature format");
    auto payload = j.at("payload").get<std::string>();
    if (payload.size() > update_manifest_limit / 2)
        throw std::runtime_error("Update payload is too large");
    auto signature = decode(j.at("signature").get<std::string>(), 64);
    auto bytes = decode(public_hex, sizeof(BCRYPT_ECCKEY_BLOB) + 64);
    BCRYPT_ECCKEY_BLOB header{};
    std::memcpy(&header, bytes.data(), sizeof(header));
    if (header.dwMagic != BCRYPT_ECDSA_PUBLIC_P256_MAGIC || header.cbKey != 32)
        throw std::runtime_error("Invalid publisher trust key");
    Algorithm algorithm(BCRYPT_ECDSA_P256_ALGORITHM);
    Key key;
    checked(BCryptImportKeyPair(algorithm.h, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key.h, bytes.data(),
                                static_cast<ULONG>(bytes.size()), 0));
    auto hash = digest(payload);
    if (BCryptVerifySignature(key.h, nullptr, hash.data(), static_cast<ULONG>(hash.size()),
                              signature.data(), 64, 0) < 0)
        throw std::runtime_error("Update signature does not match the trusted publisher");
    return parse_update_payload(payload, channel);
}
UpdateCheck check_update(const std::string &url, const std::string &key, const UpdateFetch &fetch) {
    try {
        auto envelope = fetch(url, update_manifest_limit);
        auto release = verify_update_envelope(envelope, "preview", key);
        return {std::move(release), {}, std::move(envelope)};
    } catch (const std::exception &e) {
        return {{}, e.what()};
    }
}
std::string update_publisher_key() {
    return std::string(update_trust::publisher_key);
}
} // namespace cd
