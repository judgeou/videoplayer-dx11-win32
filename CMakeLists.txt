cmake_minimum_required(VERSION 3.20)
project(videoplayer-dx11-win32 VERSION 0.1.0)

# C++ 标准设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
if(MSVC)
    add_compile_options(/W4 /MP)
    # 启用多线程编译
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()

# 源文件
set(SOURCES
    src/main.cpp
    src/decoder/ffmpeg_decoder.cpp
    src/renderer/d3d11_renderer.cpp
    src/audio/wasapi_audio.cpp
    src/ui/player_ui.cpp
)

# ImGui 源文件
set(IMGUI_SOURCES
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_demo.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_dx11.cpp
    third_party/imgui/backends/imgui_impl_win32.cpp
)

# 更新源文件列表
set(SOURCES
    src/main.cpp
    src/decoder/ffmpeg_decoder.cpp
    src/renderer/d3d11_renderer.cpp
    src/audio/wasapi_audio.cpp
    src/ui/player_ui.cpp
    ${IMGUI_SOURCES}
)

# 添加 FFmpeg 库路径
link_directories($ENV{FFMPEG_LIB})

# 主程序
add_executable(${PROJECT_NAME} ${SOURCES})

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
    $ENV{FFMPEG_INCLUDE}
)

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE
    avcodec
    avformat
    avutil
    swscale
    swresample
    d3d11
    dxgi
)