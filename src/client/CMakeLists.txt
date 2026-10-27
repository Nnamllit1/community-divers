add_library(community_client compatibility.cpp depot_manifest.cpp prepared_identity.cpp)
target_link_libraries(community_client PUBLIC community_core)
if(WIN32)
  add_executable(CommunityDiversInspect inspect_main.cpp preparation_cli.cpp)
  target_link_libraries(CommunityDiversInspect PRIVATE community_platform)
  add_custom_target(community_catalog ALL COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/compatibility" "$<TARGET_FILE_DIR:CommunityDiversInspect>/compatibility")
  add_dependencies(community_catalog CommunityDiversInspect)
endif()
