#include "Video_Player_VLC.h"
#include "PhysFS_Loader.h"

#include <stdlib.h>

static int VIDEOWIDTH = 320;
static int VIDEOHEIGHT = 200;

static const wchar_t* VIDEO_WND_CLASS = L"VIDEO_PLAYER_VLC_DX9";
static bool wndClassRegistered = false;

static LRESULT CALLBACK VideoWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

struct VideoContext {
    IDirect3DDevice9 *device;
    IDirect3DSurface9 *surface;
    CRITICAL_SECTION cs;
    int width;
    int height;
    unsigned char *pixelBuffer;
};

static void *lock(void *data, void **p_pixels) {
    VideoContext *ctx = (VideoContext *)data;
    EnterCriticalSection(&ctx->cs);
    *p_pixels = ctx->pixelBuffer;
    return nullptr;
}

static void unlock(void *data, void *id, void *const *p_pixels) {
    VideoContext *ctx = (VideoContext *)data;
    LeaveCriticalSection(&ctx->cs);
}

static void display(void *data, void *id) {
    VideoContext *ctx = (VideoContext *)data;

    if (!ctx->device || !ctx->surface)
        return;

    EnterCriticalSection(&ctx->cs);

    D3DLOCKED_RECT locked;
    HRESULT hr = ctx->surface->LockRect(&locked, nullptr, D3DLOCK_DISCARD);
    if (SUCCEEDED(hr)) {
        for (int y = 0; y < ctx->height; y++) {
            const unsigned char *src = ctx->pixelBuffer + y * ctx->width * 4;
            unsigned char *dst = (unsigned char *)locked.pBits + y * locked.Pitch;
            for (int x = 0; x < ctx->width; x++) {
                dst[x * 4 + 0] = src[x * 4 + 2]; // B
                dst[x * 4 + 1] = src[x * 4 + 1]; // G
                dst[x * 4 + 2] = src[x * 4 + 0]; // R
                dst[x * 4 + 3] = 0xFF;           // A/X
            }
        }
        ctx->surface->UnlockRect();
    }

    IDirect3DSurface9 *backBuffer = nullptr;
    hr = ctx->device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
    if (SUCCEEDED(hr) && backBuffer) {
        ctx->device->StretchRect(ctx->surface, nullptr, backBuffer, nullptr, D3DTEXF_POINT);
        backBuffer->Release();
    }

    ctx->device->Present(nullptr, nullptr, nullptr, nullptr);

    LeaveCriticalSection(&ctx->cs);
}


VLC_VIDEO::VLC_VIDEO()
    : window(nullptr), d3d(nullptr), d3dDevice(nullptr),
      vlcdone(0), libvlc(nullptr), m(nullptr), mp(nullptr)
{
    char const *vlc_argv[] = {"--play-and-stop"};
    int vlc_argc = sizeof(vlc_argv) / sizeof(*vlc_argv);
    libvlc = libvlc_new(vlc_argc, vlc_argv);

    if (nullptr == libvlc) {
        OutputDebugStringA("LibVLC initialization failure.\n");
    }
}

VLC_VIDEO::~VLC_VIDEO()
{
    if (libvlc) {
        libvlc_release(libvlc);
        libvlc = nullptr;
    }
}

struct VLCMediaBuffer {
    unsigned char *data;
    size_t size;
    size_t pos;
};

static int vlc_media_open(void *opaque, void **datap, UINT64 *sizep)
{
    VLCMediaBuffer *buf = (VLCMediaBuffer *)opaque;
    buf->pos = 0;
    *datap = buf;
    *sizep = (UINT64)buf->size;
    return 0;
}

static ssize_t vlc_media_read(void *opaque, unsigned char *buf_out, size_t len)
{
    VLCMediaBuffer *buf = (VLCMediaBuffer *)opaque;
    size_t remaining = buf->size - buf->pos;
    if (remaining == 0) return 0;
    size_t to_read = len < remaining ? len : remaining;
    CopyMemory(buf_out, buf->data + buf->pos, to_read);
    buf->pos += to_read;
    return (ssize_t)to_read;
}

static int vlc_media_seek(void *opaque, UINT64 offset)
{
    VLCMediaBuffer *buf = (VLCMediaBuffer *)opaque;
    if (offset > (uint64_t)buf->size) return -1;
    buf->pos = (size_t)offset;
    return 0;
}

static void vlc_media_close(void *opaque)
{
    VLCMediaBuffer *buf = (VLCMediaBuffer *)opaque;
    free(buf->data);
    free(buf);
}

HWND VLC_VIDEO::CreateVideoWindow(int &outWidth, int &outHeight)
{
    if (!wndClassRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = VideoWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = VIDEO_WND_CLASS;
        RegisterClassExW(&wc);
        wndClassRegistered = true;
    }

    outWidth = GetSystemMetrics(SM_CXSCREEN);
    outHeight = GetSystemMetrics(SM_CYSCREEN);

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST,
        VIDEO_WND_CLASS,
        L"VLC VIDEO PLAYER",
        WS_POPUP,
        0, 0, outWidth, outHeight,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

    if (hwnd) {
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        SetForegroundWindow(hwnd);
        ShowCursor(TRUE);
    }

    return hwnd;
}

bool VLC_VIDEO::InitD3D(HWND hwnd, int width, int height)
{
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) {
        OutputDebugStringA("Failed to create Direct3D9\n");
        return false;
    }

    D3DPRESENT_PARAMETERS pp = {};
    pp.BackBufferWidth = width;
    pp.BackBufferHeight = height;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.hDeviceWindow = hwnd;
    pp.Windowed = TRUE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    HRESULT hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, hwnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &d3dDevice);
    if (FAILED(hr)) {
        { char _buf[64]; wsprintfA(_buf, "Failed to create D3D9 device: 0x%08X\n", (UINT)hr); OutputDebugStringA(_buf); }
        d3d->Release();
        d3d = nullptr;
        return false;
    }
    return true;
}

void VLC_VIDEO::CleanupD3D()
{
    if (d3dDevice) {
        d3dDevice->Release();
        d3dDevice = nullptr;
    }
    if (d3d) {
        d3d->Release();
        d3d = nullptr;
    }
}

int VLC_VIDEO::Play(const char* filename, HWND AttachWindow, IDirect3DDevice9* externalDevice) {

    if (!libvlc) {
        { char _buf[512]; wsprintfA(_buf, "LibVLC not initialized, skipping video: %s\n", filename); OutputDebugStringA(_buf); }
        return EXIT_FAILURE;
    }

    vlcdone = 0;
    bool ownsWindow = (AttachWindow == nullptr);
    bool ownsDevice = (externalDevice == nullptr);

    if (ownsWindow) {
        int screenW, screenH;
        window = CreateVideoWindow(screenW, screenH);
        if (!window) {
            OutputDebugStringA("Couldn't create video window\n");
            return EXIT_FAILURE;
        }
        VIDEOWIDTH = screenW;
        VIDEOHEIGHT = screenH;
        AttachWindow = window;
    } else {
        RECT clientRect;
        GetClientRect(AttachWindow, &clientRect);
        VIDEOWIDTH = clientRect.right - clientRect.left;
        VIDEOHEIGHT = clientRect.bottom - clientRect.top;
    }

    if (ownsDevice) {
        if (!InitD3D(AttachWindow, VIDEOWIDTH, VIDEOHEIGHT)) {
            if (ownsWindow) { DestroyWindow(window); window = nullptr; }
            return EXIT_FAILURE;
        }
    } else {
        d3dDevice = externalDevice;
    }

    VideoContext context = {};
    context.device = d3dDevice;
    context.width = VIDEOWIDTH;
    context.height = VIDEOHEIGHT;
    context.pixelBuffer = new unsigned char[VIDEOWIDTH * VIDEOHEIGHT * 4]();
    InitializeCriticalSection(&context.cs);

    auto cleanup = [&]() {
        if (context.surface) context.surface->Release();
        delete[] context.pixelBuffer;
        DeleteCriticalSection(&context.cs);
        if (ownsDevice) CleanupD3D(); else d3dDevice = nullptr;
        if (ownsWindow && window) { DestroyWindow(window); window = nullptr; }
        ShowCursor(TRUE);
    };

    HRESULT hr = d3dDevice->CreateOffscreenPlainSurface(
        VIDEOWIDTH, VIDEOHEIGHT, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT,
        &context.surface, nullptr);
    if (FAILED(hr)) {
        { char _buf[64]; wsprintfA(_buf, "Failed to create D3D9 offscreen surface: 0x%08X\n", (UINT)hr); OutputDebugStringA(_buf); }
        cleanup();
        return EXIT_FAILURE;
    }

    DWORD fileAttrs = GetFileAttributesA(filename);
    if (fileAttrs != INVALID_FILE_ATTRIBUTES && !(fileAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
        m = libvlc_media_new_path(libvlc, filename);
        if (!m) {
            { char _buf[512]; wsprintfA(_buf, "Failed to create VLC media for: %s\n", filename); OutputDebugStringA(_buf); }
            cleanup();
            return EXIT_FAILURE;
        }
    } else if (PhysFS_DoesFileExist(filename)) {
        PHYSFS_sint64 fileSize = 0;
        void *fileData = PhysFS_GetFileContents(filename, &fileSize);
        if (!fileData || fileSize <= 0 || fileSize > (PHYSFS_sint64)(size_t)(-1)) {
            { char _buf[512]; wsprintfA(_buf, "Failed to load video from PhysFS: %s\n", filename); OutputDebugStringA(_buf); }
            if (fileData) free(fileData);
            cleanup();
            return EXIT_FAILURE;
        }
        VLCMediaBuffer *buf = (VLCMediaBuffer *)malloc(sizeof(VLCMediaBuffer));
        if (!buf) {
            free(fileData);
            cleanup();
            return EXIT_FAILURE;
        }
        buf->data = (unsigned char *)fileData;
        buf->size = (size_t)fileSize;
        buf->pos = 0;
        m = libvlc_media_new_callbacks(libvlc,
            vlc_media_open, vlc_media_read, vlc_media_seek, vlc_media_close, buf);
        if (!m) {
            { char _buf[512]; wsprintfA(_buf, "Failed to create VLC media callbacks for: %s\n", filename); OutputDebugStringA(_buf); }
            free(fileData);
            free(buf);
            cleanup();
            return EXIT_FAILURE;
        }
    } else {
        { char _buf[512]; wsprintfA(_buf, "Video file not found locally or in PhysFS: %s\n", filename); OutputDebugStringA(_buf); }
        cleanup();
        return EXIT_FAILURE;
    }
    mp = libvlc_media_player_new_from_media(m);
    libvlc_media_release(m);
    m = nullptr;
    if (!mp) {
        OutputDebugStringA("Failed to create VLC media player\n");
        cleanup();
        return EXIT_FAILURE;
    }

    libvlc_video_set_callbacks(mp, lock, unlock, display, &context);
    libvlc_video_set_format(mp, "RGBA", VIDEOWIDTH, VIDEOHEIGHT, VIDEOWIDTH * 4);
    libvlc_media_player_play(mp);

    MSG msg;
    while (!vlcdone) {
        if (libvlc_media_player_get_state(mp) == libvlc_Ended)
            vlcdone = 1;

        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                vlcdone = 1;
                break;
            }
            if (msg.message == WM_KEYDOWN &&
                (msg.wParam == VK_ESCAPE || msg.wParam == VK_SPACE)) {
                vlcdone = 1;
                break;
            }
            if (!ownsWindow && msg.hwnd == AttachWindow) {
                if ((msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST)
                    || msg.message >= WM_APP)
                    continue;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(100);
    }
    libvlc_media_player_stop(mp);
    libvlc_media_player_release(mp);
    cleanup();
    return 0;
}

void VLC_VIDEO::Stop()
{
    vlcdone = 1;
}

int Video_Play_VLC(const char* filename, HWND AttachWindow, IDirect3DDevice9* externalDevice)
{
    if (VLC_Video) {
        return VLC_Video->Play(filename, AttachWindow, externalDevice);
    }
    return -1;
}