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

include(GoogleTest)

# Add the test executable
add_executable(compression_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RleCompressorTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/HuffmanCompressorTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Lz77CompressorTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BwtCompressorTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/OptimizedCompressorTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ArithmeticCompressorTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/UltraCompressorTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ExtremeCompressorTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BugRegressionTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/ByteViewTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/ErrorsTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/BinaryIOTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/ExecutorTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/ByteSourceTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/codec/CodecRegistryTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/codec/ParallelCodecDecoratorTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/codec/pipeline/PipelineTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/codec/pipeline/AnsCoderTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/codec/decorator/DecoratorTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/format/FrameTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/format/GoldenTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/security/FuzzGatesTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/EventBusTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/CompressionServiceTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app/CommandTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/archive/ArchiveTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/archive/ArchiveServiceTest.cpp
    )
# Link the test executable against GoogleTest and the compression library
target_link_libraries(compression_tests PRIVATE GTest::gtest_main compression)


# Discover and add tests to CTestgtest_discover_tests(compression_tests)

gtest_discover_tests(compression_tests)
