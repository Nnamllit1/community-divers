#include "update_fixture.hpp"
#include "community/installer.hpp"
#include <iostream>
using namespace cd;
using namespace cd::test;
int main() {
    try {
        auto j = update_payload();
        auto r = parse_update_payload(j.dump(), "preview");
        update_require(r.bytes == 3 && r.revision == 6, "Signed payload fields lost");
        std::cout << "PASS update package identity and bounds\n";
        auto numbered = j;
        numbered["release_id"] = "0.1.0-preview.45";
        numbered["source_revision"] = 45u;
        numbered["filename"] = "CommunityDivers-0.1.0-preview.45-windows-x64.zip";
        numbered["url"] = "https://github.com/Nnamllit1/community-divers/releases/download/"
                          "v0.1.0-preview.45/CommunityDivers-0.1.0-preview.45-windows-x64.zip";
        auto next = parse_update_payload(numbered.dump(), "preview");
        update_require(next.release_id == "0.1.0-preview.45" &&
                           compare_update(next, r.source_commit, r.revision) == UpdateRelation::current,
                       "Numbered release format rejected or source identity changed");
        numbered["source_commit"] = std::string(40, 'b');
        next = parse_update_payload(numbered.dump(), "preview");
        update_require(compare_update(next, r.source_commit, r.revision) == UpdateRelation::available,
                       "Legacy install cannot recognize a numbered update");
        std::cout << "PASS numbered previews retain legacy updater compatibility\n";
        for (const auto &field : {"source_commit", "sha256", "filename", "url"}) {
            auto bad = j;
            bad[field] = "../../escape";
            update_rejects([&] { parse_update_payload(bad.dump(), "preview"); });
        }
        for (const auto &url :
             {"https://evil.example/a.zip", "http://github.com/a.zip",
              "https://github.com/Nnamllit1/community-divers/releases/download/../"}) {
            auto bad = j;
            bad["url"] = url;
            update_rejects([&] { parse_update_payload(bad.dump(), "preview"); });
        }
        std::cout << "PASS package traversal and foreign release URLs rejected\n";
        for (const auto &field : {"source_revision", "bytes", "service_protocol"}) {
            for (const auto &value : {json(0), json(-1), json(1.5), json("3")}) {
                auto bad = j;
                bad[field] = value;
                update_rejects([&] { parse_update_payload(bad.dump(), "preview"); });
            }
        }
        auto too_large = j;
        too_large["bytes"] = update_package_limit + 1;
        update_rejects([&] { parse_update_payload(too_large.dump(), "preview"); });
        update_rejects([&] { parse_update_payload(j.dump(), "stable"); });
        std::cout << "PASS update size, integer and channel validation\n";
        update_require(compare_update(r, r.source_commit, 6) == UpdateRelation::current,
                       "Current release not recognized");
        update_require(compare_update(r, std::string(40, 'b'), 5) == UpdateRelation::available,
                       "New release not recognized");
        update_require(compare_update(r, std::string(40, 'b'), 7) == UpdateRelation::local_newer,
                       "Development build was downgraded");
        update_require(compare_update(r, "unknown", 0) == UpdateRelation::unknown &&
                           compare_update(r, std::string(40, 'b'), 6) == UpdateRelation::unknown,
                       "Unknown or divergent build ordering was invented");
        std::cout << "PASS update ordering protects newer development builds\n";
        for (const auto &path : {"../CommunityDivers.exe", "docs/../launcher.json", "docs/CON.txt",
                                 "docs/a:stream", "docs/a.", "docs/a\\x", "mods/private.json",
                                 "userdata/profile.json", "docs/a//x", "docs/COM1.txt"})
            update_require(!managed_update_path(path), "Unsafe managed inventory path accepted");
        update_require(managed_update_path("assets/brand/logo.png") &&
                           managed_update_path("CommunityDivers.exe"),
                       "Launcher-owned paths rejected");
        std::cout << "PASS managed-file policy excludes user data and Windows path aliases\n";
        Settings settings;
        settings.check_updates = false;
        update_require(!decode_settings(encode_settings(settings)).check_updates,
                       "Update setting lost");
        auto legacy = encode_settings(settings);
        legacy.erase("check_updates");
        update_require(decode_settings(legacy).check_updates, "Legacy default missing");
        std::cout << "PASS compatible update preferences\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
