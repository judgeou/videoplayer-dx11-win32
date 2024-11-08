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

// 添加全局变量用于存储视频帧
struct VideoState {
    uint8_t* frameBuffer;
    int width;
    int height;
    // 添加 D3D11 相关成员
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
} videoState;

// 顶点结构体定义
struct Vertex {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 tex;
};

bool CreateShaders(ID3D11Device* device);
bool CreateBuffers(ID3D11Device* device);
bool InitD3D11(HWND hwnd);

// 解码第一帧的函数
bool DecodeFirstFrame(const wchar_t* filename) {
    char utf8_filename[260];
    WideCharToMultiByte(CP_UTF8, 0, filename, -1, utf8_filename, sizeof(utf8_filename), NULL, NULL);
    
    AVFormatContext* formatContext = nullptr;
    if (avformat_open_input(&formatContext, utf8_filename, NULL, NULL) < 0) {
        return false;
    }
    
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        avformat_close_input(&formatContext);
        return false;
    }
    
    // 找到视频流
    int videoStream = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            break;
        }
    }
    
    if (videoStream == -1) {
        avformat_close_input(&formatContext);
        return false;
    }
    
    // 获取解码器
    const AVCodec* codec = avcodec_find_decoder(formatContext->streams[videoStream]->codecpar->codec_id);
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecContext, formatContext->streams[videoStream]->codecpar);
    
    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        return false;
    }
    
    // 读取第一帧
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    
    bool frameDecoded = false;
    while (av_read_frame(formatContext, packet) >= 0 && !frameDecoded) {
        if (packet->stream_index == videoStream) {
            avcodec_send_packet(codecContext, packet);
            if (avcodec_receive_frame(codecContext, frame) == 0) {
                // 转换为 BGR 格式而不是 RGB
                SwsContext* swsContext = sws_getContext(
                    frame->width, frame->height, (AVPixelFormat)frame->format,
                    frame->width, frame->height, AV_PIX_FMT_BGR24,
                    SWS_BILINEAR, NULL, NULL, NULL);
                
                videoState.width = frame->width;
                videoState.height = frame->height;
                videoState.frameBuffer = (uint8_t*)malloc(frame->width * frame->height * 3);
                
                uint8_t* dest[4] = { videoState.frameBuffer, NULL, NULL, NULL };
                int destLinesize[4] = { frame->width * 3, 0, 0, 0 };
                
                sws_scale(swsContext, frame->data, frame->linesize, 0,
                         frame->height, dest, destLinesize);
                
                sws_freeContext(swsContext);
                frameDecoded = true;
            }
        }
        av_packet_unref(packet);
    }
    
    // 清理资源
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);
    
    return frameDecoded;
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
            if (videoState.frameBuffer && videoState.d3dContext && videoState.videoTexture) {
                // 更新纹理数据
                D3D11_MAPPED_SUBRESOURCE mappedResource;
                videoState.d3dContext->Map(videoState.videoTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
                
                // 复制帧数据到纹理，BGR -> BGRA 转换
                uint8_t* dest = (uint8_t*)mappedResource.pData;
                uint8_t* src = videoState.frameBuffer;
                for (int y = 0; y < videoState.height; y++) {
                    uint8_t* destRow = dest + y * mappedResource.RowPitch;
                    uint8_t* srcRow = src + y * videoState.width * 3;
                    for (int x = 0; x < videoState.width; x++) {
                        destRow[x * 4 + 0] = srcRow[x * 3 + 0]; // B
                        destRow[x * 4 + 1] = srcRow[x * 3 + 1]; // G
                        destRow[x * 4 + 2] = srcRow[x * 3 + 2]; // R
                        destRow[x * 4 + 3] = 255;               // A
                    }
                }
                
                videoState.d3dContext->Unmap(videoState.videoTexture, 0);

                // 设置渲染目标和视口
                videoState.d3dContext->OMSetRenderTargets(1, &videoState.renderTargetView, nullptr);
                
                float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
                videoState.d3dContext->ClearRenderTargetView(videoState.renderTargetView, clearColor);
                
                // 设置视口
                D3D11_VIEWPORT viewport = {};
                viewport.Width = (float)videoState.width;
                viewport.Height = (float)videoState.height;
                viewport.MinDepth = 0.0f;
                viewport.MaxDepth = 1.0f;
                videoState.d3dContext->RSSetViewports(1, &viewport);
                
                // 设置着色器和输入布局
                videoState.d3dContext->IASetInputLayout(videoState.inputLayout);
                videoState.d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                
                // 设置顶点和索引缓冲区
                UINT stride = sizeof(Vertex);
                UINT offset = 0;
                videoState.d3dContext->IASetVertexBuffers(0, 1, &videoState.vertexBuffer, &stride, &offset);
                videoState.d3dContext->IASetIndexBuffer(videoState.indexBuffer, DXGI_FORMAT_R32_UINT, 0);
                
                // 设置着色器和资源
                videoState.d3dContext->VSSetShader(videoState.vertexShader, nullptr, 0);
                videoState.d3dContext->PSSetShader(videoState.pixelShader, nullptr, 0);
                videoState.d3dContext->PSSetShaderResources(0, 1, &videoState.videoTextureView);
                videoState.d3dContext->PSSetSamplers(0, 1, &videoState.samplerState);
                
                // 绘制
                videoState.d3dContext->DrawIndexed(6, 0, 0);
                
                // 呈现
                videoState.swapChain->Present(1, 0);
            }
            return 0;
        }
        
        case WM_SIZE: {
            // 窗口大小改变时强制重绘
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        
        case WM_DESTROY: {
            if (videoState.frameBuffer) {
                free(videoState.frameBuffer);
                videoState.frameBuffer = nullptr;
            }
            
            // 释放 D3D11 资源
            if (videoState.samplerState) videoState.samplerState->Release();
            if (videoState.videoTextureView) videoState.videoTextureView->Release();
            if (videoState.videoTexture) videoState.videoTexture->Release();
            if (videoState.renderTargetView) videoState.renderTargetView->Release();
            if (videoState.swapChain) videoState.swapChain->Release();
            if (videoState.d3dContext) videoState.d3dContext->Release();
            if (videoState.d3dDevice) videoState.d3dDevice->Release();
            
            if (videoState.vertexShader) videoState.vertexShader->Release();
            if (videoState.pixelShader) videoState.pixelShader->Release();
            if (videoState.inputLayout) videoState.inputLayout->Release();
            if (videoState.vertexBuffer) videoState.vertexBuffer->Release();
            if (videoState.indexBuffer) videoState.indexBuffer->Release();
            
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

bool InitD3D11(HWND hwnd) {
    HRESULT hr;
    
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
        &scd, &videoState.swapChain, &videoState.d3dDevice,
        nullptr, &videoState.d3dContext))) {
        return false;
    }

#ifdef _DEBUG
    // 获取并配置 Debug 层
    ID3D11Debug* debugLayer = nullptr;
    if (SUCCEEDED(videoState.d3dDevice->QueryInterface(__uuidof(ID3D11Debug), (void**)&debugLayer))) {
        ID3D11InfoQueue* infoQueue = nullptr;
        if (SUCCEEDED(debugLayer->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&infoQueue))) {
            infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
            infoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
            infoQueue->Release();
        }
        debugLayer->Release();
    }
#endif

    // 创建渲染目标视图
    ID3D11Texture2D* backBuffer;
    videoState.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    videoState.d3dDevice->CreateRenderTargetView(backBuffer, nullptr, &videoState.renderTargetView);
    backBuffer->Release();

    // 创建视频纹理
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = videoState.width;
    texDesc.Height = videoState.height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DYNAMIC;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(videoState.d3dDevice->CreateTexture2D(&texDesc, nullptr, &videoState.videoTexture))) {
        return false;
    }

    // 创建着色器资源视图
    hr = videoState.d3dDevice->CreateShaderResourceView(
        videoState.videoTexture, nullptr, &videoState.videoTextureView);
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create shader resource view\n");
        return false;
    }

    // 创建采样器状态
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    if (FAILED(videoState.d3dDevice->CreateSamplerState(&sampDesc, &videoState.samplerState))) {
        return false;
    }

    if (!CreateShaders(videoState.d3dDevice)) {
        return false;
    }

    if (!CreateBuffers(videoState.d3dDevice)) {
        return false;
    }

    return true;
}

bool CreateShaders(ID3D11Device* device) {
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    // 修改着色器代码字符串
    const char* shaderCode = R"(
        struct VS_INPUT {
            float3 pos : POSITION;
            float2 tex : TEXCOORD;
        };

        struct PS_INPUT {
            float4 pos : SV_POSITION;
            float2 tex : TEXCOORD;
        };

        Texture2D videoTexture : register(t0);
        SamplerState videoSampler : register(s0);

        PS_INPUT VS(VS_INPUT input) {
            PS_INPUT output;
            output.pos = float4(input.pos, 1.0f);
            output.tex = input.tex;
            return output;
        }

        float4 PS(PS_INPUT input) : SV_Target {
            return videoTexture.Sample(videoSampler, input.tex);
        }
    )";

    // 编译顶点着色器
    HRESULT hr = D3DCompile(
        shaderCode, strlen(shaderCode),
        nullptr, nullptr, nullptr, "VS", "vs_5_0",
        D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0, &vsBlob, &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }

    // 编译像素着色器（现在使用相同的代码字符串）
    hr = D3DCompile(
        shaderCode, strlen(shaderCode),
        nullptr, nullptr, nullptr, "PS", "ps_5_0",
        D3DCOMPILE_OPTIMIZATION_LEVEL3,
        0, &psBlob, &errorBlob);

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        if (vsBlob) vsBlob->Release();
        return false;
    }

    // 创建着色器
    hr = device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        &videoState.vertexShader);

    if (FAILED(hr)) {
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    hr = device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        &videoState.pixelShader);

    if (FAILED(hr)) {
        vsBlob->Release();
        psBlob->Release();
        return false;
    }

    // 创建输入布局
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = device->CreateInputLayout(
        layout,
        2,
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &videoState.inputLayout);

    vsBlob->Release();
    psBlob->Release();

    return SUCCEEDED(hr);
}

bool CreateBuffers(ID3D11Device* device) {
    // 创建顶点缓冲区
    Vertex vertices[] = {
        { DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
        { DirectX::XMFLOAT3(-1.0f,  1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
        { DirectX::XMFLOAT3( 1.0f,  1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
        { DirectX::XMFLOAT3( 1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) }
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;

    if (FAILED(device->CreateBuffer(&vbDesc, &vbData, &videoState.vertexBuffer))) {
        return false;
    }

    // 创建索引缓冲区
    UINT indices[] = { 0, 1, 2, 0, 2, 3 };

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices;

    if (FAILED(device->CreateBuffer(&ibDesc, &ibData, &videoState.indexBuffer))) {
        return false;
    }

    return true;
}