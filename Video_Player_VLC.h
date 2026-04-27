#pragma once
#include <windows.h>

#include <d3d9.h>

// For libVLC build error
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;

#include "PhysFS_Loader.h"
#include "vlc/vlc.h"

class VLC_VIDEO
{
public:

VLC_VIDEO();
~VLC_VIDEO();

int Play(const char* filename, HWND AttachWindow = nullptr, IDirect3DDevice9* externalDevice = nullptr);
void Stop();

private:
int vlcdone;
libvlc_instance_t *libvlc;
libvlc_media_t *m;
libvlc_media_player_t *mp;
HWND window;
IDirect3D9 *d3d;
IDirect3DDevice9 *d3dDevice;

bool InitD3D(HWND hwnd, int width, int height);
void CleanupD3D();

HWND CreateVideoWindow(int &outWidth, int &outHeight);
};

static VLC_VIDEO *VLC_Video = new VLC_VIDEO();

int Video_Play_VLC(const char* filename, HWND AttachWindow = nullptr, IDirect3DDevice9* externalDevice = nullptr);