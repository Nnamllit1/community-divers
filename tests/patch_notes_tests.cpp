#include "community/patch_notes.hpp"
#include <iostream>
#include <stdexcept>
using namespace cd;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() {
    try {
        {
            std::vector<std::string> urls;
            auto source = std::string(40, 'a');
            auto notes = fetch_release_notes(source, [&](const std::string &url, uint64_t limit) {
                urls.push_back(url);
                if (url.ends_with("release-notes.md")) {
                    require(limit == 65536, "Markdown download bound changed");
                    return std::string("# "
                                       "Notes\n**Bold**\n![Good](images/example.png)\n![Bad](../"
                                       "private.png)\n![External](https://example.com/x.png)");
                }
                require(limit == 4 * 1024 * 1024, "Image download bound changed");
                return std::string("image");
            });
            require(notes.error.empty() && notes.images.size() == 1 && urls.size() == 2 &&
                        urls[0] == "https://raw.githubusercontent.com/Nnamllit1/community-divers/" +
                                       source + "/docs/release-notes.md",
                    "Release notes escaped the exact project source revision");
            auto invalid = fetch_release_notes("main", [&](const auto &, uint64_t) {
                throw std::runtime_error("Should not fetch");
                return std::string{};
            });
            require(invalid.markdown.empty() && !invalid.error.empty(),
                    "Mutable notes source accepted");
            auto oversized = fetch_release_notes(
                source, [](const auto &, uint64_t) { return std::string(65537, 'x'); });
            require(oversized.markdown.empty() && !oversized.error.empty(),
                    "Oversized notes accepted");
            std::cout << "PASS version-specific release notes and bounded local image paths\n";
        }
        auto future = fetch_release_notes_async(std::string(40, 'a'), [](const auto &, uint64_t) {
            return std::string("# Notes\n**Formatted** release notes.");
        });
        require(future.get().error.empty(), "Injected asynchronous transport failed");
        std::cout << "PASS asynchronous notes without platform or update dependency\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
