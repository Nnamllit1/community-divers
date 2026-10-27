#include "community/updates.hpp"
#include "community/installer.hpp"
#include "community/setup.hpp"
#include "community/platform.hpp"
#include "community/version.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
namespace {
void write_new(const cd::fs::path &p, const std::string &value) {
    if (cd::fs::exists(p))
        throw std::runtime_error("Output already exists; refusing to overwrite it");
    if (!p.parent_path().empty())
        cd::fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << value;
    out.close();
}
} // namespace
int wmain(int count, wchar_t **args) {
    try {
        if (count == 3 && std::wstring(args[1]) == L"--setup-check") {
            cd::check_setup_destination(args[2]);
            return 0;
        }
        if (count == 4 && std::wstring(args[1]) == L"--setup-begin") {
            cd::begin_setup_install(args[2], cd::utf8(args[3]));
            return 0;
        }
        if (count == 4 && std::wstring(args[1]) == L"--setup-register") {
            cd::register_setup_install(args[2], cd::utf8(args[3]));
            return 0;
        }
        if (count == 3 && std::wstring(args[1]) == L"--uninstall-managed") {
            auto result = cd::uninstall_managed_launcher(args[2]);
            std::cout << "Removed " << result.deleted << " launcher files; kept "
                      << result.preserved << " modified files.\n";
            return 0;
        }
        if (count == 3 && std::wstring(args[1]) == L"--inspect-feed") {
            auto body = cd::read_text(args[2], cd::update_manifest_limit);
            cd::verify_update_envelope(body, "preview", cd::update_publisher_key());
            std::cout << cd::json::parse(body).at("payload").get<std::string>() << '\n';
            return 0;
        }
        if (count == 4 && std::wstring(args[1]) == L"--sign-env") {
            char *raw = nullptr;
            size_t length = 0;
            if (_dupenv_s(&raw, &length, "CD_UPDATE_SIGNING_KEY") || !raw)
                throw std::runtime_error("CD_UPDATE_SIGNING_KEY is not set");
            std::string key(raw);
            std::fill_n(raw, length, '\0');
            std::free(raw);
            auto signed_feed = cd::sign_update_payload(cd::read_text(args[2], 32768), key);
            std::fill(key.begin(), key.end(), '\0');
            cd::verify_update_envelope(signed_feed, "preview", cd::update_publisher_key());
            write_new(args[3], signed_feed);
            std::cout << "Signed feed matches the compiled publisher key.\n";
            return 0;
        }
        if (count == 5 && std::wstring(args[1]) == L"--verify-package") {
            auto release =
                cd::verify_update_envelope(cd::read_text(args[3], cd::update_manifest_limit),
                                           "preview", cd::update_publisher_key());
            auto files = cd::extract_update_bundle(args[2], release, args[4]);
            std::cout << "Verified and extracted " << files.size() << " owned launcher files.\n";
            return 0;
        }
        if (count == 3 &&
            (std::wstring(args[1]) == L"--apply" || std::wstring(args[1]) == L"--recover"))
            return cd::run_update_installer(args[2], cd::update_publisher_key(),
                                            std::wstring(args[1]) == L"--recover");
        if (count == 3 && std::wstring(args[1]) == L"--generate-key") {
            cd::fs::path prefix(args[2]);
            auto private_path = prefix;
            private_path += ".private";
            auto public_path = prefix;
            public_path += ".public";
            if (cd::fs::exists(private_path) || cd::fs::exists(public_path))
                throw std::runtime_error("Key files already exist");
            auto key = cd::generate_update_key();
            write_new(private_path, key.private_hex + "\n");
            write_new(public_path, key.public_hex + "\n");
            std::cout
                << "Publisher key files created. Keep the .private file outside source control.\n";
            return 0;
        }
        if (count == 5 && std::wstring(args[1]) == L"--sign") {
            auto key = cd::read_text(args[3], 4096);
            while (!key.empty() && (key.back() == '\r' || key.back() == '\n'))
                key.pop_back();
            write_new(args[4], cd::sign_update_payload(cd::read_text(args[2], 32768), key));
            std::cout << "Signed update manifest written.\n";
            return 0;
        }
        if ((count == 2 && std::wstring(args[1]) == L"--check") ||
            (count == 3 && std::wstring(args[1]) == L"--download")) {
            auto result = cd::check_update(std::string(cd::preview_feed),
                                           cd::update_publisher_key(), cd::fetch_update_text);
            if (!result.release)
                throw std::runtime_error(result.error);
            const auto &r = *result.release;
            std::cout << "Authenticated preview release " << r.release_id << " (revision "
                      << r.revision << ", " << r.bytes << " bytes)\n";
            if (count == 3) {
                auto staged = cd::stage_update(r, args[2], cd::stream_update);
                if (!staged.error.empty())
                    throw std::runtime_error(staged.error);
                std::cout << "SHA-256 verified package: " << cd::path_text(staged.package) << '\n';
            }
            return 0;
        }
        std::cout << "CommunityDiversUpdate --check\n"
                     "CommunityDiversUpdate --download <staging-directory>\n"
                     "CommunityDiversUpdate --verify-package <ZIP> <signed-feed> <new-directory>\n"
                     "Maintainers: --inspect-feed <signed-feed.json>\n"
                     "Maintainers: --sign-env <payload.json> <output.json>\n"
                     "Maintainers: --generate-key <private-prefix>\n"
                     "Maintainers: --sign <payload.json> <private-key> <output.json>\n";
        return 2;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
