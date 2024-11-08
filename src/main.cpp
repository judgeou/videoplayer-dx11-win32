#include <imgui.h>
#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <directxmath.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}
#include "decoder/ffmpeg_decoder.hpp"
#include "renderer/d3d11_renderer.hpp"

// 添加全局变量用于存储视频帧
struct VideoState {
    uint8_t* frameBuffer;
    int width;
    int height;
    std::unique_ptr<D3D11Renderer> renderer;
} videoState;

bool InitD3D11(HWND hwnd);

// 解码第一帧的函数
bool DecodeFirstFrame(const wchar_t* filename) {
    FFmpegDecoder decoder;
    if (!decoder.OpenFile(filename)) {
        return false;
    }
    
    return decoder.DecodeFirstFrame(&videoState.frameBuffer, &videoState.width, &videoState.height);
}

// 窗口过程函数声明
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow
) {
    // 设置文件选择对话框
    OPENFILENAMEW ofn = { 0 };
    WCHAR szFile[260] = { 0 };
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"视频文件 (*.mp4;*.avi;*.mkv;*.flv)\0*.mp4;*.avi;*.mkv;*.flv\0所有文件\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    // 显示文件选择对话框
    if (!GetOpenFileNameW(&ofn)) {
        return 1;  // 用户取消或发生错误
    }

    // 解码第一帧
    if (!DecodeFirstFrame(ofn.lpstrFile)) {
        MessageBoxW(NULL, L"无法解码视频文件", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 注册窗口类
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"VideoPlayerClass";
    
    RegisterClassEx(&wc);

    // 创建窗口，使用选择的文件路径作为窗口标题
    HWND hwnd = CreateWindowEx(
        0,
        L"VideoPlayerClass",
        ofn.lpstrFile,  // 使用文件路径作为窗口标题
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (hwnd == nullptr) {
        return 1;
    }

    // 初始化 D3D11
    if (!InitD3D11(hwnd)) {
        MessageBoxW(NULL, L"初始化 D3D11 失败", L"错误", MB_OK | MB_ICONERROR);
        return 1;
    }

    // 显示窗口
    ShowWindow(hwnd, SW_SHOW);

    // 消息循环
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

// 窗口过程函数实现
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            if (videoState.frameBuffer && videoState.renderer) {
                videoState.renderer->Render(videoState.frameBuffer);
            }
            return 0;
        }
        
        case WM_SIZE: {
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        
        case WM_DESTROY: {
            if (videoState.frameBuffer) {
                free(videoState.frameBuffer);
                videoState.frameBuffer = nullptr;
            }
            videoState.renderer.reset();
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool InitD3D11(HWND hwnd) {
    videoState.renderer = std::make_unique<D3D11Renderer>();
    return videoState.renderer->Initialize(hwnd, videoState.width, videoState.height);
}