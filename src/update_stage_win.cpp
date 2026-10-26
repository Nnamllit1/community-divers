#include <windows.h>
#include "community/updates.hpp"
#include "community/platform.hpp"
#include <algorithm>
#include <memory>
#include <thread>
namespace cd {
namespace {
struct StageFile {
    fs::path path;
    HANDLE handle = INVALID_HANDLE_VALUE;
    bool keep = false;
    explicit StageFile(const fs::path &p) : path(p) {
        handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot create update staging file");
    }
    void close() {
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
    }
    ~StageFile() {
        close();
        if (!keep) {
            std::error_code e;
            fs::remove(path, e);
        }
    }
};
} // namespace
UpdateDownload stage_update(const UpdateRelease &r, const fs::path &directory,
                            const UpdateStream &fetch) {
    try {
        // Revalidate identifiers before using any of them as a local path.
        json j{{"schema", 1},
               {"platform", "windows-x64"},
               {"channel", r.channel},
               {"release_id", r.release_id},
               {"source_commit", r.source_commit},
               {"source_revision", r.revision},
               {"bytes", r.bytes},
               {"sha256", r.sha256},
               {"filename", r.filename},
               {"url", r.url},
               {"service_protocol", r.service_protocol}};
        parse_update_payload(j.dump(), "preview");
        fs::create_directories(directory);
        const auto target = directory / from_utf8(r.sha256 + ".zip");
        // Existing packages are accepted only after a fresh fingerprint check.
        auto cached = fingerprint_file(target, "cached-update");
        if (cached.error.empty() && cached.bytes == r.bytes && cached.sha256 == r.sha256)
            return {target, {}};
        LARGE_INTEGER counter{};
        QueryPerformanceCounter(&counter);
        auto temp = directory / from_utf8(r.sha256 + "." + std::to_string(GetCurrentProcessId()) +
                                          "." + std::to_string(counter.QuadPart) + ".partial");
        StageFile file(temp);
        uint64_t total = 0;
        fetch(r.url, r.bytes, [&](std::string_view block) {
            if (block.size() > r.bytes - total)
                throw std::runtime_error("Downloaded update exceeds its signed size");
            size_t offset = 0;
            while (offset < block.size()) {
                DWORD written = 0;
                auto n = static_cast<DWORD>(std::min<size_t>(1024 * 1024, block.size() - offset));
                if (!WriteFile(file.handle, block.data() + offset, n, &written, nullptr) ||
                    !written)
                    throw std::runtime_error("Cannot write downloaded update");
                offset += written;
            }
            total += block.size();
        });
        if (total != r.bytes)
            throw std::runtime_error("Downloaded update size does not match the signed manifest");
        if (!FlushFileBuffers(file.handle))
            throw std::runtime_error("Cannot flush downloaded update");
        file.close();
        auto actual = fingerprint_file(temp, "download");
        if (!actual.error.empty() || actual.bytes != r.bytes || actual.sha256 != r.sha256)
            throw std::runtime_error("Downloaded update failed SHA-256 verification");
        if (!MoveFileExW(temp.c_str(), target.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot finish update staging");
        file.keep = true;
        return {target, {}};
    } catch (const std::exception &e) {
        return {{}, e.what()};
    }
}
std::future<UpdateCheck> check_update_async() {
    auto promise = std::make_shared<std::promise<UpdateCheck>>();
    auto future = promise->get_future();
    std::thread([promise] {
        try {
            promise->set_value(
                check_update(std::string(preview_feed), update_publisher_key(), fetch_update_text));
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }).detach();
    return future;
}
std::future<UpdateDownload> stage_update_async(UpdateRelease release, fs::path directory) {
    auto promise = std::make_shared<std::promise<UpdateDownload>>();
    auto future = promise->get_future();
    std::thread([promise, release = std::move(release), directory = std::move(directory)] {
        try {
            promise->set_value(stage_update(release, directory, stream_update));
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }).detach();
    return future;
}
} // namespace cd
