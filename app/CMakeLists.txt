add_executable(compress_app main.cpp)

target_link_libraries(compress_app PRIVATE compression)

# --- Installation --- 
install(TARGETS compress_app 
    RUNTIME DESTINATION ${INSTALL_BIN_DIR}
    LIBRARY DESTINATION ${INSTALL_LIB_DIR}
    ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
) 