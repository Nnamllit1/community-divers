#pragma once
#include "community/patch_notes.hpp"
#include "community/updates.hpp"
namespace cd {
ReleaseNotesFetch make_note_transport(UpdateStream stream);
std::future<ReleaseNotes> fetch_launcher_notes(std::string source);
} // namespace cd
