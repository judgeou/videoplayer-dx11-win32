#include <imgui.h>
#include <Windows.h>
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
} videoState;

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
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            if (videoState.frameBuffer) {
                // 获取窗口客户区大小
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                int windowWidth = clientRect.right - clientRect.left;
                int windowHeight = clientRect.bottom - clientRect.top;
                
                // 计算保持宽高比的缩放尺寸
                float videoAspectRatio = (float)videoState.width / videoState.height;
                float windowAspectRatio = (float)windowWidth / windowHeight;
                
                int destWidth, destHeight;
                int destX = 0, destY = 0;
                
                if (windowAspectRatio > videoAspectRatio) {
                    // 窗口较宽，以高度为基准
                    destHeight = windowHeight;
                    destWidth = (int)(windowHeight * videoAspectRatio);
                    destX = (windowWidth - destWidth) / 2; // 水平居中
                } else {
                    // 窗口较高，以宽度为基准
                    destWidth = windowWidth;
                    destHeight = (int)(windowWidth / videoAspectRatio);
                    destY = (windowHeight - destHeight) / 2; // 垂直居中
                }
                
                // 创建位图
                BITMAPINFO bmi = {0};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = videoState.width;
                bmi.bmiHeader.biHeight = -videoState.height;  // 负值表示自上而下
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 24;
                bmi.bmiHeader.biCompression = BI_RGB;
                
                // 填充黑色背景
                HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
                FillRect(hdc, &clientRect, blackBrush);
                DeleteObject(blackBrush);
                
                // 绘制帧
                SetStretchBltMode(hdc, HALFTONE); // 设置更好的缩放质量
                StretchDIBits(hdc,
                    destX, destY, destWidth, destHeight,
                    0, 0, videoState.width, videoState.height,
                    videoState.frameBuffer, &bmi, DIB_RGB_COLORS, SRCCOPY);
            }
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_SIZE: {
            // 窗口大小改变时强制重绘
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        
        case WM_DESTROY:
            if (videoState.frameBuffer) {
                free(videoState.frameBuffer);
                videoState.frameBuffer = nullptr;
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}