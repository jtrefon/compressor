add_library(compression)

# Namespaced alias so consumers can use the same target name in
# FetchContent/add_subdirectory mode as in the installed package.
add_library(CompressionLib::compression ALIAS compression)

target_include_directories(compression 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:${INSTALL_INCLUDE_DIR}> # Use install path
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} # Needed for internal headers if any
)

target_sources(compression PRIVATE
    codec/legacy/NullCompressor.cpp
    codec/legacy/RleCompressor.cpp
    codec/legacy/HuffmanCompressor.cpp
    codec/legacy/Lz77Compressor.cpp
    codec/legacy/BwtCompressor.cpp
    ThreadPool.cpp
    codec/legacy/ArithmeticCompressor.cpp
    codec/legacy/OptimizedCompressor.cpp
    codec/legacy/UltraCompressor.cpp
    codec/legacy/ExtremeCompressor.cpp
    core/ByteSource.cpp
    codec/CodecRegistry.cpp
    codec/ParallelCodecDecorator.cpp
    codec/pipeline/Pipeline.cpp
    codec/pipeline/AnsCoder.cpp
    codec/decorator/Decorators.cpp
    format/FrameRegistry.cpp
    format/FileFormat.cpp
    events/EventBus.cpp
    app/CompressionService.cpp
    app/Command.cpp
    app/ArchiveService.cpp
    archive/ArchiveWriter.cpp
    archive/ArchiveReader.cpp
)

# --- Platform adapters: exactly one TU per OS, zero ifdefs in domain code ---
if(WIN32)
    target_sources(compression PRIVATE platform/SystemInfo_windows.cpp)
elseif(APPLE)
    target_sources(compression PRIVATE platform/SystemInfo_darwin.cpp)
else()
    target_sources(compression PRIVATE platform/SystemInfo_linux.cpp)
endif()
# --- Installation --- 

# Install the library target itself
install(TARGETS compression 
    EXPORT CompressionLibTargets # Export the target for CMake config
    LIBRARY DESTINATION ${INSTALL_LIB_DIR}
    ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
    RUNTIME DESTINATION ${INSTALL_BIN_DIR} # For DLLs on Windows
    INCLUDES DESTINATION ${INSTALL_INCLUDE_DIR} # For header-only parts if any
)

# --- CMake Package Config Generation --- 

include(CMakePackageConfigHelpers)

# Generate the version file (CompressionLibConfigVersion.cmake)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/CompressionLibConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Configure the main config file (CompressionLibConfig.cmake.in -> CompressionLibConfig.cmake)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompressionLibConfig.cmake.in 
    ${CMAKE_CURRENT_BINARY_DIR}/CompressionLibConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CMAKE_DIR}
    PATH_VARS INSTALL_INCLUDE_DIR INSTALL_LIB_DIR
)

# Install the generated config files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/CompressionLibConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/CompressionLibConfigVersion.cmake
    DESTINATION ${INSTALL_CMAKE_DIR}
)

# Install the export set (CompressionLibTargets.cmake)
install(EXPORT CompressionLibTargets
    FILE CompressionLibTargets.cmake
    NAMESPACE CompressionLib::
    DESTINATION ${INSTALL_CMAKE_DIR}
) 