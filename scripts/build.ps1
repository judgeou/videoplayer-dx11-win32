# PowerShell 构建脚本
param(
    [string]$BuildType = "Release",
    [string]$Generator = "Visual Studio 17 2022"
)

# 创建构建目录
$BuildDir = "build"
if (!(Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Force -Path $BuildDir
}

# CMake 配置
cmake -B $BuildDir -G $Generator `
    -DCMAKE_BUILD_TYPE=$BuildType `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

# 构建
cmake --build $BuildDir --config $BuildType