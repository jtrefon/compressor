# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - YYYY-MM-DD

### Fixed
- Resolved a critical build failure caused by namespace pollution. A missing closing brace in `include/compression/FileFormat.hpp` was causing `std` library symbols to be misinterpreted within the `compression` namespace, leading to compilation errors when using standard headers like `<numeric>`.
- The `BwtCompressor.cpp` file has been restored and is now included in the build.

### Changed
- The project now builds cleanly with CMake, and all multi-threading and compression logic is functional.
