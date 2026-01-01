# KERNEL BYPASS FRAMEWORK - Ring 0 Driver
# CMake build configuration
#
# NOTE: This is a skeleton CMakeLists.
# To actually build, you need Visual Studio + WDK installed.
# The proper way is to use WDK's build system or a VS project.

cmake_minimum_required(VERSION 3.20)
project(KernelBypass_Ring0 LANGUAGES C)

# NOTE: Kernel drivers require special compilation
# This CMakeLists is for reference only
# Use Visual Studio with WDK or WDK's MSBuild integration

message(WARNING "
================================================================================
  KERNEL DRIVER BUILD NOTICE
================================================================================

  Kernel drivers cannot be built with standard CMake.
  You need:
  
  1. Visual Studio 2022
  2. Windows Driver Kit (WDK) 10 or 11
  3. Windows SDK
  
  Build options:
  
  A) Use Visual Studio:
     - Open VS -> Create new Kernel Mode Driver project
     - Add source files from driver/ folder
     
  B) Use WDK command line:
     msbuild KernelBypass.vcxproj /p:Configuration=Release /p:Platform=x64
     
  C) Use inf2cat and signtool for signing
  
================================================================================
")

# List source files (for reference)
set(DRIVER_SOURCES
    driver/main.c
    driver/memory.c
    driver/memory.h
    driver/process.c
    driver/process.h
    driver/callbacks.c
    driver/callbacks.h
    driver/hwid.c
    driver/hwid.h
)

# List common headers
set(COMMON_HEADERS
    ../common/types.h
    ../common/ioctl_codes.h
)

# Print files for reference
message(STATUS "Driver sources: ${DRIVER_SOURCES}")
message(STATUS "Common headers: ${COMMON_HEADERS}")

