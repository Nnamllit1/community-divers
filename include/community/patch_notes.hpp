#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <string>
#include <vector>
namespace cd {
struct ReleaseNotes {
    std::string source_commit, markdown, error;
    std::map<std::string, std::string> images;
};
bool release_note_image_path(const std::string &path);
std::vector<std::string> release_note_images(const std::string &markdown);
ReleaseNotes
fetch_release_notes(const std::string &source_commit,
                    const std::function<std::string(const std::string &, uint64_t)> &fetch);
ReleaseNotes local_release_notes(const std::filesystem::path &docs,
                                 const std::string &source_commit);
using ReleaseNotesFetch = std::function<std::string(const std::string &, uint64_t)>;
std::future<ReleaseNotes> fetch_release_notes_async(std::string source_commit,
                                                    ReleaseNotesFetch fetch);
} // namespace cd
