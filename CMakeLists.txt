cmake_minimum_required(VERSION 4.0)
project(Substrant.Hydra VERSION 1.0.0 LANGUAGES CXX C)

option(HYDRA_BUILD_TESTS "Build Hydra tests" OFF)

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

# Add source files
file(GLOB_RECURSE HYDRA_SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")
file(GLOB_RECURSE HYDRA_HEADERS "include/*.hpp" "include/*.h")

# Add dependencies first
add_subdirectory(dependencies EXCLUDE_FROM_ALL)
find_package(OpenSSL REQUIRED)

# Create static library
add_library(Substrant.Hydra STATIC ${HYDRA_SOURCES} ${HYDRA_HEADERS})

set_target_properties(Substrant.Hydra PROPERTIES
    OUTPUT_NAME "libhydra"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
)

target_compile_definitions(Substrant.Hydra PRIVATE
    WIN32_LEAN_AND_MEAN              # Exclude rarely used Windows headers
    _MBCS                            # Use multi-byte character set
    ZYDIS_STATIC_BUILD               # Link Zydis statically
    $<$<CONFIG:Debug>:_DEBUG>        # Define _DEBUG macro in Debug builds
    $<$<CONFIG:Release>:NDEBUG>      # Define NDEBUG macro in Release builds
)

target_compile_options(Substrant.Hydra PRIVATE
    /W3                              # Warning level 3
    /permissive-                     # Disable non-conforming code extensions
    /clang:-Wno-pragma-pack          # Suppress pragma pack warnings
    /clang:-Wno-system-headers       # Suppress warnings in system headers
    /clang:-Wno-microsoft-enum-forward-reference # Enable MS extensions in Clang
    $<$<CXX_COMPILER_ID:MSVC>:/MP>   # Multi-processor compilation
    $<$<CONFIG:Debug>:/Od>           # Debug: Disable optimizations
    $<$<CONFIG:Release>:/O2>         # Release: Optimize for speed
)

target_include_directories(Substrant.Hydra
    PUBLIC include "C:\\Boost" # I fucking hate boost
    PUBLIC SYSTEM dependencies/phnt/include
)

target_link_libraries(Substrant.Hydra
    PRIVATE Zydis Zycore OpenSSL::SSL OpenSSL::Crypto
    PUBLIC phnt
)

if(HYDRA_BUILD_TESTS)
    # Add source files
    file(GLOB_RECURSE HYDRA_TESTS_SOURCES CONFIGURE_DEPENDS "tests/src/*.cpp" "src/*.c")
    file(GLOB_RECURSE HYDRA_TESTS_HEADERS "tests/include/*.hpp" "tests/include/*.h")

    # Create tests program
    add_executable(Substrant.HydraTests ${HYDRA_TESTS_SOURCES} ${HYDRA_TESTS_HEADERS})

    set_target_properties(Substrant.HydraTests PROPERTIES
        OUTPUT_NAME "libhydra"
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )

    target_compile_definitions(Substrant.HydraTests PRIVATE
        WIN32_LEAN_AND_MEAN              # Exclude rarely used Windows headers
        _MBCS                            # Use multi-byte character set
        $<$<CONFIG:Debug>:_DEBUG>        # Define _DEBUG macro in Debug builds
        $<$<CONFIG:Release>:NDEBUG>      # Define NDEBUG macro in Release builds
    )

    target_compile_options(Substrant.HydraTests PRIVATE
        /W3                              # Warning level 3
        /permissive-                     # Disable non-conforming code extensions
        /clang:-Wno-pragma-pack          # Suppress pragma pack warnings
        /clang:-Wno-system-headers       # Suppress warnings in system headers
        /clang:-Wno-microsoft-enum-forward-reference # Enable MS extensions in Clang
        $<$<CXX_COMPILER_ID:MSVC>:/MP>   # Multi-processor compilation
        $<$<CONFIG:Debug>:/Od>           # Debug: Disable optimizations
        $<$<CONFIG:Release>:/O2>         # Release: Optimize for speed
    )

    target_include_directories(Substrant.HydraTests
        PUBLIC tests/include
        PUBLIC SYSTEM dependencies/phnt/include
    )

    target_link_libraries(Substrant.HydraTests
        PRIVATE Substrant.Hydra
    )
endif()