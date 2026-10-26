#include "update_fixture.hpp"
#include <iostream>
using namespace cd;
using namespace cd::test;
int main() {
    try {
        auto j = update_payload();
        auto r = parse_update_payload(j.dump(), "preview");
        update_require(r.bytes == 3 && r.revision == 6, "Signed payload fields lost");
        std::cout << "PASS update package identity and bounds\n";
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
