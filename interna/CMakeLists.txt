cmake_minimum_required(VERSION 3.20)
project(interna LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================
# Fetch dependencies automatically
# ============================================
include(FetchContent)

# ImGui (same as externa)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6
)
FetchContent_MakeAvailable(imgui)

# Build as DLL
add_library(${PROJECT_NAME} SHARED
    src/dllmain.cpp
    # ImGui sources
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${imgui_SOURCE_DIR} 
    ${imgui_SOURCE_DIR}/backends
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11
    dxgi
    dwmapi
)

# Compiler flags
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W3
        /O2
        /GS-
        /GL
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        /LTCG
    )
endif()

# Output
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "===========================================")
message(STATUS "  INTERNA CS2 Internal Cheat (DLL)")
message(STATUS "===========================================")
message(STATUS "  C++ Standard: C++23")
message(STATUS "  ImGui: Auto-download v1.91.6")
message(STATUS "  MinHook: Auto-download")
message(STATUS "  Renderer: Present Hook (DX11)")
message(STATUS "===========================================")
