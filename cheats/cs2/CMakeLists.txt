cmake_minimum_required(VERSION 3.20)
project(ExternaCS2 VERSION 2.0 LANGUAGES CXX ASM_MASM)

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

# 1. Driver Library (Ring 0)
add_library(driver_lib STATIC
    ../../libraries/driver/driver_mapper.cpp
    ../../libraries/driver/shellcode.cpp
    ../../libraries/driver/workitem_executor.cpp
    ../../libraries/driver/vulnerable_drivers.cpp
)
target_include_directories(driver_lib PUBLIC ../../libraries/driver)
target_compile_definitions(driver_lib PRIVATE WIN32_LEAN_AND_MEAN)

# 2. UIAccess Library (Fullscreen)
add_library(uiaccess_lib STATIC
    ../../libraries/uiaccess/uiaccess.cpp
)
target_include_directories(uiaccess_lib PUBLIC ../../libraries/uiaccess)

# 3. ImGui Helper
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
# ASM for Syscalls
add_library(syscall_asm OBJECT src/syscall.asm)
set_target_properties(syscall_asm PROPERTIES LINKER_LANGUAGE CXX)

add_executable(ExternaCS2 WIN32
    src/main.cpp
    src/esp_menu.hpp
    $<TARGET_OBJECTS:syscall_asm>
)

target_link_libraries(ExternaCS2 PRIVATE
    driver_lib
    uiaccess_lib
    imgui_lib
    omath::omath
    d3d11
    dxgi
    dwmapi
)

# Optimization Flags
if(MSVC)
    target_compile_options(ExternaCS2 PRIVATE /O2 /Oi /Ot /GL /arch:AVX2)
    target_link_options(ExternaCS2 PRIVATE /LTCG)
endif()
