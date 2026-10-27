find_package(Threads REQUIRED)
add_library(community_patch_notes release_notes.cpp fetch.cpp)
target_link_libraries(community_patch_notes PUBLIC community_core PRIVATE Threads::Threads)
# Each ImGui build gets its own renderer. No application state enters this module.
function(community_notes_view target imgui_target)
  add_library(${target} STATIC "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/view.cpp")
  target_include_directories(${target} PRIVATE "${markdown_SOURCE_DIR}")
  target_link_libraries(${target} PUBLIC community_patch_notes ${imgui_target})
endfunction()
