#include "community/platform.hpp"
#include "preparation_cli.hpp"
#include <fstream>
#include <algorithm>
#include <iostream>
int wmain(int argc, wchar_t **argv) {
    if (argc > 1) {
        std::wstring first = argv[1];
        if (first == L"--plan" || first == L"--copy" || first == L"--verify")
            return cd::run_preparation_cli(argc, argv);
    }
    try {
        cd::fs::path game, output,
            catalog = cd::executable_directory() / L"compatibility/client-builds.json";
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--game" && i + 1 < argc)
                game = argv[++i];
            else if (arg == L"--output" && i + 1 < argc)
                output = argv[++i];
            else if (arg == L"--catalog" && i + 1 < argc)
                catalog = argv[++i];
            else {
                std::cerr << "Usage: CommunityDiversInspect --game <directory> [--catalog <file>] "
                             "[--output <file>]\n";
                return 2;
            }
        }
        if (game.empty())
            throw std::runtime_error("A game directory is required");
        cd::json profiles = {{"schema", 1}, {"profiles", cd::json::array()}};
        if (cd::fs::exists(catalog))
            profiles = cd::json::parse(cd::read_text(catalog));
        auto identity = cd::inspect_client(game);
        auto report = cd::identity_report(identity, profiles);
        if (output.empty())
            std::cout << report.dump(2) << '\n';
        else {
            if (!output.parent_path().empty())
                cd::fs::create_directories(output.parent_path());
            std::ofstream file(output, std::ios::binary);
            file.exceptions(std::ios::failbit | std::ios::badbit);
            file << report.dump(2) << '\n';
            std::cout << "Client identity report written. Integration remains unavailable.\n";
        }
        return std::any_of(identity.files.begin(), identity.files.end(),
                           [](const auto &f) { return !f.error.empty(); })
                   ? 1
                   : 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}
