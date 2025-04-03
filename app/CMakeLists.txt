add_executable(compress_app main.cpp)

target_link_libraries(compress_app PRIVATE compression)

# --- Benchmark Executable ---
add_executable(compression_benchmark benchmark.cpp)
target_link_libraries(compression_benchmark PRIVATE compression)
# Require C++17 for <filesystem>
set_target_properties(compression_benchmark PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
# Pass data directory path as a compile definition
target_compile_definitions(compression_benchmark PRIVATE BENCHMARK_DATA_DIR="${CMAKE_SOURCE_DIR}/data")


# --- Installation --- 
install(TARGETS compress_app 
    RUNTIME DESTINATION ${INSTALL_BIN_DIR}
    LIBRARY DESTINATION ${INSTALL_LIB_DIR}
    ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
)

# Optionally install benchmark tool (or not, depending on preference)
# install(TARGETS compression_benchmark RUNTIME DESTINATION ${INSTALL_BIN_DIR}) 