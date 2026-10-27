#include "community/patch_notes.hpp"
#include <thread>
namespace cd {
std::future<ReleaseNotes> fetch_release_notes_async(std::string source, ReleaseNotesFetch fetch) {
    auto promise = std::make_shared<std::promise<ReleaseNotes>>();
    auto future = promise->get_future();
    std::thread([promise, source = std::move(source), fetch = std::move(fetch)] {
        try {
            promise->set_value(fetch_release_notes(source, fetch));
        } catch (...) {
            promise->set_exception(std::current_exception());
        }
    }).detach();
    return future;
}
} // namespace cd
