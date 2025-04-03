# Compression Library

This project aims to develop a cutting-edge file compression library focused on achieving high compression ratios for files at rest.

## Project Structure

```
compression/
├── CMakeLists.txt         # Main CMake configuration
├── README.md              # Project overview (this file)
├── .gitignore             # Git ignore rules
├── include/               # Public header files for the library
│   └── compression/
│       └── dummy.hpp      # Placeholder header
├── src/                   # Source files for the library
│   └── CMakeLists.txt     # Library CMake configuration
│   # *.cpp                # Library implementation files will go here
├── app/                   # Source files for the example application
│   ├── CMakeLists.txt     # Application CMake configuration
│   └── main.cpp           # Application entry point
├── tests/                 # Unit and integration tests
│   ├── CMakeLists.txt     # Tests CMake configuration (includes GoogleTest)
│   └── main.cpp           # Test runner entry point
├── docs/                  # Documentation files (e.g., Doxygen config, Markdown)
└── external/              # Directory for external dependencies (if not managed by CMake)
```

## Building the Project

This project uses CMake for building.

1.  **Configure:** Create a build directory and run CMake:
    ```bash
    mkdir build
    cd build
    cmake ..
    ```

2.  **Build:** Compile the library, application, and tests:
    ```bash
    cmake --build .
    ```

3.  **Run Tests:** Execute the tests:
    ```bash
    ctest
    ```

4.  **Run Application:** The application executable will be in the `build/app` directory:
    ```bash
    ./app/compress_app
    ```

## Contributing

(Details on contributing guidelines will go here)

## License

(License information will go here)
