#include "community/preparation.hpp"
#include <algorithm>
#include <cctype>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
namespace cd {
namespace {
[[noreturn]] void invalid() {
    throw std::runtime_error("Invalid or unsupported local Steam depot manifest");
}
struct Proto {
    std::string_view input;
    size_t pos = 0;
    uint64_t varint() {
        uint64_t result = 0;
        for (unsigned shift = 0; shift < 70; shift += 7) {
            if (pos == input.size())
                invalid();
            auto c = static_cast<unsigned char>(input[pos++]);
            if (shift == 63 && (c & 0xfe))
                invalid();
            result |= uint64_t(c & 0x7f) << shift;
            if (!(c & 0x80))
                return result;
        }
        invalid();
    }
    std::string_view take(uint64_t length) {
        if (length > input.size() - pos)
            invalid();
        auto out = input.substr(pos, static_cast<size_t>(length));
        pos += static_cast<size_t>(length);
        return out;
    }
    struct Field {
        uint32_t number, wire;
        uint64_t value = 0;
        std::string_view data;
    };
    Field next() {
        auto tag = varint();
        if (!(tag >> 3) || (tag >> 3) > 0x1fffffff)
            invalid();
        Field f{static_cast<uint32_t>(tag >> 3), static_cast<uint32_t>(tag & 7)};
        switch (f.wire) {
        case 0:
            f.value = varint();
            break;
        case 1:
            f.data = take(8);
            break;
        case 2:
            f.data = take(varint());
            break;
        case 5:
            f.data = take(4);
            break;
        default:
            invalid();
        }
        return f;
    }
    bool more() const { return pos < input.size(); }
};
std::string hex(std::string_view bytes) {
    constexpr char alphabet[] = "0123456789abcdef";
    std::string out;
    for (unsigned char c : bytes) {
        out += alphabet[c >> 4];
        out += alphabet[c & 15];
    }
    return out;
}
std::string lower(std::string s) {
    for (auto &c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
DepotFile mapping(std::string_view bytes) {
    Proto p{bytes};
    DepotFile out;
    uint64_t flags = 0;
    std::set<uint32_t> seen;
    while (p.more()) {
        auto f = p.next();
        if (f.number <= 7 && f.number != 6 && !seen.insert(f.number).second)
            invalid();
        switch (f.number) {
        case 1:
            if (f.wire != 2)
                invalid();
            out.path = f.data;
            std::replace(out.path.begin(), out.path.end(), '\\', '/');
            break;
        case 2:
            if (f.wire != 0)
                invalid();
            out.bytes = f.value;
            break;
        case 3:
            if (f.wire != 0)
                invalid();
            flags = f.value;
            break;
        case 5:
            if (f.wire != 2)
                invalid();
            out.sha1 = hex(f.data);
            break;
        case 7:
            if (f.wire != 2 || !f.data.empty())
                invalid();
            break;
        default:
            break;
        }
    }
    // Encrypted files and symlinks need behavior this preparer does not provide.
    if (!client_relative_path(out.path) || (flags & (4 | 512)) || (flags & ~uint64_t(1023)))
        invalid();
    out.directory = (flags & 64) != 0;
    if (out.directory ? out.bytes != 0 : out.sha1.size() != 40)
        invalid();
    return out;
}
} // namespace
bool client_relative_path(std::string_view path) {
    if (path.empty() || path.size() > 220 || path.front() == '/' || path.back() == '/')
        return false;
    for (unsigned char c : path)
        if (c < 32 || c > 126 || c == '\\' || c == ':' || c == '"' || c == '<' || c == '>' ||
            c == '|' || c == '?' || c == '*' || c == '~')
            return false;
    size_t start = 0;
    while (start < path.size()) {
        auto end = path.find('/', start);
        auto part = path.substr(start, end == path.npos ? path.size() - start : end - start);
        if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ')
            return false;
        auto base = lower(std::string(part.substr(0, part.find('.'))));
        if (base == "con" || base == "prn" || base == "aux" || base == "nul" ||
            lower(std::string(part)) == ".community-divers" ||
            (base.size() == 4 && (base.starts_with("com") || base.starts_with("lpt")) &&
             base.back() >= '0' && base.back() <= '9'))
            return false;
        if (end == path.npos)
            break;
        start = end + 1;
    }
    return true;
}
DepotManifest parse_depot_manifest(std::string_view bytes) {
    if (bytes.size() > 64 * 1024 * 1024)
        invalid();
    Proto sections{bytes};
    auto u32 = [&]() {
        auto b = sections.take(4);
        uint32_t n = 0;
        for (unsigned i = 0; i < 4; ++i)
            n |= uint32_t(static_cast<unsigned char>(b[i])) << (8 * i);
        return n;
    };
    std::string_view payload, metadata;
    bool has_payload = false, has_metadata = false, signature = false, ended = false;
    while (sections.more()) {
        auto magic = u32();
        if (magic == 0x32c415ab) {
            ended = true;
            break;
        }
        auto data = sections.take(u32());
        if (magic == 0x71f617d0 && !has_payload) {
            payload = data;
            has_payload = true;
        } else if (magic == 0x1f4812be && !has_metadata) {
            metadata = data;
            has_metadata = true;
        } else if (magic == 0x1b81b817 && !signature)
            signature = true;
        else
            invalid();
    }
    if (!ended || sections.more() || payload.empty() || metadata.empty() || !signature)
        invalid();
    DepotManifest out;
    Proto meta{metadata};
    std::set<uint32_t> seen;
    while (meta.more()) {
        auto f = meta.next();
        if (f.number <= 9 && !seen.insert(f.number).second)
            invalid();
        if (f.number == 1) {
            if (f.wire != 0 || f.value > UINT32_MAX)
                invalid();
            out.depot = static_cast<uint32_t>(f.value);
        } else if (f.number == 2) {
            if (f.wire != 0)
                invalid();
            out.manifest = f.value;
        } else if (f.number == 4 && (f.wire != 0 || f.value != 0))
            throw std::runtime_error(
                "Local depot manifest filenames are encrypted; use Steam's installed cache");
    }
    if (!out.depot || !out.manifest)
        invalid();
    Proto p{payload};
    std::map<std::string, bool> names;
    while (p.more()) {
        auto f = p.next();
        if (f.number != 1)
            continue;
        if (f.wire != 2 || out.files.size() >= 100000)
            invalid();
        auto file = mapping(f.data);
        if (!names.emplace(lower(file.path), file.directory).second)
            invalid();
        out.files.push_back(std::move(file));
    }
    for (const auto &[name, directory] : names) {
        (void)directory;
        for (auto pos = name.find('/'); pos != name.npos; pos = name.find('/', pos + 1)) {
            auto it = names.find(name.substr(0, pos));
            if (it != names.end() && !it->second)
                invalid();
        }
    }
    if (out.files.empty())
        invalid();
    return out;
}
json preparation_report(const PreparationPlan &plan) {
    json out{{"schema", 1},
             {"steam_build", plan.build},
             {"depots", plan.depots},
             {"bytes", plan.bytes},
             {"files", json::array()},
             {"state", "planned"},
             {"integration_available", false},
             {"profile_isolation_verified", false},
             {"official_network_isolation_verified", false}};
    for (const auto &f : plan.files)
        out["files"].push_back({{"path", f.path}, {"bytes", f.bytes}, {"sha1", f.sha1}});
    return out;
}
} // namespace cd
