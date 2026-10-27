#include "note_transport.hpp"
namespace cd {
ReleaseNotesFetch make_note_transport(UpdateStream stream) {
    return [stream = std::move(stream)](const std::string &url, uint64_t limit) {
        std::string bytes;
        // Notes enforce separate text/image bounds. The updater's manifest-only
        // text helper intentionally has a smaller limit, so use its bounded stream.
        stream(url, limit, [&](std::string_view part) { bytes.append(part); });
        return bytes;
    };
}
std::future<ReleaseNotes> fetch_launcher_notes(std::string source) {
    return fetch_release_notes_async(std::move(source), make_note_transport(stream_update));
}
} // namespace cd
