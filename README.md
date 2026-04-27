# Video_Player_VLC_DX9

Play videos using lib VLC in a new fullscreen window or pass through to an already existing DX9 window and render directly to that target. 

Also supports .zip file packaging using PhysFS

https://github.com/ActingBadly/PhysFS_Loader.git

# Build instructions

Clone the repo and build with cmake.

Inside the build folder grab "vlc.dll", "vlccore.dll" and the "plugins" folder and drop them in you projects build folder. Then go to your projects cmake and add the following.

include(FetchContent)

FetchContent_Declare(
  Video_Player_VLC_DX9
  GIT_REPOSITORY https://github.com/ActingBadly/Video_Player_VLC_DX9.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(Video_Player_VLC_DX9)
target_include_directories(Video_Player_VLC_DX9 PUBLIC ${video_player_vlc_dx9_SOURCE_DIR})

Make sure to add "Video_Player_VLC_DX9" & "d3d9" to your target_link_libraries

ex:
    target_link_libraries(My_Program PRIVATE Video_Player_VLC_DX9 d3d9 Other)

When you build your project it will build physFS into your .exe or .dll

# Usage

#include "VLC_Video_Player.h"

PhysFS_LoadZip("Videos/Videos.zip","/Videos");

VLC_Video_Play("Videos/Video_1.mp4"); // New Fullscreen Window
VLC_Video_Play("Videos/Video_2.mp4", _window, _device); // DX9 Linked Window
