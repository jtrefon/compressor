# Changelog

## [2.0.0] - 2026-08-20

### Added

- **Codec registry** (`CodecRegistry`): codecs are discovered and instantiated purely from the registry by `AlgorithmID` — no per-codec dispatch chains in the caller — the open/closed principle proof (OCP) for the hexagonal refactor.
- **`ans` codec** (`AnsCoder`): rANS entropy coder registered under `AlgorithmID::ANS`; wired into the CLI (`compress_app compress ans`), the `ParallelCodecDecorator`, and the format pipeline (`[PLIN]` frames with BWT/RLE transforms, `[CPRO]` containers).
- **Golden format gates** (`tests/format/GoldenTest.cpp`): frozen byte-for-byte fixtures for the ANS payload, BWT+RLE pipeline payload, and `[CPRO]` ANS/optimized frames — any accidental format drift fails the build.
- **Fuzz gates** (`tests/security/FuzzGatesTest.cpp`): seeded, deterministic mutation fuzz across every registry codec and the `ParallelCodecDecorator` framing; asserts no crash and no hang (only `std::runtime_error`-family exceptions).

### Fixed

- **AnsCoder frequency overflow**: a single-symbol input produced `freq = 65536`, overflowing the `uint16_t` frequency table to 0 and causing an infinite encoder renorm loop plus a division by zero; frequencies are now capped at `kFreqSum / 2` with the deficit redistributed.
- **AnsCoder decode order**: normalization ran before the state step-down; reordered to the rANS-canonical slot → step-down → renorm-up.
- **ArithmeticCompressor truncated-stream hang** (pre-existing 1.x robustness hole found by the fuzz gates): `decompress` trusted the untrusted `u32 originalSize` header and decoded up to ~4 billion symbols from a 16-byte corrupt input (minutes of work, a denial-of-service vector). The range decoder now allows the up-to-two-bytes-past-the-end slack that correct streams legitimately need (measured over ~240k round trips) and throws on anything further, bounding decode work to the input size.

### Removed

- **`DeflateCompressor`**, **`EnhancedCompressor`**, **`EnhancedBwtCompressor`**, **`HybridCompressor`** (and support classes) — these were removed in 1.6.1 and remain absent.

## [1.6.1] - 2026-08-18

### Removed

- **DeflateCompressor** (broken constructor arguments made it a literal passthrough; table machinery stubbed), **EnhancedCompressor** (unrecorded BWT decision silently corrupted data on decompress), **EnhancedBwtCompressor** (0x00 sentinel collision in BWT sort, O(n^2) inverse, out-of-bounds reads), **HybridCompressor** (0xFF RLE marker ambiguity, O(n^2) data analysis), plus the deflate-only `HuffmanCoder`/`ByteTypeAdapter` support classes and scratch benchmark apps.

### Fixed

- **OptimizedCompressor RLE 0xFF ambiguity**: a lone 0xFF literal was written raw and misread as a run header; literals are now escaped (`[0xFF][0x00][0x00][value]`).
- **ParallelCompressor**: chunk count from an untrusted header is validated (cap 1024, reject 0) before sizing the thread pool; the checksum computed on compress is now verified on decompress; output reserve is bounded.
- **ArithmeticCompressor**: reserve bounded on the untrusted 4-byte size header.
- **ThreadPool(0)** now throws instead of hanging forever.
- **UltraCompressor**: raw (strategy-0) streams are no longer misrouted through the wrapped-header dispatch; unknown strategies throw cleanly.
- **Lz77Compressor**: match search uses the configured window size and minimum match length instead of hardcoded 32768/3.
- **HuffmanCompressor**: removed library-side `std::cerr` output.
- **ExtremeCompressor**: removed stdout banner, strategies run on local instances (thread-safe on a shared object), dead strategies dropped.

### Performance

- **HuffmanCompressor** packs code bits directly into the output, eliminating a transient byte-per-bit buffer (~8x input size). Output is byte-identical.

### Tests & CI

- Test suite grown from 72 tests / 8 disabled to **105 tests / 0 disabled**: new Ultra/Extreme suites, corruption and multi-chunk tests, ratio assertions, re-enabled Lz77 `InvalidFormat`.
- CI now fails on benchmark algorithm errors, guards against merge-conflict markers, and runs the full suite under ASan/UBSan.
- Release workflow runs `ctest` before tagging and publishes SHA-256 checksums.

## [1.6.0] - 2026-07-12

### Added

- **Correct Witten–Neal–Cleary range coder** replacing the broken carry/renormalization implementation; fixes arithmetic round-trip failures on random/skewed/large data.
- **BWT EOF-sentinel handling**: escape sequences for all-0x00/0xFF inputs, fixing the BWT primary-index collision on periodic data.
- **BugRegressionTest suite** (12 cases) covering BWT and arithmetic edge cases.

### Changed

- **Arithmetic decoder** uses Fenwick binary lifting for O(log N) symbol lookup (decompression speedup on large inputs).
- **BWT MTF** encode/decode use `std::rotate` instead of per-symbol erase/insert.
- **HuffmanCompressor** writes bits via a `std::vector<uint8_t>` buffer.
- **Ultra/Extreme** debug output removed; Extreme simplified to robust fallback strategies.
- **OptimizedCompressor** redundant 0xFF RLE run condition dropped.

### Fixed

- **BWT EOF-marker collision** and escape handling for edge-case inputs.

## [1.5.0] - 2025-12-13

### Changed

- Ultra/Extreme compressors now operate on raw BWT transform data and include safe fallbacks to avoid expansion on incompressible inputs.

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
