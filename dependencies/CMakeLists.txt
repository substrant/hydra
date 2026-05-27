cmake_minimum_required(VERSION 4.0)

# Build all libraries as static
set(BUILD_SHARED_LIBS OFF)

# Add phnt
add_subdirectory(phnt)

# Add Zydis
set(ZYDIS_BUILD_SHARED_LIB OFF)
set(ZYDIS_BUILD_EXAMPLES OFF)
set(ZYDIS_BUILD_TOOLS OFF)
set(ZYDIS_BUILD_TESTS OFF)
add_subdirectory(zydis)

# Fuck you Zydis devs. You can't feed MSVC flags to NASM
set_target_properties(Zycore PROPERTIES INTERFACE_COMPILE_OPTIONS "")
set_target_properties(Zydis PROPERTIES INTERFACE_COMPILE_OPTIONS "")
