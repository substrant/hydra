cmake_minimum_required(VERSION 4.0)
project(Substrant.HydraTests VERSION 1.0.0 LANGUAGES CXX)

# Add source files
file(GLOB_RECURSE HYDRA_SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")
file(GLOB_RECURSE HYDRA_HEADERS "include/*.hpp" "include/*.h")

# Create static library
add_executable(Substrant.HydraTests ${HYDRA_SOURCES} ${HYDRA_HEADERS})

set_target_properties(Substrant.HydraTests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_compile_definitions(Substrant.HydraTests PRIVATE
    _MBCS                            # Use multi-byte character set
    $<$<CONFIG:Debug>:_DEBUG>        # Define _DEBUG macro in Debug builds
    $<$<CONFIG:Release>:NDEBUG>      # Define NDEBUG macro in Release builds
)

target_include_directories(Substrant.HydraTests
    PUBLIC include
    PUBLIC SYSTEM dependencies/phnt/include
)

target_link_libraries(Substrant.HydraTests
    PUBLIC Substrant.Hydra
)
