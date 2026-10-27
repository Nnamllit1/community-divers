#include "note_transport.hpp"
#include <iostream>
int main() {
    try {
        const std::string image(128 * 1024, '\0');
        auto transport = cd::make_note_transport(
            [&](const std::string &url, uint64_t limit, const cd::UpdateSink &sink) {
                if (url.ends_with("release-notes.md")) {
                    if (limit != 65536)
                        throw std::runtime_error("Markdown bound changed");
                    sink("# Notes\n![Preview](images/preview.png)\n");
                } else {
                    if (limit != 4 * 1024 * 1024)
                        throw std::runtime_error("Image bound changed");
                    // A binary image may exceed the text manifest limit and arrive in chunks.
                    sink(std::string_view(image).substr(0, 100000));
                    sink(std::string_view(image).substr(100000));
                }
            });
        auto notes = cd::fetch_release_notes(std::string(40, 'a'), transport);
        if (!notes.error.empty() || notes.images.at("images/preview.png") != image)
            throw std::runtime_error("Application note transport lost the binary image");
        std::cout << "PASS streamed note images exceed manifest size without truncation\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
