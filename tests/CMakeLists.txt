include(FetchContent)

# Fetch GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG       release-1.12.1 # Use a specific tag for stability
)

# Make GoogleTest available
# Must be called before first add_test
FetchContent_MakeAvailable(googletest)

# Add the test executable
add_executable(compression_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    # ${CMAKE_CURRENT_SOURCE_DIR}/NullCompressorTest.cpp # Missing file
    # ${CMAKE_CURRENT_SOURCE_DIR}/RleCompressorTest.cpp # Missing file
    # ${CMAKE_CURRENT_SOURCE_DIR}/HuffmanCompressorTest.cpp # Missing file
    ${CMAKE_CURRENT_SOURCE_DIR}/Lz77CompressorTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DeflateCompressorTest.cpp
)

# Link the test executable against GoogleTest and the compression library
target_link_libraries(compression_tests PRIVATE GTest::gtest_main compression)

# Discover and add tests to CTest
include(GoogleTest)
gtest_discover_tests(compression_tests) 