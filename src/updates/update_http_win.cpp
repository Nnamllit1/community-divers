#include <windows.h>
#include <winhttp.h>
#include "community/platform.hpp"
#include "community/updates.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
namespace cd {
namespace {
struct Handle {
    HINTERNET h;
    explicit Handle(HINTERNET v) : h(v) {
        if (!h)
            throw std::runtime_error("Update HTTP initialization failed");
    }
    ~Handle() { WinHttpCloseHandle(h); }
    operator HINTERNET() const { return h; }
};
void check(BOOL ok) {
    if (!ok)
        throw std::runtime_error("Update HTTP error " + std::to_string(GetLastError()));
}
struct Url {
    std::wstring host, path;
};
Url parse_url(const std::string &url) {
    if (url.size() > 16384 || url.find_first_of("\r\n\\#") != std::string::npos ||
        std::any_of(url.begin(), url.end(), [](unsigned char c) { return c <= 32 || c >= 127; }))
        throw std::runtime_error("Invalid update transport URL");
    auto authority_end = url.find('/', 8);
    if (url.substr(0, authority_end).find('@') != std::string::npos)
        throw std::runtime_error("Update URLs must not contain credentials");
    auto text = wide(url);
    URL_COMPONENTS c{};
    c.dwStructSize = sizeof(c);
    c.dwHostNameLength = c.dwUrlPathLength = c.dwExtraInfoLength = c.dwUserNameLength =
        c.dwPasswordLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(text.c_str(), static_cast<DWORD>(text.size()), 0, &c) ||
        c.nScheme != INTERNET_SCHEME_HTTPS || c.nPort != 443 || c.dwUserNameLength ||
        c.dwPasswordLength)
        throw std::runtime_error("Update requests require credential-free HTTPS");
    std::wstring host(c.lpszHostName, c.dwHostNameLength);
    std::transform(host.begin(), host.end(), host.begin(), [](wchar_t x) {
        return static_cast<wchar_t>(x >= L'A' && x <= L'Z' ? x + (L'a' - L'A') : x);
    });
    if (host != L"raw.githubusercontent.com" && host != L"github.com" &&
        host != L"release-assets.githubusercontent.com" &&
        host != L"objects.githubusercontent.com" &&
        host != L"github-releases.githubusercontent.com")
        throw std::runtime_error("Update redirect is outside the permitted release hosts");
    std::wstring path(c.lpszUrlPath, c.dwUrlPathLength);
    if (c.dwExtraInfoLength)
        path.append(c.lpszExtraInfo, c.dwExtraInfoLength);
    return {host, path.empty() ? L"/" : path};
}
std::wstring header(HINTERNET request, DWORD name) {
    DWORD size = 0;
    if (WinHttpQueryHeaders(request, name, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size,
                            WINHTTP_NO_HEADER_INDEX))
        return {};
    if (GetLastError() == ERROR_WINHTTP_HEADER_NOT_FOUND)
        return {};
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size > 32768)
        throw std::runtime_error("Invalid update HTTP header");
    std::wstring value(size / sizeof(wchar_t), L'\0');
    check(WinHttpQueryHeaders(request, name, WINHTTP_HEADER_NAME_BY_INDEX, value.data(), &size,
                              WINHTTP_NO_HEADER_INDEX));
    value.resize(size / sizeof(wchar_t));
    return value;
}
} // namespace
bool update_transport_url_allowed(const std::string &url) {
    try {
        parse_url(url);
        return true;
    } catch (...) {
        return false;
    }
}
void stream_update(const std::string &source, uint64_t limit, const UpdateSink &sink) {
    if (!limit || limit > update_package_limit)
        throw std::runtime_error("Invalid update download bound");
    Handle session(WinHttpOpen(L"CommunityDivers-Updater/1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    check(WinHttpSetTimeouts(session, 3000, 5000, 5000, 5000));
    auto url = source;
    auto deadline =
        std::chrono::steady_clock::now() +
        (limit <= update_manifest_limit ? std::chrono::seconds(20) : std::chrono::seconds(300));
    for (int redirects = 0; redirects <= 4; ++redirects) {
        if (std::chrono::steady_clock::now() > deadline)
            throw std::runtime_error("Update request timed out");
        auto parsed = parse_url(url);
        Handle connection(WinHttpConnect(session, parsed.host.c_str(), 443, 0));
        Handle request(WinHttpOpenRequest(connection, L"GET", parsed.path.c_str(), nullptr,
                                          WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          WINHTTP_FLAG_SECURE));
        DWORD disabled = WINHTTP_DISABLE_AUTHENTICATION | WINHTTP_DISABLE_COOKIES;
        check(
            WinHttpSetOption(request, WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)));
        DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        check(WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy,
                               sizeof(redirect_policy)));
        check(WinHttpSendRequest(request, L"Accept: application/octet-stream\r\n",
                                 static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0));
        check(WinHttpReceiveResponse(request, nullptr));
        DWORD status = 0, size = sizeof(status);
        check(WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                  WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                                  WINHTTP_NO_HEADER_INDEX));
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            if (redirects == 4)
                throw std::runtime_error("Too many update redirects");
            url = utf8(header(request, WINHTTP_QUERY_LOCATION));
            parse_url(url); // Validate every hop before opening it; never send credentials.
            continue;
        }
        if (status != 200)
            throw std::runtime_error("Update service returned HTTP " + std::to_string(status));
        auto length = utf8(header(request, WINHTTP_QUERY_CONTENT_LENGTH));
        if (!length.empty()) {
            uint64_t bytes = 0;
            auto [end, error] =
                std::from_chars(length.data(), length.data() + length.size(), bytes);
            if (error != std::errc{} || end != length.data() + length.size() || bytes > limit)
                throw std::runtime_error("Update response exceeds the permitted size");
        }
        uint64_t total = 0;
        char block[65536];
        for (;;) {
            if (std::chrono::steady_clock::now() > deadline)
                throw std::runtime_error("Update request timed out");
            DWORD count = 0;
            check(WinHttpReadData(request, block, sizeof(block), &count));
            if (!count)
                return;
            if (count > limit - total)
                throw std::runtime_error("Update response exceeds the permitted size");
            sink(std::string_view(block, count));
            total += count;
        }
    }
    throw std::runtime_error("Update request did not finish");
}
std::string fetch_update_text(const std::string &url, uint64_t limit) {
    if (limit > update_manifest_limit)
        throw std::runtime_error("Text update request exceeds the manifest bound");
    std::string body;
    stream_update(url, limit, [&](std::string_view part) { body.append(part); });
    return body;
}
} // namespace cd
