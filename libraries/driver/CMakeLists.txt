# BYOVD Driver Mapper
# Stealthy kernel driver loading via multiple vulnerable drivers
# Supports: Intel iqvw64e, MSI RTCore64, WinRing0, CPUZ

cmake_minimum_required(VERSION 3.20)
project(DriverMapper VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Only build on Windows
if(NOT WIN32)
    message(FATAL_ERROR "Driver mapper only supports Windows")
endif()

# Source files
set(MAPPER_SOURCES
    driver_mapper.cpp
    driver_mapper.hpp
    shellcode.cpp
    shellcode.hpp
    workitem_executor.cpp
    workitem_executor.hpp
    vulnerable_drivers.cpp
    vulnerable_drivers.hpp
    logger.hpp
)

# Static library
add_library(driver_mapper STATIC ${MAPPER_SOURCES})

target_include_directories(driver_mapper PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link with required libraries
target_link_libraries(driver_mapper PRIVATE
    ntdll
    advapi32
)

# Compiler options
if(MSVC)
    target_compile_options(driver_mapper PRIVATE
        /W4                     # Warning level 4
        /O2                     # Optimize for speed
        /GL                     # Whole program optimization
        /Gy                     # Enable function-level linking
        /Gw                     # Enable whole-program global data optimization
        /wd4100                 # Unreferenced formal parameter
        /wd4127                 # Conditional expression is constant
        /wd4201                 # Nonstandard extension: nameless struct/union
    )
    
    # Link-time optimization
    target_link_options(driver_mapper PRIVATE
        /LTCG                   # Link-time code generation
    )
endif()

# Preprocessor definitions
target_compile_definitions(driver_mapper PRIVATE
    _WIN32_WINNT=0x0A00         # Windows 10
    NOMINMAX                    # No min/max macros
    WIN32_LEAN_AND_MEAN         # Reduce Windows headers
)

# Example usage executable (optional)
option(BUILD_MAPPER_EXAMPLE "Build mapper example" OFF)

if(BUILD_MAPPER_EXAMPLE)
    add_executable(mapper_example
        example.cpp
    )
    
    target_link_libraries(mapper_example PRIVATE
        driver_mapper
    )
endif()
