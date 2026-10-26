#include "update_fixture.hpp"
#include "community/platform.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace cd;
using namespace cd::test;
int main() {
    auto root =
        fs::temp_directory_path() /
        from_utf8("community-divers-updates-" +
                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                  "-\xc3\xa4");
    try {
        auto public_feed = verify_update_envelope(
            read_text(fs::path(CD_SOURCE_DIR) / "updates/preview.json", update_manifest_limit),
            "preview", update_publisher_key());
        update_require(public_feed.source_commit.size() == 40 && public_feed.bytes > 0,
                       "Repository channel does not match the compiled publisher key");
        std::cout << "PASS repository channel authenticates with the compiled trust root\n";
        auto key = generate_update_key();
        auto other = generate_update_key();
        auto payload = update_payload().dump();
        auto signed_text = sign_update_payload(payload, key.private_hex);
        auto r = verify_update_envelope(signed_text, "preview", key.public_hex);
        update_require(r.bytes == 3, "Verified release lost its fields");
        auto changed = json::parse(signed_text);
        changed["payload"] = payload + " ";
        update_rejects([&] { verify_update_envelope(changed.dump(), "preview", key.public_hex); });
        update_rejects([&] { verify_update_envelope(signed_text, "preview", other.public_hex); });
        update_rejects([&] { verify_update_envelope(signed_text, "stable", key.public_hex); });
        changed = json::parse(signed_text);
        changed["signature"] = std::string(128, '0');
        update_rejects([&] { verify_update_envelope(changed.dump(), "preview", key.public_hex); });
        update_rejects([&] { verify_update_envelope(signed_text, "preview", ""); });
        std::cout << "PASS publisher signatures reject tampering, wrong keys and channels\n";
        auto checked = check_update("fixture", key.public_hex, [&](const auto &, uint64_t limit) {
            update_require(limit == update_manifest_limit, "Unbounded manifest request");
            return signed_text;
        });
        update_require(checked.release.has_value(), "Authenticated check failed");
        auto invalid =
            check_update("fixture", key.public_hex, [&](const auto &, uint64_t) { return "bad"; });
        update_require(!invalid.release && !invalid.error.empty(),
                       "Invalid check returned a release");
        std::cout << "PASS only authenticated checks expose a release\n";
        for (const auto &url :
             {"http://github.com/file", "https://github.com.evil/file", "https://github.com:80/a",
              "https://user:pass@github.com/a", "https://127.0.0.1/a", "https://github.com/a#b",
              "https://github.com\\@evil.test/a"})
            update_require(!update_transport_url_allowed(url), "Unsafe update URL permitted");
        update_require(update_transport_url_allowed(
                           "https://release-assets.githubusercontent.com/x?token=public-download"),
                       "GitHub asset redirect rejected");
        std::cout << "PASS HTTPS-only bounded redirect host policy\n";
        auto stream = [](const std::string &, uint64_t limit, const UpdateSink &sink) {
            update_require(limit == 3, "Download not bounded to signed package size");
            sink("a");
            sink("bc");
        };
        auto staged = stage_update(r, root, stream);
        update_require(staged.error.empty() && read_text(staged.package) == "abc",
                       "Package staging failed");
        update_require(staged.package.filename() == from_utf8(r.sha256 + ".zip"),
                       "Untrusted local filename used");
        auto cached = stage_update(r, root, [](const auto &, uint64_t, const UpdateSink &) {
            throw std::runtime_error("Cache should have been reused");
        });
        update_require(cached.error.empty(), "Verified cache was not reused");
        std::cout << "PASS streamed Unicode staging and verified cache reuse\n";
        {
            std::ofstream(staged.package, std::ios::binary | std::ios::trunc) << "bad";
        }
        auto mismatch = stage_update(
            r, root, [](const auto &, uint64_t, const UpdateSink &sink) { sink("xyz"); });
        update_require(!mismatch.error.empty() && read_text(staged.package) == "bad",
                       "Corrupted download was installed over the cache");
        auto truncated = stage_update(
            r, root, [](const auto &, uint64_t, const UpdateSink &sink) { sink("a"); });
        auto oversized = stage_update(
            r, root, [](const auto &, uint64_t, const UpdateSink &sink) { sink("abcd"); });
        auto interrupted =
            stage_update(r, root, [](const auto &, uint64_t, const UpdateSink &sink) {
                sink("a");
                throw std::runtime_error("Connection interrupted");
            });
        update_require(!truncated.error.empty() && !oversized.error.empty() &&
                           !interrupted.error.empty(),
                       "Incomplete transfer was accepted");
        for (const auto &entry : fs::directory_iterator(root))
            update_require(entry.path().extension() != ".partial",
                           "Failed transfer left a staging file");
        std::cout
            << "PASS digest mismatch, truncation, overflow and interruption preserve prior files\n";
        auto restored = stage_update(r, root, stream);
        update_require(restored.error.empty() && read_text(restored.package) == "abc",
                       "Corrupted cache was not repaired");
        auto invalid_release = r;
        invalid_release.sha256 = "../../escape";
        update_require(!stage_update(invalid_release, root, stream).error.empty(),
                       "Stage path escaped through digest");
        std::cout << "PASS corrupt cache repair and staging path validation\n";
        fs::remove_all(root);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
