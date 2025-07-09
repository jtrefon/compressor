# Compression Library

[![Build and Test](https://github.com/jtrefon/compressor/actions/workflows/build-test.yml/badge.svg)](https://github.com/jtrefon/compressor/actions/workflows/build-test.yml)
[![Release](https://github.com/jtrefon/compressor/actions/workflows/release.yml/badge.svg)](https://github.com/jtrefon/compressor/actions/workflows/release.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

A high-performance file compression library implemented in C++ that provides multiple compression algorithms with a focus on achieving excellent compression ratios for files at rest.

## Features

- Multiple compression algorithms:
  - **Null Compressor**: Reference implementation with no compression
  - **RLE (Run-Length Encoding)**: Simple compression for data with repeated patterns
  - **Huffman Coding**: Statistical compression using variable-length codes
  - **LZ77**: Dictionary-based compression using sliding window technique
  - **Deflate**: Combined LZ77 and Huffman coding (similar to gzip/zlib)

- Optimized implementations:
  - Fast hash-based string matching for LZ77
  - Efficient bit-level encoding and decoding
  - Robust error handling for corrupted data
  - Optional multithreaded mode for large files

## Project Structure

```
compression/
├── CMakeLists.txt         # Main CMake configuration
├── README.md              # Project overview (this file)
├── .gitignore             # Git ignore rules
├── include/               # Public header files for the library
│   └── compression/       # Library public API headers
├── src/                   # Source files for the library implementation
├── app/                   # Source files for applications
│   ├── main.cpp           # Compression utility app
│   └── benchmark.cpp      # Benchmark application
├── tests/                 # Unit and integration tests
├── docs/                  # Documentation files
└── data/                  # Test data for benchmarking
```

## Building the Project

### Prerequisites

- C++17 compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.14+
- Git (for downloading dependencies)

### Build Steps

1. **Clone the repository:**
   ```bash
   git clone https://github.com/jtrefon/compression.git
   cd compression
   ```

2. **Configure:** Create a build directory and run CMake:
   ```bash
   mkdir build
   cd build
   cmake ..
   ```

3. **Build:** Compile the library, applications, and tests:
   ```bash
   cmake --build build
   ```
   
   Or, for multi-core builds:
   ```bash
   cmake --build build -j$(nproc)
   ```

## Running Tests

After building, run the tests to verify everything is working correctly:

```bash
cd build
./tests/compression_tests
```

To run a specific test category:
```bash
./tests/compression_tests --gtest_filter="Lz77CompressorTest.*"
```

Available test categories:
- `NullCompressorTest.*`
- `RleCompressorTest.*`
- `HuffmanCompressorTest.*`
- `Lz77CompressorTest.*`
- `DeflateCompressorTest.*`

## Running Benchmarks

Benchmark different compression algorithms using the benchmark application:

```bash
cd build
./app/compression_benchmark
```

The benchmark results will be displayed in the console and also written to a file named `BENCHMARKS.md` in the project root directory.

The benchmark:
- Tests all implemented compression algorithms
- Reports original and compressed size
- Calculates compression ratio
- Measures compression and decompression time
- Uses real-world test data from the `data/` directory

## Usage Examples

### Basic Library Usage

```cpp
#include <compression/Lz77Compressor.hpp>
#include <vector>
#include <iostream>

int main() {
    // Create data to compress
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o', ',', ' ', 'w', 'o', 'r', 'l', 'd', '!'};
    
    // Create compressor instance
    compression::Lz77Compressor compressor;
    
    // Compress data
    std::vector<uint8_t> compressed = compressor.compress(data);
    
    // Decompress data
    std::vector<uint8_t> decompressed = compressor.decompress(compressed);
    
    // Check compression stats
    std::cout << "Original size: " << data.size() << " bytes\n";
    std::cout << "Compressed size: " << compressed.size() << " bytes\n";
    std::cout << "Compression ratio: " << (float)compressed.size() / data.size() * 100 << "%\n";
    
    return 0;
}
```

### Command-line Utility

```bash
# Compress a file using the LZ77 algorithm
./app/compress_app compress lz77 input.txt output.compressed

# Decompress a file (strategy argument is ignored)
./app/compress_app decompress ignored output.compressed restored.txt

# Compress with a specific thread count (default uses all cores)
./app/compress_app compress lz77 input.txt output.compressed --threads 4

# Disable multithreading
./app/compress_app compress lz77 input.txt output.compressed --no-threads
```

## API Documentation

The library offers a simple interface for compression operations:

- All compressors implement the `ICompressor` interface
- Main methods: `compress()` and `decompress()`
- Common parameters and return types for all algorithms
- Thread-safe implementations for concurrent use

Generate detailed API documentation with Doxygen:

```bash
cmake --build build --target doc
```

Then open `build/docs/html/index.html` in your browser.

## Contributing

Contributions are welcome! Here's how you can contribute:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Run tests to ensure they pass (`./tests/compression_tests`)
5. Commit your changes (`git commit -m 'Add amazing feature'`)
6. Push to the branch (`git push origin feature/amazing-feature`)
7. Open a Pull Request

Please ensure that your code follows the existing style, includes appropriate tests, and all tests pass.

## License

This project is licensed under the GNU General Public License v3.0 (GPLv3) - see the LICENSE file for details.

The GPLv3 is a strong copyleft license that ensures the software and its derivatives remain free and open source. This means you can:

- Use the software for any purpose
- Study how the software works and modify it
- Redistribute the software
- Improve the software and release your improvements to the public

Any modifications or software that incorporates this code must also be released under the GPLv3.
