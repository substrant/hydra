cmake_minimum_required(VERSION 3.20)

project(Substrant.Hydra VERSION 1.0.0 LANGUAGES CXX C)

# Validate compiler compatibility on Windows
if(WIN32)
    if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
        message(FATAL_ERROR
            "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}. "
            "This project requires MSVC or Clang-CL for Windows ABI compatibility."
        )
    endif()

    # Ensure Clang is using MSVC-like command line (clang-cl)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        message(FATAL_ERROR
            "Clang must be used with MSVC-like command line (clang-cl) for ABI compatibility. "
            "Please use clang-cl instead of clang."
        )
    endif()
endif()

# Build dependencies first - this sets up Zydis, Zycore, and other deps
add_subdirectory(dependencies)

# Standard settings - C++23 for modern features, C17 for any C code
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Recursively find all source files - modern CMake approach
file(GLOB_RECURSE HYDRA_SOURCES
    "src/*.cpp"
    "src/*.c"
)

# Recursively find all header files for IDE organization
file(GLOB_RECURSE HYDRA_HEADERS
    "include/*.hpp"
    "include/*.h"
)

# Create static library
add_library(libhydra STATIC ${HYDRA_SOURCES} ${HYDRA_HEADERS})

# Configure basic properties
set_target_properties(libhydra PROPERTIES
    OUTPUT_NAME "libhydra"
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Windows-specific compiler settings
if(WIN32)
    # Preprocessor definitions for all configurations
    target_compile_definitions(libhydra PRIVATE
        WIN32_LEAN_AND_MEAN                # Exclude rarely used Windows headers
        NOMINMAX                           # Don't define min/max macros that conflict with std::
        _MBCS                              # Use multi-byte character set (not Unicode)
        ZYDIS_STATIC_BUILD                 # Link Zydis statically
        $<$<CONFIG:Debug>:_DEBUG>          # Define _DEBUG macro only in Debug builds
        $<$<CONFIG:Release>:NDEBUG>        # Define NDEBUG macro only in Release builds
    )

    # Compiler options
    target_compile_options(libhydra PRIVATE
        /W3                              # Warning level 3
        /permissive-                     # Disable non-conforming code extensions
        $<$<CXX_COMPILER_ID:MSVC>:/MP>     # Multi-processor compilation (MSVC only)

        # Configuration-specific optimizations
        $<$<CONFIG:Debug>:/Od>             # Debug: Disable optimizations
        $<$<CONFIG:Release>:/O2>           # Release: Optimize for speed
    )

    # Runtime library selection
    set_target_properties(libhydra PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()

# Public headers that consumers need
target_include_directories(libhydra PUBLIC
    include
)

# Private headers (implementation details and dependencies)
target_include_directories(libhydra PRIVATE
    include/hydra
    dependencies/phnt/include
)

# Link with Zydis (built by dependencies/CMakeLists.txt)
target_link_libraries(libhydra PRIVATE
    Zydis
    Zycore
)
