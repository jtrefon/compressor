# Contributing to Compression Library

Thank you for your interest in contributing to the Compression Library! This document provides guidelines and instructions for contributing to the project.

## Table of Contents

- [Development Setup](#development-setup)
- [Building and Testing](#building-and-testing)
- [Coding Standards](#coding-standards)
- [Testing Requirements](#testing-requirements)
- [Pull Request Process](#pull-request-process)
- [Code Review Checklist](#code-review-checklist)

## Development Setup

### Prerequisites

- **C++17 compatible compiler**:
  - GCC 8+ on Linux
  - Clang 7+ on macOS
  - MSVC 2019+ on Windows
- **CMake 3.14 or higher**
- **Git** for version control

### Setting Up Your Development Environment

1. **Fork and clone the repository**:
   ```bash
   git clone https://github.com/YOUR_USERNAME/compressor.git
   cd compressor
   ```

2. **Create a build directory**:
   ```bash
   mkdir build
   cd build
   ```

3. **Configure the project**:
   ```bash
   cmake ..
   ```

4. **Build the project**:
   ```bash
   cmake --build . -j$(nproc)
   ```

## Building and Testing

### Building

```bash
# From the build directory
cmake --build .
```

For a release build:
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

### Running Tests

```bash
# Run all tests
./tests/compression_tests

# Run tests with verbose output
./tests/compression_tests --gtest_output=xml

# Run specific test suite
./tests/compression_tests --gtest_filter="Lz77CompressorTest.*"

# Run specific test
./tests/compression_tests --gtest_filter="Lz77CompressorTest.SimpleRepeatingPattern"
```

### Running Benchmarks

```bash
./app/compression_benchmark
```

## Coding Standards

### Code Style

- **Indentation**: 4 spaces (no tabs)
- **Line length**: Maximum 100 characters (soft limit)
- **Naming conventions**:
  - Classes: `PascalCase` (e.g., `HuffmanCompressor`)
  - Functions/methods: `camelCase` (e.g., `compressData`)
  - Member variables: `camelCase_` with trailing underscore (e.g., `bufferSize_`)
  - Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_BUFFER_SIZE`)
  - Namespaces: `lowercase` (e.g., `compression`)

### Code Organization

- **Header files** go in `include/compression/`
- **Implementation files** go in `src/`
- **Test files** go in `tests/` with suffix `Test.cpp`
- **One class per file** when possible

### Documentation

- **All public APIs** must have Doxygen comments
- **Complex algorithms** should have inline comments explaining the logic
- **Example**:
  ```cpp
  /**
   * @brief Compresses data using the LZ77 algorithm
   * @param data The input data to compress
   * @return Compressed data as a byte vector
   * @throws std::runtime_error if compression fails
   */
  std::vector<uint8_t> compress(const std::vector<uint8_t>& data) const override;
  ```

##  Testing Requirements

### All New Code Must Have Tests

- **Every new compressor** must have a corresponding test file
- **Minimum test coverage**:
  - Empty data test
  - Single byte test
  - Small data test
  - Large data test
  - Round-trip verification
  - Edge cases specific to the algorithm

### Test File Template

```cpp
#include <gtest/gtest.h>
#include <compression/YourCompressor.hpp>

class YourCompressorTest : public ::testing::Test {
protected:
    compression::YourCompressor compressor;
};

TEST_F(YourCompressorTest, CompressEmptyData) {
    std::vector<uint8_t> data;
    auto compressed = compressor.compress(data);
    EXPECT_TRUE(compressed.empty());
}

TEST_F(YourCompressorTest, RoundTripTest) {
    std::vector<uint8_t> data = {/* test data */};
    auto compressed = compressor.compress(data);
    auto decompressed = compressor.decompress(compressed);
    EXPECT_EQ(data, decompressed);
}
```

### Running Tests Before Committing

**Always run the full test suite before committing**:
```bash
cd build
cmake --build .
./tests/compression_tests
```

All tests must pass before submitting a pull request.

## Pull Request Process

1. **Create a feature branch**:
   ```bash
   git checkout -b feat/your-feature-name
   ```

2. **Make your changes** following the coding standards

3. **Add tests** for your changes

4. **Run tests locally**:
   ```bash
   ./tests/compression_tests
   ```

5. **Commit your changes**:
   ```bash
   git add .
   git commit -m "feat: Add your feature description"
   ```

   Use conventional commit messages:
   - `feat:` for new features
   - `fix:` for bug fixes
   - `docs:` for documentation changes
   - `test:` for adding tests
   - `refactor:` for code refactoring

6. **Push to your fork**:
   ```bash
   git push origin feat/your-feature-name
   ```

7. **Create a Pull Request** on GitHub

8. **Wait for CI/CD checks** to pass

9. **Address review feedback** if any

## Code Review Checklist

Before submitting your PR, verify:

- [ ] Code compiles without warnings on GCC, Clang, and MSVC
- [ ] All tests pass locally
- [ ] New code has corresponding tests
- [ ] Public APIs have Doxygen documentation
- [ ] Code follows the project's style guidelines
- [ ] No unnecessary dependencies added
- [ ] Performance-critical sections are optimized
- [ ] Error handling is appropriate
- [ ] Memory leaks checked (use valgrind or similar tools)
- [ ] Changes are backward compatible (or documented as breaking changes)

### Platform Testing

If possible, test on multiple platforms before submitting:
- Linux (Ubuntu/Debian)
- macOS (latest)
- Windows (latest)

The CI/CD pipeline will automatically test all three platforms.

## Getting Help

- **Questions**: Open a GitHub Discussion
- **Bugs**: Open a GitHub Issue with reproduction steps
- **Feature requests**: Open a GitHub Issue tagged with "enhancement"

## License

By contributing, you agree that your contributions will be licensed under the GNU General Public License v3.0 (GPLv3), the same license as this project.

---

Thank you for contributing to the Compression Library! 🎉
