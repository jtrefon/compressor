# Code Review Checklist

Checklist for reviewing changes to this C++ compression library. Work through
the items below in order; the correctness and safety sections are the ones
that have historically caught real bugs in this codebase.

---

### 1. Correctness

- **Roundtrip with ratio, not just roundtrip:** a test that only checks
  `decompress(compress(x)) == x` passes even if the compressor is a no-op.
  Compressible inputs (text-like, repetitive) must also assert
  `compressed.size() < data.size()`.
- **Decision is stored, not re-guessed:** anything chosen during compression
  (strategy, transform, method) must be written into the stream and read back
  on decompression. Heuristics that re-derive the choice on decompress
  (e.g. `detectBWTUsage`) silently corrupt data and must not be added.
- **Format changes are versioned:** bump the format version or add a flags
  byte when a pipeline changes; keep reading previous versions. Backward
  compatibility is expected unless the old format was broken.
- **Marker ambiguity:** any reserved byte used as a run/escape marker must be
  escaped as a literal (see the 0xFF RLE bug in OptimizedCompressor). A lone
  reserved byte mid-stream must roundtrip.
- **Sentinel/rank collisions:** sentinel values used in sort keys (BWT
  primary index, EOF markers) must not collide with real data bytes. This
  bit us twice: `0` as out-of-range rank collides with real 0x00 bytes.
- **Bounded indices:** every index read from (potentially untrusted)
  compressed data must be validated before use (`primary_index < n`,
  chunk offsets, symbol counts, match distances).

### 2. Robustness / hostile input

- **Untrusted header fields are capped and validated before use:** chunk
  counts, original sizes, and symbol counts come from the file. Validate
  them before sizing thread pools, reserving buffers, or looping.
- **CRC/checksum computed on compress is verified on decompress.**
- **Decompression must not loop forever, OOB-read, or allocate absurdly** on
  truncated or corrupted input; it must throw `std::runtime_error`.
- **Library code has no console side effects:** no `std::cout`/`std::cerr`
  in `src/`. Errors are exceptions only.

### 3. Concurrency

- **Shared compressor instances must be safe to call from multiple threads.**
  No mutable shared state during `compress()`/`decompress()`; run strategies
  on local instances (`ExtremeCompressor` pattern) rather than shared members.
- **Thread pools:** `ThreadPool(0)` throws; worker count derived from input
  is bounded.

### 4. Testing

- New behavior needs a regression test in `tests/` (gtest), registered in
  `tests/CMakeLists.txt`. The suite must run green with **zero** `DISABLED_`
  tests.
- Corruption tests for new decompression paths: flip a payload byte
  mid-stream (not the trailing byte, which can hold dead padding bits) and
  assert throw-or-differ.
- Thread-count and chunk-boundary variation for anything parallel.

### 5. Hygiene

- No dead code, stub functions, or `// ... existing code ...` leftovers.
- No committed build artifacts: `Makefile*`, `CompressionLibConfig*.cmake`,
  test binaries, `docs/doxygen/`.
- No unresolved merge conflict markers (`<<<<<<<` / `=======` / `>>>>>>>`).
- README/CHANGELOG reflect what the code actually does (strategy list,
  benchmark behavior, test paths).

### 6. Performance

- No accidental O(n²) over whole inputs (the Hybrid `analyzeData` pattern).
- No full-size intermediate buffers that scale with input (the old
  byte-per-bit Huffman buffer) when a streaming pack is possible.
- Benchmark ratios must be unchanged (or better) for existing strategies
  when touching core compressors; verify with `./app/compression_benchmark --quick`.
