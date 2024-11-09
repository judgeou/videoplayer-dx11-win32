#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
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
#include "ui/player_ui.hpp"

// 添加全局变量用于存储视频帧
struct VideoState {
    uint8_t* frameBuffer;
    int width;
    int height;
    std::unique_ptr<D3D11Renderer> renderer;
    std::unique_ptr<PlayerUI> ui;
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

bool InitImGui(HWND hwnd, D3D11Renderer* renderer) {
    videoState.ui = std::make_unique<PlayerUI>();
    return videoState.ui->Initialize(hwnd, renderer);
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
    if (!InitImGui(hwnd, videoState.renderer.get())) {
        MessageBoxW(NULL, L"初始化 ImGui 失败", L"错误", MB_OK | MB_ICONERROR);
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
    if (videoState.ui && videoState.ui->HandleMessage(hwnd, uMsg, wParam, lParam)) {
        return true;
    }

    switch (uMsg) {
        case WM_PAINT: {
            if (videoState.frameBuffer && videoState.renderer) {
                videoState.renderer->Render(videoState.frameBuffer);
                videoState.ui->Render();
                videoState.renderer->Present(1);
            }
            return 0;
        }
        
        case WM_SIZE: {
            if (videoState.renderer) {
                // 获取新的窗口尺寸
                UINT width = LOWORD(lParam);
                UINT height = HIWORD(lParam);
                videoState.renderer->Resize(width, height);
            }
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        
        case WM_DESTROY: {
            if (videoState.frameBuffer) {
                free(videoState.frameBuffer);
                videoState.frameBuffer = nullptr;
            }
            videoState.ui.reset();
            videoState.renderer.reset();
            PostQuitMessage(0);
            return 0;
        }

        case WM_SETCURSOR: {
            // 当鼠标在客户区时，设置为默认箭头光标
            if (LOWORD(lParam) == HTCLIENT) {
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                return TRUE;  // 返回TRUE表示我们已经处理了这个消息
            }
            break;  // 对于非客户区，使用默认处理
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool InitD3D11(HWND hwnd) {
    videoState.renderer = std::make_unique<D3D11Renderer>();
    return videoState.renderer->Initialize(hwnd, videoState.width, videoState.height);
}