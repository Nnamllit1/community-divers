add_library(community_update_core updates.cpp install_inventory.cpp)
target_link_libraries(community_update_core PUBLIC community_core)
if(WIN32)
  add_library(community_maintenance update_crypto_win.cpp update_http_win.cpp update_stage_win.cpp
    update_archive_win.cpp update_install_win.cpp)
  target_link_libraries(community_maintenance PUBLIC community_update_core community_platform
    PRIVATE winhttp shell32 ole32 bcrypt advapi32 community_zip)
  add_executable(CommunityDiversUpdate update_main.cpp)
  target_link_libraries(CommunityDiversUpdate PRIVATE community_maintenance)
endif()
