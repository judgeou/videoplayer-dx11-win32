#include "renderer/d3d11_renderer.hpp"
#include <d3dcompiler.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

D3D11Renderer::~D3D11Renderer() {
    Cleanup();
}

void D3D11Renderer::Cleanup() {
    if (samplerState) samplerState->Release();
    if (videoTextureView) videoTextureView->Release();
    if (videoTexture) videoTexture->Release();
    if (renderTargetView) renderTargetView->Release();
    if (swapChain) swapChain->Release();
    if (d3dContext) d3dContext->Release();
    if (d3dDevice) d3dDevice->Release();
    if (vertexShader) vertexShader->Release();
    if (pixelShader) pixelShader->Release();
    if (inputLayout) inputLayout->Release();
    if (vertexBuffer) vertexBuffer->Release();
    if (indexBuffer) indexBuffer->Release();
}

bool D3D11Renderer::Initialize(HWND hwnd, int width, int height) {
    textureWidth = width;
    textureHeight = height;
    
    // 创建设备和交换链
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    if (FAILED(D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        featureLevels, 1, D3D11_SDK_VERSION,
        &scd, &swapChain, &d3dDevice,
        nullptr, &d3dContext))) {
        return false;
    }

    // 创建渲染目标视图
    ID3D11Texture2D* backBuffer;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    d3dDevice->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
    backBuffer->Release();

    if (!CreateTextures(width, height)) return false;
    if (!CreateShaders()) return false;
    if (!CreateBuffers()) return false;

    return true;
}

void D3D11Renderer::Render(const uint8_t* frameData) {
    if (!frameData || !d3dContext || !videoTexture) return;

    // 更新纹理数据
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    d3dContext->Map(videoTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    
    // 复制帧数据到纹理，BGR -> BGRA 转换
    uint8_t* dest = (uint8_t*)mappedResource.pData;
    const uint8_t* src = frameData;
    for (int y = 0; y < textureHeight; y++) {
        uint8_t* destRow = dest + y * mappedResource.RowPitch;
        const uint8_t* srcRow = src + y * textureWidth * 3;
        for (int x = 0; x < textureWidth; x++) {
            destRow[x * 4 + 0] = srcRow[x * 3 + 0]; // B
            destRow[x * 4 + 1] = srcRow[x * 3 + 1]; // G
            destRow[x * 4 + 2] = srcRow[x * 3 + 2]; // R
            destRow[x * 4 + 3] = 255;               // A
        }
    }
    
    d3dContext->Unmap(videoTexture, 0);

    // 设置渲染目标和视口
    d3dContext->OMSetRenderTargets(1, &renderTargetView, nullptr);
    
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    d3dContext->ClearRenderTargetView(renderTargetView, clearColor);
    
    // 获取后备缓冲区的尺寸
    ID3D11Texture2D* backBuffer;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    D3D11_TEXTURE2D_DESC backBufferDesc;
    backBuffer->GetDesc(&backBufferDesc);
    backBuffer->Release();

    // 计算保持宽高比的视口尺寸
    float aspectRatio = (float)textureWidth / textureHeight;
    float windowAspectRatio = (float)backBufferDesc.Width / backBufferDesc.Height;
    
    D3D11_VIEWPORT viewport = {};
    if (windowAspectRatio > aspectRatio) {
        // 窗口较宽，以高度为基准
        viewport.Height = (float)backBufferDesc.Height;
        viewport.Width = viewport.Height * aspectRatio;
        viewport.TopLeftX = (backBufferDesc.Width - viewport.Width) / 2;
        viewport.TopLeftY = 0;
    } else {
        // 窗口较高，以宽度为基准
        viewport.Width = (float)backBufferDesc.Width;
        viewport.Height = viewport.Width / aspectRatio;
        viewport.TopLeftX = 0;
        viewport.TopLeftY = (backBufferDesc.Height - viewport.Height) / 2;
    }
    
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    d3dContext->RSSetViewports(1, &viewport);
    
    // 设置渲染状态
    d3dContext->IASetInputLayout(inputLayout);
    d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    d3dContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    d3dContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    
    d3dContext->VSSetShader(vertexShader, nullptr, 0);
    d3dContext->PSSetShader(pixelShader, nullptr, 0);
    d3dContext->PSSetShaderResources(0, 1, &videoTextureView);
    d3dContext->PSSetSamplers(0, 1, &samplerState);
    
    // 绘制
    d3dContext->DrawIndexed(6, 0, 0);
}

void D3D11Renderer::Present(int syncInterval) {
    swapChain->Present(syncInterval, 0);
}

bool D3D11Renderer::CreateShaders() {
    // 顶点着色器代码
    const char* vsCode = R"(
        struct VS_Input {
            float3 pos : POSITION;
            float2 tex : TEXCOORD;
        };
        
        struct VS_Output {
            float4 pos : SV_POSITION;
            float2 tex : TEXCOORD;
        };
        
        VS_Output main(VS_Input input) {
            VS_Output output;
            output.pos = float4(input.pos, 1.0f);
            output.tex = input.tex;
            return output;
        }
    )";

    // 像素着色器代码
    const char* psCode = R"(
        Texture2D shaderTexture : register(t0);
        SamplerState samplerState : register(s0);
        
        float4 main(float4 pos : SV_POSITION, float2 tex : TEXCOORD) : SV_Target {
            return shaderTexture.Sample(samplerState, tex);
        }
    )";

    // 编译顶点着色器
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    if (FAILED(D3DCompile(vsCode, strlen(vsCode), nullptr, nullptr, nullptr, "main", 
        "vs_5_0", 0, 0, &vsBlob, &errorBlob))) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    // 创建顶点着色器
    if (FAILED(d3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, &vertexShader))) {
        vsBlob->Release();
        return false;
    }

    // 创建输入布局
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    if (FAILED(d3dDevice->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), &inputLayout))) {
        vsBlob->Release();
        return false;
    }
    vsBlob->Release();

    // 编译像素着色器
    ID3DBlob* psBlob = nullptr;
    if (FAILED(D3DCompile(psCode, strlen(psCode), nullptr, nullptr, nullptr, "main",
        "ps_5_0", 0, 0, &psBlob, &errorBlob))) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    // 创建像素着色器
    if (FAILED(d3dDevice->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, &pixelShader))) {
        psBlob->Release();
        return false;
    }
    psBlob->Release();

    return true;
}

bool D3D11Renderer::CreateBuffers() {
    // 创建顶点缓冲区
    Vertex vertices[] = {
        { DirectX::XMFLOAT3(-1.0f,  1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
        { DirectX::XMFLOAT3( 1.0f,  1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
        { DirectX::XMFLOAT3( 1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) },
        { DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) }
    };

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(vertices);
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    if (FAILED(d3dDevice->CreateBuffer(&vbd, &initData, &vertexBuffer))) {
        return false;
    }

    // 创建索引缓冲区
    UINT indices[] = { 0, 1, 2, 0, 2, 3 };

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(indices);
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    initData.pSysMem = indices;

    if (FAILED(d3dDevice->CreateBuffer(&ibd, &initData, &indexBuffer))) {
        return false;
    }

    return true;
}

bool D3D11Renderer::CreateTextures(int width, int height) {
    // 创建视频纹理
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DYNAMIC;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(d3dDevice->CreateTexture2D(&textureDesc, nullptr, &videoTexture))) {
        return false;
    }

    // 创建着色器资源视图
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    if (FAILED(d3dDevice->CreateShaderResourceView(videoTexture, &srvDesc, &videoTextureView))) {
        return false;
    }

    // 创建采样器状态
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    if (FAILED(d3dDevice->CreateSamplerState(&samplerDesc, &samplerState))) {
        return false;
    }

    return true;
}

void D3D11Renderer::Resize(int width, int height) {
    if (!d3dDevice || !swapChain) return;

    // 释放旧的资源
    if (renderTargetView) renderTargetView->Release();

    // 调整交换链大小
    swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

    // 重新创建渲染目标视图
    ID3D11Texture2D* backBuffer;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    d3dDevice->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView);
    backBuffer->Release();
}
