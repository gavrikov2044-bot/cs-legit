project(Launcher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Setup ImGui
include(FetchContent)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui
  GIT_TAG        v1.90
)
FetchContent_MakeAvailable(imgui)

set(SOURCES
    src/main.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

# GUI application (uses WinMain)
add_executable(launcher WIN32 ${SOURCES})

target_include_directories(launcher PRIVATE 
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    src
)

# Static link runtime
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Version from CMake/CI
# Если переменная LAUNCHER_VERSION не передана в CMake,
# используем дефолтное значение для локальной сборки.
if(NOT DEFINED LAUNCHER_VERSION)
  set(LAUNCHER_VERSION "1.0.0-local")
endif()

message(STATUS "Launcher version: ${LAUNCHER_VERSION}")

target_compile_definitions(launcher PRIVATE LAUNCHER_VERSION=\"${LAUNCHER_VERSION}\")

target_link_libraries(launcher PRIVATE 
    d3d11
    d3dcompiler
    dwmapi
    wininet
    kernel32 
    user32 
    advapi32
    shell32
)

# Require admin rights for auto-update (UAC elevation)
if(MSVC)
    set_target_properties(launcher PROPERTIES
        LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\""
    )
endif()
