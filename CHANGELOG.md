# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.2.0] - 2025-11-25

### Added
- **Zero-Run-Length (ZRL) Encoding**: Implemented a specialized encoding scheme for BWT output, improving compression ratio by ~15%.

### Changed
- **ExtremeCompressor Optimization**: Parallelized the execution of compression strategies, resulting in a ~4.4x speedup.
- **BWT Optimization**: Replaced the Suffix Array construction algorithm with a cyclic doubling approach (O(N) space), resulting in a ~5.5x speedup.

## [1.1.0] - YYYY-MM-DD

### Fixed
- Resolved a critical build failure caused by namespace pollution. A missing closing brace in `include/compression/FileFormat.hpp` was causing `std` library symbols to be misinterpreted within the `compression` namespace, leading to compilation errors when using standard headers like `<numeric>`.
- The `BwtCompressor.cpp` file has been restored and is now included in the build.

### Changed
- The project now builds cleanly with CMake, and all multi-threading and compression logic is functional.
