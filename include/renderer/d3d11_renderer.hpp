#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <directxmath.h>
#include <memory>

struct Vertex {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 tex;
};

class D3D11Renderer {
public:
    D3D11Renderer() = default;
    ~D3D11Renderer();

    bool Initialize(HWND hwnd, int width, int height);
    void Render(const uint8_t* frameData);
    void Resize(int width, int height);
    void Cleanup();

private:
    bool CreateShaders();
    bool CreateBuffers();
    bool CreateTextures(int width, int height);

    ID3D11Device* d3dDevice = nullptr;
    ID3D11DeviceContext* d3dContext = nullptr;
    IDXGISwapChain* swapChain = nullptr;
    ID3D11RenderTargetView* renderTargetView = nullptr;
    ID3D11Texture2D* videoTexture = nullptr;
    ID3D11ShaderResourceView* videoTextureView = nullptr;
    ID3D11SamplerState* samplerState = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer = nullptr;
    ID3D11InputLayout* inputLayout = nullptr;

    int textureWidth = 0;
    int textureHeight = 0;
};
