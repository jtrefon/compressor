# Porting Guide: consuming CompressionLib from UI repositories

Target audience: maintainers of `compressor-windows` / `compressor-macos`.

## 1. Dependency wiring (pick one)

### Option A — FetchContent pinned to a tag (recommended)

```cmake
include(FetchContent)
FetchContent_Declare(
    compressionlib
    GIT_REPOSITORY https://github.com/jtrefon/compressionlib.git
    GIT_TAG        v1.6.2          # exact tag, never a branch
)
FetchContent_MakeAvailable(compressionlib)
```

Then link `CompressionLib::compression` (headers + library) — the package
target works identically via `add_subdirectory`.

### Option B — installed package

```cmake
find_package(CompressionLib REQUIRED)
# target: CompressionLib::compression
```

## 2. Pin policy

- Pins move **only** via an intentional bump, reviewed against the engine
  [CHANGELOG](../CHANGELOG.md).
- PATCH/MINOR engine releases (same MAJOR): formats and API are stable —
  bump freely after a round-trip test.
- MAJOR engine release: test stream compatibility explicitly before bumping.
- A stream produced by a newer engine version must be **rejected loudly**
  (typed errors), never mis-decoded.

## 3. The anti-corruption contract (MUST)

1. **Byte-identical round trips**: every compress → decompress cycle must
   return the exact input. Reference vectors: `tests/format/GoldenFixtures.inc`
   in the engine; the engine CLI (`compress_app`) is the reference adapter.
   The UI's interop tests must run the same payloads through its bindings and
   compare byte-for-byte.
2. **Surface CRC failures**: `ExtractResult::verified`, `ChecksummedCodec`
   and archive `verify()` must surface to the user. Never silently ignore.
3. **Typed errors**: map `core::InvalidFormatError` / `CorruptDataError` /
   `ConfigurationError` / `IoError` to user-visible errors.
4. **64-bit lengths**: `uint64_t`/`size_t` end-to-end at the interop
   boundary; no truncation to 32-bit, no signed/unsigned mismatches.
5. **Ownership**: `core::ByteView` (non-owning) / `core::IByteSink` (owning)
   are the boundary types; document who owns the buffer at every crossing.

## 4. Public surface (installed package)

`core/`, `format/`, `codec/` (registry, pipeline, decorators), `archive/`,
`events/`, `app/` (facades), plus `ICompressor.hpp`, `FileFormat.hpp`,
`ThreadPool.hpp`, `Crc32.hpp`, `SystemInfo.hpp`. Legacy codec implementations
(`codec/legacy/`) are **not** installed — reach codecs only through
`CodecRegistry`.

## 5. Reference smoke test

See `examples/consumer/` — a minimal CMake project that round-trips data
through `CompressionService` and creates/lists/verifies a `.cza` archive,
buildable in either dependency mode (A or B).