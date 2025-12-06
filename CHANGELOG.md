# Changelog

## [1.4.0] - 2025-12-06
### Added
- **Adaptive Order-1 Arithmetic Coding**: Replaced the placeholder implementation with a high-performance adaptive range coder.
- **CRC32 Integrity Check**: Added CRC32 checksums to compressed streams for robust error detection.

### Changed
- **BWT Pipeline Optimized**: Switch BWT backend to use the new Arithmetic Coder, removing the legacy ZRL/Huffman stages.
- **Compression Ratio Improved**: BWT compression ratio improved from ~30% to ~26.8% on benchmark data.
- **Performance**: Compress time significantly improved for high-ratio algorithms.

## [1.3.0] - 2025-11-26
### Added
- Initial release preparation.
