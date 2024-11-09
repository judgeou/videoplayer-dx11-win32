#pragma once
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "renderer/d3d11_renderer.hpp"
#include <Windows.h>

class PlayerUI {
public:
    PlayerUI() = default;
    ~PlayerUI();

    bool Initialize(HWND hwnd, D3D11Renderer* renderer);
    void Render();
    void Shutdown();
    
    // 处理窗口消息
    static LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    bool initialized = false;
};
