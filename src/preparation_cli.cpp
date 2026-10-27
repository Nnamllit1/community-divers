#include <windows.h>
#include "community/preparation.hpp"
#include "community/platform.hpp"
#include "preparation_cli.hpp"
#include <iostream>
int cd::run_preparation_cli(int argc, wchar_t **argv) {
    try {
        cd::fs::path source, cache, destination, output;
        std::wstring action;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if ((arg == L"--plan" || arg == L"--copy" || arg == L"--verify") && action.empty())
                action = arg;
            else if (arg == L"--source" && i + 1 < argc && source.empty())
                source = argv[++i];
            else if (arg == L"--depot-cache" && i + 1 < argc && cache.empty())
                cache = argv[++i];
            else if (arg == L"--destination" && i + 1 < argc && destination.empty())
                destination = argv[++i];
            else if (arg == L"--output" && i + 1 < argc && output.empty())
                output = argv[++i];
            else
                throw std::runtime_error("Unknown, duplicate or incomplete argument");
        }
        if (action.empty() || (action != L"--verify" && (source.empty() || cache.empty())) ||
            (action != L"--plan" && destination.empty()) ||
            (action == L"--verify" && (!source.empty() || !cache.empty())) ||
            (action == L"--plan" && !destination.empty()) ||
            (action == L"--copy" && !output.empty())) {
            std::cerr
                << "Usage:\n  CommunityDiversInspect --plan --source GAME --depot-cache CACHE "
                   "[--output JSON]\n"
                   "  CommunityDiversInspect --copy --source GAME --depot-cache CACHE "
                   "--destination NEW_DIRECTORY\n"
                   "  CommunityDiversInspect --verify --destination DIRECTORY [--output JSON]\n";
            return 2;
        }
        auto progress = [](size_t current, size_t count, const std::string &file) {
            std::cerr << current << '/' << count << ' ' << file << '\n';
        };
        cd::json report;
        if (action == L"--plan")
            report = cd::preparation_report(cd::plan_client_copy(source, cache));
        else if (action == L"--copy") {
            cd::prepare_client_copy(source, cache, destination, progress);
            std::cout << "Independent game copy prepared. Game startup, profile/Cloud isolation "
                         "and official-network isolation remain unverified.\n";
            return 0;
        } else
            report = cd::verify_client_copy(destination, progress);
        if (output.empty())
            std::cout << report.dump(2) << '\n';
        else {
            // Never overwrite an existing report or an input/receipt by mistake.
            auto file = CreateFileW(output.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
                throw std::runtime_error("Report cannot be created, or already exists");
            auto body = report.dump(2) + "\n";
            DWORD written = 0;
            bool saved =
                body.size() <= MAXDWORD &&
                WriteFile(file, body.data(), static_cast<DWORD>(body.size()), &written, nullptr) &&
                written == body.size() && FlushFileBuffers(file);
            CloseHandle(file);
            if (!saved)
                throw std::runtime_error("Cannot write complete preparation report");
            std::cout << "Preparation report written. This does not authorize game launch.\n";
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
