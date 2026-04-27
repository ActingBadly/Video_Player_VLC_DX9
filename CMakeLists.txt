cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

add_library(Video_Player_VLC_DX9 STATIC
  Video_Player_VLC.cpp
)

target_compile_features(Video_Player_VLC_DX9 PUBLIC cxx_std_17)
target_compile_definitions(Video_Player_VLC_DX9 PRIVATE _WIN32_WINNT=0x0602)  # Windows 8+ required by XAudio2
target_include_directories(Video_Player_VLC_DX9 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Fetch prebuilt libVLC binaries for Windows using FetchContent
include(FetchContent)

# libVLC prebuilt binaries (example: v3.0.20-win64)
FetchContent_Declare(
  libvlc_bin
  URL https://download.videolan.org/pub/videolan/vlc/3.0.20/win32/vlc-3.0.20-win32.7z
  SOURCE_DIR ${CMAKE_BINARY_DIR}/libvlc_bin
)
FetchContent_MakeAvailable(libvlc_bin)

# Set libVLC include and library paths
set(LIBVLC_INCLUDE_DIR ${CMAKE_BINARY_DIR}/libvlc_bin/sdk/include)
set(LIBVLC_LIBRARY ${CMAKE_BINARY_DIR}/libvlc_bin/sdk/lib/libvlc.lib)
set(LIBVLCCORE_LIBRARY ${CMAKE_BINARY_DIR}/libvlc_bin/sdk/lib/libvlccore.lib)

include_directories(${LIBVLC_INCLUDE_DIR})
#target_link_libraries(Video_Player_VLC_DX9 ${LIBVLC_LIBRARY} ${LIBVLCCORE_LIBRARY})

# Link Direct3D 9 and shell utilities
#target_link_libraries(Video_Player_VLC_DX9 d3d9)

# PhysFS
FetchContent_Declare(
  physfs
  GIT_REPOSITORY https://github.com/icculus/physfs.git
  GIT_TAG main
)
FetchContent_MakeAvailable(physfs)

# PhysFS Loader
FetchContent_Declare(
  PhysFS_Loader
  GIT_REPOSITORY https://github.com/ActingBadly/PhysFS_Loader.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(PhysFS_Loader)

#target_link_libraries(Video_Player_VLC_DX9 PhysFS_Loader)

target_link_libraries(Video_Player_VLC_DX9 ${LIBVLC_LIBRARY} ${LIBVLCCORE_LIBRARY} d3d9 PhysFS_Loader)
