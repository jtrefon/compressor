cmake_minimum_required(VERSION 3.15)
project(Consumer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Two consumption modes (see docs/PORTING.md):
#   Mode A (default): FetchContent pinned to an exact engine tag.
#   Mode B:           find_package(CompressionLib) against an installed tree.
#                     Configure with -DCOMPRESSIONLIB_CONSUMER_MODE=installed
#                     and -DCMAKE_PREFIX_PATH=<prefix>.
set(COMPRESSIONLIB_CONSUMER_MODE "fetch" CACHE STRING
    "Dependency mode: 'fetch' (FetchContent tag pin) or 'installed' (find_package)")
set(COMPRESSIONLIB_TAG "v1.6.2" CACHE STRING
    "Engine tag to pin in fetch mode (exact tag, never a branch)")

if(COMPRESSIONLIB_CONSUMER_MODE STREQUAL "installed")
    find_package(CompressionLib REQUIRED)
    set(CONSUMER_LIB CompressionLib::compression)
else()
    include(FetchContent)
    FetchContent_Declare(
        compressionlib
        GIT_REPOSITORY https://github.com/jtrefon/compressionlib.git
        GIT_TAG        ${COMPRESSIONLIB_TAG}
    )
    FetchContent_MakeAvailable(compressionlib)
    set(CONSUMER_LIB CompressionLib::compression)
endif()

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE ${CONSUMER_LIB})