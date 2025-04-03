add_library(compression)

target_include_directories(compression 
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:${INSTALL_INCLUDE_DIR}> # Use install path
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} # Needed for internal headers if any
)

# Placeholder for source files - we'll add actual source files here later
target_sources(compression PRIVATE
    NullCompressor.cpp
    RleCompressor.cpp
    HuffmanCompressor.cpp
    Lz77Compressor.cpp
#     some_compression_algorithm.cpp
)

# Example of linking dependencies if needed in the future
# target_link_libraries(compression PRIVATE SomeDependency) 

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

# --- Optional: Build as Shared Library (Uncomment if desired) --- 
# set_property(TARGET compression PROPERTY POSITION_INDEPENDENT_CODE ON)
# add_library(compression_shared SHARED $<TARGET_OBJECTS:compression>)
# set_target_properties(compression_shared PROPERTIES 
#     VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
#     SOVERSION ${PROJECT_VERSION_MAJOR}
# )
# install(TARGETS compression_shared
#     EXPORT CompressionLibTargets
#     LIBRARY DESTINATION ${INSTALL_LIB_DIR}
#     RUNTIME DESTINATION ${INSTALL_BIN_DIR}
# ) 