cmake_minimum_required(VERSION 3.20)
project(ExternaCS2 VERSION 5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================
# Dependencies (Auto-Fetch)
# ============================================
include(FetchContent)

# ImGui (UI Library)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6
)
FetchContent_MakeAvailable(imgui)

# OMath (Math Library)
FetchContent_Declare(
    omath
    GIT_REPOSITORY https://github.com/orange-cpp/omath.git
    GIT_TAG v4.5.1
)
FetchContent_MakeAvailable(omath)

# ============================================
# Libraries
# ============================================

# 1. UIAccess Library (Fullscreen)
add_library(uiaccess_lib STATIC
    ../../libraries/uiaccess/uiaccess.cpp
)
target_include_directories(uiaccess_lib PUBLIC ../../libraries/uiaccess)

# 2. ImGui Helper
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui_lib PUBLIC 
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# ============================================
# Main Executable
# ============================================
add_executable(ExternaCS2 WIN32
    src/main.cpp
    src/esp_menu.hpp
)

target_link_libraries(ExternaCS2 PRIVATE
    uiaccess_lib
    imgui_lib
    omath::omath
    d3d11
    dxgi
    dwmapi
    dcomp
)

# Optimization Flags
if(MSVC)
    target_compile_options(ExternaCS2 PRIVATE /O2 /Oi /Ot /GL /arch:AVX2)
    target_link_options(ExternaCS2 PRIVATE /LTCG)
endif()
