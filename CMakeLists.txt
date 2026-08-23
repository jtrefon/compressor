cmake_minimum_required(VERSION 3.15)
project(CompressionLib VERSION 1.6.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Installation Paths --- 
include(GNUInstallDirs)
set(INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME} CACHE PATH "Include install directory for the project")
set(INSTALL_LIB_DIR ${CMAKE_INSTALL_LIBDIR} CACHE PATH "Library install directory")
set(INSTALL_BIN_DIR ${CMAKE_INSTALL_BINDIR} CACHE PATH "Executable install directory")
set(INSTALL_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME} CACHE PATH "CMake package config install directory")

message(STATUS "Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Include Dir: ${INSTALL_INCLUDE_DIR}")
message(STATUS "Library Dir: ${INSTALL_LIB_DIR}")
message(STATUS "Binary Dir: ${INSTALL_BIN_DIR}")
message(STATUS "CMake Config Dir: ${INSTALL_CMAKE_DIR}")

# Enable testing
enable_testing()

# Consumable-package options: when CompressionLib is pulled in via
# FetchContent/add_subdirectory, apps and tests stay OFF by default so
# consumers get the library only.
option(COMPRESSIONLIB_BUILD_APPS "Build CLI and benchmark apps" OFF)
option(COMPRESSIONLIB_BUILD_TESTS "Build unit tests" OFF)

# Add subdirectories
add_subdirectory(src)
if(COMPRESSIONLIB_BUILD_APPS)
  add_subdirectory(app)
endif()
if(COMPRESSIONLIB_BUILD_TESTS)
  add_subdirectory(tests)
endif()

# --- Documentation (Doxygen) --- 

find_package(Doxygen)

if(DOXYGEN_FOUND)
    set(DOXYGEN_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/docs/doxygen)
    set(DOXYGEN_INPUT_DIR ${CMAKE_SOURCE_DIR})
    set(DOXYGEN_CONFIG_FILE ${CMAKE_SOURCE_DIR}/Doxyfile)

    # Add a custom target to run Doxygen
    # NOTE: Removed 'ALL' to make doc generation optional for CI/CD builds
    # Developers can still build docs manually with: make doc
    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIG_FILE}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )

    # Optional: Add a target to clean the documentation
    add_custom_target(doc_clean
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${DOXYGEN_OUTPUT_DIR}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Cleaning Doxygen documentation"
        VERBATIM
    )

    message(STATUS "Doxygen found. Added 'doc' and 'doc_clean' targets.")
else()
    message(WARNING "Doxygen not found. Documentation target 'doc' will not be available.")
endif()

# --- Installation Rules --- 

# Install public headers
install(DIRECTORY include/compression DESTINATION ${INSTALL_INCLUDE_DIR}
    PATTERN "legacy" EXCLUDE)

# CMake package config files are generated and installed in src/CMakeLists.txt

message(STATUS "Install rules added.") 