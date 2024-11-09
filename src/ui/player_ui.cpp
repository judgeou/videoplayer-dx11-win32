#include "ui/player_ui.hpp"

PlayerUI::~PlayerUI() {
    Shutdown();
}

bool PlayerUI::Initialize(HWND hwnd, D3D11Renderer* renderer) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    if (!ImGui_ImplWin32_Init(hwnd) || !ImGui_ImplDX11_Init(renderer->GetDevice(), renderer->GetContext())) {
        return false;
    }

    initialized = true;
    return true;
}

void PlayerUI::Render() {
    if (!initialized) return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    ImGui::ShowDemoWindow(); // Show demo window! :)
    
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void PlayerUI::Shutdown() {
    if (initialized) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        initialized = false;
    }
}

LRESULT PlayerUI::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
}
