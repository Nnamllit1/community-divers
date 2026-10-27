add_library(community_platform platform_win.cpp player_credentials_win.cpp identity_win.cpp
  ../client/preparation_win.cpp)
target_link_libraries(community_platform PUBLIC community_core community_client PRIVATE winhttp shell32 ole32 bcrypt advapi32)
