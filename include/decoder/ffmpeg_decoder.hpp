#pragma once
#include <cstdint>
#include <string>
#include <memory>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;

class FFmpegDecoder {
public:
    FFmpegDecoder() = default;
    ~FFmpegDecoder();

    bool OpenFile(const std::wstring& filename);
    bool DecodeFirstFrame(uint8_t** frameData, int* width, int* height);
    void Cleanup();

private:
    AVFormatContext* formatContext = nullptr;
    AVCodecContext* codecContext = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* packet = nullptr;
    int videoStreamIndex = -1;
};
