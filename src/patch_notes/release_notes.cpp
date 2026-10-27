#include "community/patch_notes.hpp"
#include "community/core.hpp"
#include <algorithm>
#include <cctype>
#include <set>
namespace cd {
bool release_note_image_path(const std::string &path) {
    if (!path.starts_with("images/") || path.size() > 180 || path.find("..") != path.npos ||
        path.find("//") != path.npos ||
        path.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-/.") != path.npos)
        return false;
    auto dot = path.rfind('.');
    if (dot == path.npos)
        return false;
    auto extension = path.substr(dot);
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
}
std::vector<std::string> release_note_images(const std::string &markdown) {
    if (markdown.size() > 65536)
        throw std::runtime_error("Release notes exceed 64 KiB");
    std::vector<std::string> out;
    std::set<std::string> seen;
    for (size_t pos = 0; (pos = markdown.find("![", pos)) != markdown.npos; ++pos) {
        auto start = markdown.find("](", pos + 2);
        if (start == markdown.npos)
            break;
        auto end = markdown.find(')', start + 2);
        if (end == markdown.npos)
            break;
        auto path = markdown.substr(start + 2, end - start - 2);
        if (release_note_image_path(path) && seen.insert(path).second && out.size() < 8)
            out.push_back(path);
        pos = end;
    }
    return out;
}
ReleaseNotes
fetch_release_notes(const std::string &source,
                    const std::function<std::string(const std::string &, uint64_t)> &fetch) {
    ReleaseNotes result;
    result.source_commit = source;
    try {
        if (source.size() != 40 || source.find_first_not_of("0123456789abcdef") != source.npos)
            throw std::runtime_error("Release notes require an exact source revision");
        auto base =
            "https://raw.githubusercontent.com/Nnamllit1/community-divers/" + source + "/docs/";
        result.markdown = fetch(base + "release-notes.md", 65536);
        if (result.markdown.empty())
            throw std::runtime_error("This preview has no release notes");
        auto images = release_note_images(result.markdown);
        for (const auto &image : images) {
            try {
                auto bytes = fetch(base + image, 4 * 1024 * 1024);
                if (bytes.size() > 4 * 1024 * 1024)
                    throw std::runtime_error("Release image exceeds 4 MiB");
                result.images.emplace(image, std::move(bytes));
            } catch (const std::exception &) {
                result.error = "Some release-note images could not be loaded.";
            }
        }
    } catch (const std::exception &e) {
        result.markdown.clear();
        result.images.clear();
        result.error = e.what();
    }
    return result;
}
ReleaseNotes local_release_notes(const fs::path &docs, const std::string &source) {
    ReleaseNotes result;
    result.source_commit = source;
    try {
        result.markdown = read_text(docs / "release-notes.md", 65536);
        for (const auto &image : release_note_images(result.markdown)) {
            try {
                result.images.emplace(image, read_text(docs / from_utf8(image), 4 * 1024 * 1024));
            } catch (const std::exception &) {
                result.error = "Some release-note images could not be loaded.";
            }
        }
    } catch (const std::exception &) {
        result.error = "Release notes are unavailable in this build.";
    }
    return result;
}
} // namespace cd
