cmake_minimum_required(VERSION 4.0)
project(Substrant.Hydra VERSION 1.0.0 LANGUAGES CXX C)

option(HYDRA_BUILD_TESTS "Build Hydra tests" ON)

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

# Add source files
file(GLOB_RECURSE HYDRA_SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")
file(GLOB_RECURSE HYDRA_HEADERS "include/*.hpp" "include/*.h")

# Variable for all source files
set(HYDRA_SOURCE_FILES ${HYDRA_SOURCES} ${HYDRA_HEADERS})

# Add dependencies first
add_subdirectory(dependencies EXCLUDE_FROM_ALL)

# Add Boost
set(Boost_DIR "D:\\libraries\\boost_1_90_0\\lib64-msvc-14.3\\cmake\\Boost-1.90.0")
set(Boost_LIBRARYDIR "D:\\libraries\\boost_1_90_0\\lib64-msvc-14.3")
set(Boost_USE_STATIC_LIBS ON) 
find_package(Boost 1.90.0 REQUIRED)

# Create static library
add_library(Substrant.Hydra STATIC ${HYDRA_SOURCES} ${HYDRA_HEADERS})

set_target_properties(Substrant.Hydra PROPERTIES
    OUTPUT_NAME "libhydra"
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_compile_definitions(Substrant.Hydra PRIVATE
    WIN32_LEAN_AND_MEAN              # Exclude rarely used Windows headers
    _MBCS                            # Use multi-byte character set
    ZYDIS_STATIC_BUILD               # Link Zydis statically
    HYDRA_IS_LIBRARY                 # Indicate this is a library build
    $<$<CONFIG:Debug>:_DEBUG>        # Define _DEBUG macro in Debug builds
    $<$<CONFIG:Release>:NDEBUG>      # Define NDEBUG macro in Release builds
)

target_compile_options(Substrant.Hydra PRIVATE
    /W3                              # Warning level 3
    /permissive-                     # Disable non-conforming code extensions
    $<$<CXX_COMPILER_ID:MSVC>:/MP>   # Multi-processor compilation
    $<$<CONFIG:Debug>:/Od>           # Debug: Disable optimizations
    $<$<CONFIG:Release>:/O2>         # Release: Optimize for speed
)

target_include_directories(Substrant.Hydra
    PUBLIC include
    PUBLIC SYSTEM dependencies/phnt/include ${Boost_INCLUDE_DIRS}
)

target_link_libraries(Substrant.Hydra
    PRIVATE Zydis Zycore ${Boost_LIBRARIES} #OpenSSL::SSL OpenSSL::Crypto
    PUBLIC phnt
)

if(HYDRA_BUILD_TESTS)
    add_subdirectory(tests)
endif()
