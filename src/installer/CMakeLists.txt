enable_language(C)
FetchContent_Declare(miniz URL https://codeload.github.com/richgel999/miniz/tar.gz/refs/tags/3.1.2
  URL_HASH SHA256=98468f8924934b723276680f85238b6c78bf1f8b49b4459cc9b7214a20e2e9fb
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE SOURCE_SUBDIR not-added)
FetchContent_MakeAvailable(miniz)
file(WRITE "${CMAKE_BINARY_DIR}/generated/miniz_export.h" "#pragma once\n#define MINIZ_EXPORT\n")
add_library(community_zip STATIC ${miniz_SOURCE_DIR}/miniz.c ${miniz_SOURCE_DIR}/miniz_zip.c
  ${miniz_SOURCE_DIR}/miniz_tdef.c ${miniz_SOURCE_DIR}/miniz_tinfl.c)
target_include_directories(community_zip PUBLIC "${miniz_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/generated")
target_compile_definitions(community_zip PUBLIC MINIZ_NO_STDIO MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
# Setup and portable updates use the same lock, inventory and ownership implementation.
target_sources(community_maintenance PRIVATE setup_win.cpp)
if(COMMUNITY_BUILD_LAUNCHER)
  enable_language(RC)
  add_executable(CommunityDiversSetup WIN32 setup_main.cpp setup_bundle_win.cpp
    setup_registration_win.cpp setup.rc setup.manifest)
  target_compile_definitions(CommunityDiversSetup PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(CommunityDiversSetup PRIVATE community_maintenance dwmapi windowscodecs shell32 ole32)
  install(FILES ${miniz_SOURCE_DIR}/LICENSE DESTINATION licenses RENAME miniz.txt)
endif()
