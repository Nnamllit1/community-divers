#include "community/preparation.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#include <cstring>
#endif
using namespace cd;
namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
template <typename F> void rejects(F action) {
    try {
        action();
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error("Expected preparation rejection");
}
std::string var(uint64_t n) {
    std::string s;
    do {
        auto c = static_cast<char>(n & 127);
        n >>= 7;
        s += static_cast<char>(c | (n ? 128 : 0));
    } while (n);
    return s;
}
std::string number(unsigned field, uint64_t n) {
    return var(field << 3) + var(n);
}
std::string text(unsigned field, const std::string &s) {
    return var((field << 3) | 2) + var(s.size()) + s;
}
std::string u32(uint32_t n) {
    std::string s;
    for (unsigned i = 0; i < 4; ++i)
        s += static_cast<char>(n >> (i * 8));
    return s;
}
std::string section(uint32_t id, const std::string &s) {
    return u32(id) + u32(static_cast<uint32_t>(s.size())) + s;
}
std::string file(const std::string &name, uint64_t flags = 0, std::string link = "") {
    // SHA-1 of abc. These are synthetic manifests; no proprietary file data.
    const unsigned char digest[]{0xa9, 0x99, 0x3e, 0x36, 0x47, 0x06, 0x81, 0x6a, 0xba, 0x3e,
                                 0x25, 0x71, 0x78, 0x50, 0xc2, 0x6c, 0x9c, 0xd0, 0xd8, 0x9d};
    return text(1, name) + number(2, (flags & 64) ? 0 : 3) + number(3, flags) +
           text(5, std::string(reinterpret_cast<const char *>(digest), sizeof(digest))) +
           text(7, link);
}
std::string manifest(const std::vector<std::string> &mappings, bool encrypted = false) {
    std::string payload;
    for (const auto &m : mappings)
        payload += text(1, m);
    return section(0x71f617d0, payload) +
           section(0x1f4812be, number(1, 99) + number(2, 88) + number(4, encrypted ? 1 : 0)) +
           section(0x1b81b817, "") + u32(0x32c415ab);
}
[[maybe_unused]] void write(const fs::path &path, const std::string &body) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << body;
}
#ifdef _WIN32
void junction(const fs::path &link, const fs::path &target) {
    fs::create_directory(link);
    auto substitute = L"\\??\\" + fs::absolute(target).wstring();
    auto printable = fs::absolute(target).wstring();
    // Mount-point reparse data, including the two terminating UTF-16 NULs.
    struct Header {
        DWORD tag;
        WORD length, reserved, substitute_offset, substitute_length, print_offset, print_length;
    };
    static_assert(sizeof(Header) == 16);
    std::vector<unsigned char> data(sizeof(Header) +
                                    (substitute.size() + printable.size() + 2) * sizeof(wchar_t));
    Header h{IO_REPARSE_TAG_MOUNT_POINT,
             static_cast<WORD>(data.size() - 8),
             0,
             0,
             static_cast<WORD>(substitute.size() * 2),
             static_cast<WORD>((substitute.size() + 1) * 2),
             static_cast<WORD>(printable.size() * 2)};
    std::memcpy(data.data(), &h, sizeof(h));
    std::memcpy(data.data() + sizeof(h), substitute.c_str(), (substitute.size() + 1) * 2);
    std::memcpy(data.data() + sizeof(h) + (substitute.size() + 1) * 2, printable.c_str(),
                (printable.size() + 1) * 2);
    auto handle = CreateFileW(link.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                              FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    require(handle != INVALID_HANDLE_VALUE, "Cannot open junction test fixture");
    DWORD returned = 0;
    auto ok = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, data.data(),
                              static_cast<DWORD>(data.size()), nullptr, 0, &returned, nullptr);
    CloseHandle(handle);
    require(ok != FALSE, "Cannot create junction test fixture");
}
#endif
} // namespace
int main() {
    auto root =
        fs::temp_directory_path() /
        from_utf8("community-preparation-\xc3\xa4-" +
                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        auto fixture = manifest({file("bin\\helldivers2.exe"), file("data", 64)});
        auto parsed = parse_depot_manifest(fixture);
        require(parsed.depot == 99 && parsed.manifest == 88 && parsed.files.size() == 2 &&
                    parsed.files[0].path == "bin/helldivers2.exe" && parsed.files[1].directory &&
                    parsed.files[0].sha1 == "a9993e364706816aba3e25717850c26c9cd0d89d",
                "Depot fields or paths changed");
        std::cout << "PASS local depot framing, metadata and content inventory\n";
        for (size_t n = 0; n < fixture.size(); ++n)
            rejects([&] { parse_depot_manifest(std::string_view(fixture).substr(0, n)); });
        rejects([&] { parse_depot_manifest(fixture + "junk"); });
        rejects(
            [&] { parse_depot_manifest(section(0x71f617d0, std::string(11, '\x80')) + fixture); });
        rejects([&] { parse_depot_manifest(section(0x71f617d0, std::string(1, '\0')) + fixture); });
        std::cout << "PASS truncated, overflowing and malformed depot data rejected\n";
        for (const auto &bad :
             {"../escape", "/absolute", "C:/absolute", "bin//x", "bin/x:ads", "bin/NUL.txt",
              "bin/trailing.", "bin/trailing ", ".community-divers/x"})
            rejects([&] { parse_depot_manifest(manifest({file(bad)})); });
        rejects([&] { parse_depot_manifest(manifest({file("bin/X"), file("bin/x")})); });
        rejects([&] { parse_depot_manifest(manifest({file("bin"), file("bin/x")})); });
        rejects([&] { parse_depot_manifest(manifest({file("link", 512, "elsewhere")})); });
        rejects([&] { parse_depot_manifest(manifest({file("cipher", 4)})); });
        rejects([&] { parse_depot_manifest(manifest({file("x")}, true)); });
        std::cout << "PASS depot traversal, aliases, symlinks and encryption rejected\n";
#ifdef _WIN32
        auto game = root / "library/steamapps/common/Helldivers 2";
        auto cache = root / "depotcache";
        auto app = root / "library/steamapps/appmanifest_553850.acf";
        auto depot = cache / "99_88.manifest";
        write(app, "AppState { appid 553850 buildid 12345 installdir \"Helldivers 2\" StateFlags 4 "
                   "InstalledDepots { 99 { manifest 88 } } }");
        std::vector<std::string> entries;
        for (auto name : client_binary_paths) {
            entries.push_back(file(std::string(name)));
            write(game / name, "abc");
        }
        entries.push_back(file("data/example.bundle"));
        write(game / "data/example.bundle", "abc");
        write(depot, manifest(entries));
        write(game / "director-mode/private.txt", "never copy");
        write(game / "data/unowned.patch_0", "never copy");
        auto plan = plan_client_copy(game, cache);
        require(plan.bytes == 15 && plan.files.size() == 5 && plan.build == "12345",
                "Preparation copied files outside installed depots");
        auto report = preparation_report(plan);
        require(report.at("integration_available") == false && !report.contains("source") &&
                    report.at("profile_isolation_verified") == false,
                "Preparation overstated isolation");
        std::cout << "PASS preparation selects installed depot files and excludes stray mods\n";
        auto destination = root / "isolated";
        prepare_client_copy(game, cache, destination);
        auto verified = verify_client_copy(destination);
        require(verified.at("state") == "verified_copy" &&
                    !fs::exists(game / ".community-divers") &&
                    !fs::exists(destination / "director-mode") &&
                    !fs::exists(destination / "data/unowned.patch_0"),
                "Copy or official install separation failed");
        write(destination / "data/example.bundle", "xyz");
        require(read_text(game / "data/example.bundle") == "abc", "Copy shares writable content");
        rejects([&] { verify_client_copy(destination); });
        write(destination / "data/example.bundle", "abc");
        std::cout
            << "PASS independent native copy, Unicode paths and destination corruption checks\n";
        rejects([&] { prepare_client_copy(game, cache, game / "nested"); });
        rejects([&] { prepare_client_copy(game, cache, game.parent_path()); });
        rejects([&] { prepare_client_copy(game, cache, root / "elsewhere/steamapps/game"); });
        rejects([&] { prepare_client_copy(game, cache, destination); });
        auto ambiguous_source = game;
        ambiguous_source += L".";
        rejects([&] { prepare_client_copy(game, cache, ambiguous_source / "escaped"); });
        rejects([&] { prepare_client_copy(game, cache, root / "trailing. /escaped"); });
        require(!fs::exists(game / "escaped.preparing"),
                "Ambiguous Windows alias escaped overlap checks");

        require(!fs::exists(game / "nested") && !fs::exists(root / "elsewhere"),
                "Unsafe copy wrote files");
        std::cout << "PASS source overlap, Steam targets and existing destinations refused\n";
        auto alias = game.parent_path() / "alias";
        junction(alias, game);
        rejects([&] { plan_client_copy(alias, cache); });
        fs::remove(alias);
        auto junction_parent = root / "destination-alias";
        junction(junction_parent, destination);
        rejects([&] { prepare_client_copy(game, cache, junction_parent / "new-copy"); });
        require(!fs::exists(destination / "new-copy"), "Destination junction was followed");
        fs::remove(junction_parent);
        std::cout << "PASS source and destination junctions refused without following them\n";

        write(destination / "extra.dll", "abc");
        rejects([&] { verify_client_copy(destination); });
        fs::remove(destination / "extra.dll");
        fs::rename(destination / "data/example.bundle", destination / "data/original.bundle");
        fs::create_hard_link(destination / "data/original.bundle",
                             destination / "data/example.bundle");
        rejects([&] { verify_client_copy(destination); });
        fs::remove(destination / "data/example.bundle");
        fs::rename(destination / "data/original.bundle", destination / "data/example.bundle");
        std::cout << "PASS unexpected files and hardlinked prepared content rejected\n";
        write(game / "data/example.bundle", "xyz");
        auto failed = root / "bad-copy";
        rejects([&] { prepare_client_copy(game, cache, failed); });
        require(!fs::exists(failed) && fs::exists(root / "bad-copy.preparing") &&
                    !fs::exists(root / "bad-copy.preparing/.community-divers/preparation.json"),
                "Partial copy was published as prepared");
        write(game / "data/example.bundle", "abc");
        std::cout << "PASS source corruption retains only unpublished staging data\n";
        rejects([&] {
            prepare_client_copy(game, cache, root / "cancelled",
                                [&](size_t n, size_t, const std::string &) {
                                    if (n == 1)
                                        throw std::runtime_error("Synthetic interruption");
                                });
        });
        require(!fs::exists(root / "cancelled") && fs::exists(root / "cancelled.preparing"),
                "Interrupted copy was published");
        std::cout << "PASS interrupted preparation never publishes a completed copy\n";
        prepare_client_copy(
            game, cache, root / "metadata-lock-copy",
            [&](size_t n, size_t count, const std::string &) {
                if (n != 0 || count == 0)
                    return;
                auto handle = CreateFileW(app.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                          OPEN_EXISTING, 0, nullptr);
                auto error = GetLastError();
                if (handle != INVALID_HANDLE_VALUE)
                    CloseHandle(handle);
                require(handle == INVALID_HANDLE_VALUE && error == ERROR_SHARING_VIOLATION,
                        "Installed metadata was not protected against writes");
            });
        require(verify_client_copy(root / "metadata-lock-copy").at("state") == "verified_copy",
                "Locked copy did not finish");
        std::cout << "PASS installed metadata remains read-locked throughout preparation\n";

        write(depot, manifest({file("bin/helldivers2.exe")}));
        rejects([&] { plan_client_copy(game, cache); });
        std::cout << "PASS incomplete depot inventories refused\n";
#endif
        fs::remove_all(root);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        // Retain the uniquely named fixture on failure for diagnosis.
        return 1;
    }
}
