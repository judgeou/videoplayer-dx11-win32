#include "decoder/ffmpeg_decoder.hpp"
#include <Windows.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}

FFmpegDecoder::~FFmpegDecoder() {
    Cleanup();
}

bool FFmpegDecoder::OpenFile(const std::wstring& filename) {
    // 转换文件名为 UTF-8
    char utf8_filename[260];
    WideCharToMultiByte(CP_UTF8, 0, filename.c_str(), -1, utf8_filename, sizeof(utf8_filename), NULL, NULL);
    
    if (avformat_open_input(&formatContext, utf8_filename, NULL, NULL) < 0) {
        return false;
    }
    
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        Cleanup();
        return false;
    }
    
    // 找到视频流
    videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }
    
    if (videoStreamIndex == -1) {
        Cleanup();
        return false;
    }
    
    // 获取解码器
    const AVCodec* codec = avcodec_find_decoder(formatContext->streams[videoStreamIndex]->codecpar->codec_id);
    codecContext = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecContext, formatContext->streams[videoStreamIndex]->codecpar);
    
    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        Cleanup();
        return false;
    }

    // 分配 packet 和 frame
    packet = av_packet_alloc();
    frame = av_frame_alloc();

    return true;
}

bool FFmpegDecoder::DecodeFirstFrame(uint8_t** frameData, int* width, int* height) {
    bool frameDecoded = false;
    
    while (av_read_frame(formatContext, packet) >= 0 && !frameDecoded) {
        if (packet->stream_index == videoStreamIndex) {
            avcodec_send_packet(codecContext, packet);
            if (avcodec_receive_frame(codecContext, frame) == 0) {
                // 转换为 BGR 格式
                SwsContext* swsContext = sws_getContext(
                    frame->width, frame->height, (AVPixelFormat)frame->format,
                    frame->width, frame->height, AV_PIX_FMT_BGR24,
                    SWS_BILINEAR, NULL, NULL, NULL);
                
                *width = frame->width;
                *height = frame->height;
                *frameData = (uint8_t*)malloc(frame->width * frame->height * 3);
                
                uint8_t* dest[4] = { *frameData, NULL, NULL, NULL };
                int destLinesize[4] = { frame->width * 3, 0, 0, 0 };
                
                sws_scale(swsContext, frame->data, frame->linesize, 0,
                         frame->height, dest, destLinesize);
                
                sws_freeContext(swsContext);
                frameDecoded = true;
            }
        }
        av_packet_unref(packet);
    }
    
    return frameDecoded;
}

void FFmpegDecoder::Cleanup() {
    if (frame) {
        av_frame_free(&frame);
        frame = nullptr;
    }
    if (packet) {
        av_packet_free(&packet);
        packet = nullptr;
    }
    if (codecContext) {
        avcodec_free_context(&codecContext);
        codecContext = nullptr;
    }
    if (formatContext) {
        avformat_close_input(&formatContext);
        formatContext = nullptr;
    }
    videoStreamIndex = -1;
}
