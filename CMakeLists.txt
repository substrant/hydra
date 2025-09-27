cmake_minimum_required(VERSION 3.20)

project(Substrant.Hydra VERSION 1.0.0 LANGUAGES CXX C)

# Validate platform
if(NOT WIN32)
    message(FATAL_ERROR "This project is developed for Windows platforms and cannot be built on non-Windows systems.")
endif()

# Validate compatibility
if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
    message(FATAL_ERROR
        "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}. "
        "This project requires MSVC or Clang-CL for Windows ABI compatibility."
    )
endif()

# Ensure Clang is MSVC-compatible if used
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    message(FATAL_ERROR
        "Clang must be used with MSVC-like command line (clang-cl) for ABI compatibility. "
        "Please use clang-cl instead of clang."
    )
endif()

# Build dependencies first
add_subdirectory(dependencies)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

file(GLOB_RECURSE HYDRA_SOURCES
    "src/*.cpp"
    "src/*.c"
)

file(GLOB_RECURSE HYDRA_HEADERS
    "include/*.hpp"
    "include/*.h"
)

# Create static library
add_library(libhydra STATIC ${HYDRA_SOURCES} ${HYDRA_HEADERS})

# Configure basic properties
set_target_properties(libhydra PROPERTIES
    OUTPUT_NAME "libhydra"
)

target_compile_definitions(libhydra PRIVATE
    WIN32_LEAN_AND_MEAN                # Exclude rarely used Windows headers
    _MBCS                              # Use multi-byte character set (not Unicode)
    ZYDIS_STATIC_BUILD                 # Link Zydis statically
    $<$<CONFIG:Debug>:_DEBUG>          # Define _DEBUG macro only in Debug builds
    $<$<CONFIG:Release>:NDEBUG>        # Define NDEBUG macro only in Release builds
)

target_compile_options(libhydra PRIVATE
    /W3                              # Warning level 3
    /permissive-                     # Disable non-conforming code extensions
    /clang:-Wno-pragma-pack          # Suppress pragma pack warnings because they are useless
    /clang:-Wno-system-headers       # Suppress warnings in system headers because we didn't fucking write them
    $<$<CXX_COMPILER_ID:MSVC>:/MP>   # Multi-processor compilation (MSVC only)

    # Configuration-specific optimizations
    $<$<CONFIG:Debug>:/Od>             # Debug: Disable optimizations
    $<$<CONFIG:Release>:/O2>           # Release: Optimize for speed
)

set_target_properties(libhydra PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

target_include_directories(libhydra PUBLIC SYSTEM
    dependencies/phnt/include
)

target_include_directories(libhydra PUBLIC
    include
)

target_include_directories(libhydra PRIVATE
    include/hydra
)

target_link_libraries(libhydra PRIVATE
    Zydis Zycore
)
