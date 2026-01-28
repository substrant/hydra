cmake_minimum_required(VERSION 4.0)
project(Substrant.HydraTests VERSION 1.0.0 LANGUAGES CXX C)

# Add source files
file(GLOB_RECURSE HYDRA_TESTS_SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")
file(GLOB_RECURSE HYDRA_TESTS_HEADERS "include/*.hpp" "include/*.h")

# Add Boost
set(Boost_DIR "D:\\libraries\\boost_1_90_0\\lib64-msvc-14.3\\cmake\\Boost-1.90.0")
set(Boost_LIBRARYDIR "D:\\libraries\\boost_1_90_0\\lib64-msvc-14.3")
set(Boost_USE_STATIC_LIBS ON) 
find_package(Boost 1.90.0 REQUIRED)

# Create tests program
add_executable(Substrant.HydraTests ${HYDRA_TESTS_SOURCES} ${HYDRA_TESTS_HEADERS})

set_target_properties(Substrant.HydraTests PROPERTIES
    OUTPUT_NAME "Substrant.HydraTests"
    CXX_STANDARD 23
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
    $<$<CXX_COMPILER_ID:MSVC>:/MP>   # Multi-processor compilation
    $<$<CONFIG:Debug>:/Od>           # Debug: Disable optimizations
    $<$<CONFIG:Release>:/O2>         # Release: Optimize for speed
)

target_include_directories(Substrant.HydraTests
    PUBLIC include
    PUBLIC SYSTEM dependencies/phnt/include ${Boost_INCLUDE_DIRS} 
)

target_link_libraries(Substrant.HydraTests
    PRIVATE Substrant.Hydra ${Boost_LIBRARIES}
)