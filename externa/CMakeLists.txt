cmake_minimum_required(VERSION 3.20)
project(Externa VERSION 1.0 LANGUAGES CXX ASM_MASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Disable default warning level to avoid /W3 vs /W4 conflicts
if(MSVC)
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE "/W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
endif()

# Clear ALL ASM_MASM flags to prevent C++ flags leaking
set(CMAKE_ASM_MASM_FLAGS "" CACHE STRING "" FORCE)
set(CMAKE_ASM_MASM_FLAGS_DEBUG "" CACHE STRING "" FORCE)
set(CMAKE_ASM_MASM_FLAGS_RELEASE "" CACHE STRING "" FORCE)
set(CMAKE_ASM_MASM_FLAGS_RELWITHDEBINFO "" CACHE STRING "" FORCE)
set(CMAKE_ASM_MASM_FLAGS_MINSIZEREL "" CACHE STRING "" FORCE)

# ============================================
# Fetch dependencies automatically
# ============================================
include(FetchContent)

# ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6
)
FetchContent_MakeAvailable(imgui)

# OMath - professional math library
FetchContent_Declare(
    omath
    GIT_REPOSITORY https://github.com/orange-cpp/omath.git
    GIT_TAG v4.5.1
)
FetchContent_MakeAvailable(omath)

# Suppress omath warnings by setting its warning level
if(TARGET omath)
    if(MSVC)
        target_compile_options(omath PRIVATE /W0)
    endif()
endif()

# ImGui library
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# ============================================
# Main executable (with direct syscall support)
# ============================================
add_executable(${PROJECT_NAME} WIN32
    src/main.cpp
    src/syscall.asm
)

# Explicitly set empty compile options for ASM file
set_source_files_properties(src/syscall.asm PROPERTIES
    COMPILE_OPTIONS ""
    COMPILE_FLAGS ""
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    imgui
    omath::omath
    d3d11
    dxgi
    dwmapi
)

# MSVC optimizations (only for C++, not ASM)
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/W4 /O2 /GL /GS- /fp:fast /arch:AVX2>
        $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:/DNDEBUG>
    )
    
    target_link_options(${PROJECT_NAME} PRIVATE
        /LTCG /OPT:REF /OPT:ICF
    )
endif()

message(STATUS "===========================================")
message(STATUS "  EXTERNAL ESP v1.0.0")
message(STATUS "===========================================")
message(STATUS "  C++ Standard: C++23")
message(STATUS "  ImGui: v1.91.6")
message(STATUS "  OMath: v4.5.1")
message(STATUS "  Renderer: DirectX 11")
message(STATUS "===========================================")
