# Architecture Proposal — CompressionLib 2.0

Status: Proposal (for review)
Scope: Full re-architecture of the codebase into a layered, SOLID, pattern-driven
library that CLI, WinUI, and future clients consume through one stable facade.

---

## 1. Executive summary

The codebase is already close to being a real library — it builds a `compression`
target with install/export rules and a CMake package config (`src/CMakeLists.txt`),
and the compressors are isolated behind the `ICompressor` Strategy interface.
What it lacks is **layering**: domain rules, container-format logic, concurrency,
and CLI concerns are interleaved across three places, and three "private"
container formats have grown inside composite compressors. The result is a
working product with duplicated factories, duplicated framing, and no single
entry point a UI client could call.

This proposal reorganizes the codebase into a **hexagonal (ports & adapters)
layered architecture**:

```
┌──────────────────────────────────────────────────────────────┐
│ ADAPTERS:  CLI · WinUI · Benchmark · C API · Python bindings │
├──────────────────────────────────────────────────────────────┤
│ APPLICATION:  Facade (CompressionService / ArchiveService)   │
│               Use Cases · Commands · Composition Root (DI)   │
├──────────────────────────────────────────────────────────────┤
│ DOMAIN:  Codecs (Strategy/Composite/Pipeline) · Transforms   │
│          Entropy coders · Content analysis · Format/Frame    │
│          registry · Archive (.cza) model · Events (bus)      │
├──────────────────────────────────────────────────────────────┤
│ PORTS (owned by domain): IByteSource/Sink · IExecutor ·      │
│          IProgressSink · ISystemInfo · ILogger               │
├──────────────────────────────────────────────────────────────┤
│ INFRASTRUCTURE: File IO · ThreadPool/Inline executors ·      │
│          Platform adapters (win32/posix/darwin) · Binary IO  │
└──────────────────────────────────────────────────────────────┘
```

Dependency rule: **everything points inward**. Domain has zero dependencies on
infrastructure or adapters; adapters depend only on application + ports.
No layer may reference a layer further out. This is what makes the library
reusable: the facade is the only surface CLI and UI need to know.

---

## 2. Current-state audit (evidence)

### 2.1 Strengths (keep these)

| Strength | Evidence |
|---|---|
| Real installable library | `src/CMakeLists.txt:28-67` exports `CompressionLib::compression` + package config |
| Strategy interface works | `include/compression/ICompressor.hpp` — LSP-respecting base, used by all codecs |
| Library is IO-free | no `cout`/`cerr`/`printf` in `src/` |
| Solid hostile-input hardening exists | `src/ParallelCompressor.cpp:133-173` (chunk-count and reserve bounds, CRC verify) |
| Good per-codec test coverage | `tests/*.cpp`, GoogleTest + CTest discovery |
| Platform isolation already conceived | `src/SystemInfo.cpp` isolates OS calls behind functions |

### 2.2 Violations to fix

| # | Problem | Evidence | Violation |
|---|---|---|---|
| 1 | **Factory duplicated** | `createCompressorById` in `app/main.cpp:63-85` **and** `src/ParallelCompressor.cpp:17-43` | DRY; adding an algorithm needs edits in N places |
| 2 | **Framing logic duplicated** | header+payload assembly in `app/main.cpp` (compress & decompress) and again in `src/ParallelCompressor.cpp:59-119` | SRP; format layer leaky |
| 3 | **Four ad-hoc container formats** | `FileFormat.hpp` (v1) + private method-byte dispatch in `OptimizedCompressor` (0x00–0x06, `src/OptimizedCompressor.cpp:54-68`), `ExtremeCompressor` (0xFE + index, `src/ExtremeCompressor.cpp`), `UltraCompressor` (0xFFFFFFF0 wrapper, `src/UltraCompressor.cpp:30-63`) | Composite codecs each invented their own framing; no registry; can't evolve independently |
| 4 | **Hand-rolled endian serialization ×~30** | `include/compression/FileFormat.hpp:87-191` repeats the same byte-shift loops | DRY; fragile; no `BinaryWriter/Reader` |
| 5 | **Inconsistent abstraction** | `ParallelCompressor` does **not** implement `ICompressor`, while everything else does | LSP/consistency; decorators can't wrap it |
| 6 | **Two concurrency mechanisms** | `ExtremeCompressor` uses raw `std::async` (`src/ExtremeCompressor.cpp:28-35`); `ParallelCompressor` uses `ThreadPool` | No single executor policy; untestable determinism |
| 7 | **Const-correctness fudge** | `UltraCompressor.hpp:53-55` uses `mutable` members so const methods can mutate | State hiding; blocks DI of pre-built children |
| 8 | **Stringly-typed errors** | raw `std::runtime_error("...")` everywhere; no hierarchy | Callers (UI) can't distinguish corruption from config error |
| 9 | **CLI entangled with domain** | `app/main.cpp` (274 lines) contains strategy factory, header serialization, CRC verification, parallel dispatch | The facade is missing; UI would duplicate all of it |
| 10 | **Magic numbers as config** | `Lz77Compressor(32768, 3, 258, false, true, true)` positional booleans; window sizes repeated in 4 files | No options value objects |
| 11 | **Platform code inside `#ifdef`** | `src/SystemInfo.cpp` mixes three OSes in one TU | Adapter pattern missing; future pollution risk |
| 12 | **Style drift** | `#pragma once` vs `#ifndef` guards; `"ICompressor.hpp"` vs `<compression/...>` includes; snake_case (`bwt_transform`) next to camelCase; unused `#include <iostream>` in `src/UltraCompressor.cpp:6` | Consistency is a quality feature |
| 13 | **No fuzzing / format golden tests** | decode paths are the attack surface; only round-trip tests exist | Security posture incomplete |

---

## 3. Domain layer

### 3.1 Core codec abstraction (Strategy, refined with ISP)

Split the one-fat-interface into segregated roles so decorators and pipelines
compose cleanly:

```cpp
// domain/codec/Codec.hpp
namespace compression::domain {

using ByteView = std::span<const std::uint8_t>;   // C++20; gsl::span shim for C++17

class IEncoder {
public:
    virtual ~IEncoder() = default;
    virtual void encode(ByteView src, ByteSink& dst, const CodecContext& ctx) const = 0;
};

class IDecoder {
public:
    virtual ~IDecoder() = default;
    virtual void decode(ByteView src, ByteSink& dst, const CodecContext& ctx) const = 0;
};

// Static information, self-describing (used by registry, CLI --list, UI dropdowns)
struct CodecInfo {
    CodecId id;                 // stable 16-bit id, never reused
    std::string_view name;      // "huffman"
    std::string_view display;   // "Huffman coding"
    Version version;            // codec's own format version
};

class ICodec : public IEncoder, public IDecoder {
public:
    virtual CodecInfo info() const = 0;
};

} // namespace compression::domain
```

Notes:
- Output goes to a **`ByteSink` port** instead of returning vectors → enables
  streaming to file, to UI buffers, to `ByteWriter`; kills allocation churn on
  the hot path.
- Codecs are **stateless by contract** (the `mutable`-lazy-init hack in
  `UltraCompressor` disappears; children are injected in the constructor).
- `CodecContext` carries per-call options (level, dictionary, memory budget).

### 3.2 Pipeline composition (Composite + Template Method)

BWT/MTF/RLE are **reversible transforms**, Huffman/Arithmetic/ANS are
**entropy coders**. Today both masquerade as `ICompressor`. Split them:

```cpp
class ITransform {
public:
    virtual void forward(ByteView src, ByteSink& dst, const CodecContext&) const = 0;
    virtual void inverse(ByteView src, ByteSink& dst, const CodecContext&) const = 0;
};

class IEntropyCoder {
public:
    virtual void encode(ByteView src, ByteSink& dst, const CodecContext&) const = 0;
    virtual void decode(ByteView src, ByteSink& dst, const CodecContext&) const = 0;
};
```

Then:

- `BwtTransform`, `MtfTransform`, `RleTransform` implement `ITransform`.
- `HuffmanCoder`, `ArithmeticCoder`, future `AnsCoder` implement `IEntropyCoder`.
- A **`PipelineCodec`** (Composite) = `[ITransform...] → IEntropyCoder`, exposed
  as one `ICodec`. `Optimized`, `Ultra`, `Extreme` become *named pipeline
  configurations* + a `StrategySelector`, not bespoke classes with hand-rolled
  framing. New algorithm combos become data, not code.
- **Template Method**: `CodecBase` provides the skeleton — validate input →
  (optional) analyze → run stages → emit frame → report event — with hooks
  `doEncode`/`doDecode`. Every codec inherits the same guards, event emission,
  and metrics collection for free.

### 3.3 Registry (self-registration — Open/Closed Principle)

```cpp
class CodecRegistry {
public:
    static CodecRegistry& instance();                    // immutable after link time
    void registerCodec(std::unique_ptr<CodecPrototype>); // called by static registrars
    const CodecInfo* find(CodecId) const;
    const CodecInfo* find(std::string_view name) const;
    std::unique_ptr<ICodec> create(CodecId, const CodecOptions&) const;
};

#define COMPRESSION_REGISTER_CODEC(T) \
    static const CodecRegistrar<T> registrar_##T{/*id, name, display*/};
```

Adding `AnsCoder` = one new file with `COMPRESSION_REGISTER_CODEC(AnsCoder)`.
Zero edits to factories, CLI, or UI (fixes problem #1, permanently).
The CLI `--list-codecs` and the UI strategy dropdown render from the registry —
no hardcoded names anywhere.

### 3.4 Strategy selection (Strategy + Chain of Responsibility)

`Optimized` today hard-codes "try all five, keep smallest". Extract the
decision:

```cpp
class IContentAnalyzer {
public:
    // Returns 0.0..1.0 confidence that "I am best for this data"
    virtual float confidence(ByteView sample) const = 0;
    virtual CodecId codecId() const = 0;
};

class ContentClassifier {                       // Chain of Responsibility
public:
    void add(std::unique_ptr<IContentAnalyzer>);
    std::vector<RankedCodec> rank(ByteView data) const;  // sorted by confidence
};

class StrategySelector {
public:
    CodecId select(ByteView data, const CompressionOptions& opts) const;
};
```

Analyzers: `RepetitiveAnalyzer`, `PatternAnalyzer`, `EntropyAnalyzer`,
`BwtFriendlyAnalyzer`. The `--level` knob maps to *how many* candidates the
selector may trial — extremes trial more; fast modes trust the analyzer.

### 3.5 Archive domain (`.cza`)

From the win11-ui spec, as pure domain:

```cpp
class ArchiveWriter {   // block-splitting, per-block codec selection, index build
    void addEntry(const EntrySource&, const ArchiveEntryOptions&);
    void finalize(ByteSink&);
};

class ArchiveReader {   // tail → index → random-access blocks
    ArchiveListing list() const;
    void extract(EntryId, ByteSink&, const ExtractOptions&) const; // seeks only its blocks
    std::vector<BlockVerifyResult> verify(ProgressSink&) const;
};
```

Block-level compression delegates to the `CodecRegistry` — the archive layer
does not know about individual algorithms.

---

## 4. Format layer (replaces `FileFormat.hpp`)

### 4.1 Binary IO primitives (DRY fix for #4)

```cpp
class BinaryWriter {                    // Adapter over ByteSink
    void u8/u16/u32/u64(uint64_t);      // explicit little-endian
    void bytes(ByteView);
    void magic(FourCC);
};
class BinaryReader {                    // bounds-checked, throws CorruptDataError
    uint64_t u8/u16/u32/u64();
    ByteView bytes(size_t);
    bool magic(FourCC);
};
```

Every existing hand-rolled loop in `FileFormat.hpp` collapses into these two
classes, unit-tested once.

### 4.2 Frame registry (kills the three private formats, #3)

```cpp
class IFrameCodec {
public:
    virtual FourCC magic() const = 0;
    virtual void write(const FrameModel&, ByteSink&) const = 0;
    virtual FrameModel read(ByteSource&) const = 0;
};

class FrameRegistry { /* keyed by FourCC; LegacyV1Decoder registered for "CPRO" */ };
```

- The current raw 1.x layout (`CPRO` magic + version + algo + sizes) becomes
  `LegacyV1Frame` — **registered** in the registry so 1.x files stay readable
  forever (backward compatibility is a registry entry, not an `if`).
- Composite codecs emit `CompositeFrame { codecId, subFrameId, ... }` — the
  `0xFE` / `0xFFFFFFF0` markers become registry-managed IDs with versioning.
- Format evolution = register a new frame version + a migration function.
  `FormatVersionRegistry` picks the decoder by version (Strategy per version).

---

## 5. Application layer

### 5.1 Facade (the only surface clients see)

```cpp
namespace compression {

struct CompressionOptions { CodecId codec; CompressionLevel level; size_t blockSize; unsigned threads; };
struct CompressResult  { uint64_t inBytes, outBytes; double ratio; uint32_t crc; Milliseconds elapsed; };
struct ExtractResult   { uint64_t inBytes, outBytes; uint32_t crc; bool verified; Milliseconds elapsed; };

class CompressionService {                  // Facade
public:
    CompressResult compressFile(const Path& in, const Path& out, const CompressionOptions&);
    ExtractResult  decompressFile(const Path& in, const Path& out);
    CompressResult compress(ByteView, ByteSink&, const CompressionOptions&);   // in-memory: UI preview
    ExtractResult  decompress(ByteView, ByteSink&);
};

class ArchiveService {                      // Facade for .cza
    void create(const ArchiveBuildOptions&);
    ArchiveListing list(const Path&);
    ExtractResult extract(const Path&, EntrySelection, const Path& outDir, ProgressSink*);
    std::vector<VerifyResult> verify(const Path&, ProgressSink*);
};

} // namespace compression
```

- One header include: `<compression/CompressionService.hpp>`.
- All operations take **ports** (`ProgressSink*` optional) — the CLI passes a
  console sink; WinUI passes a sink that posts to the UI thread; benchmark
  passes a stats sink. The service never talks to any output medium.
- `app/main.cpp` shrinks to argument parsing + one facade call per verb.

### 5.2 Commands (for UI bindability + batch)

```cpp
class ICommand { virtual CommandResult execute(const CommandContext&) = 0; };
class CompressFileCommand : public ICommand { /* built with Options + paths */ };
class ExtractArchiveCommand : public ICommand { ... };
class VerifyCommand : public ICommand { ... };
```

Use cases own one command each; the CLI maps verbs → commands; the UI binds
buttons → commands (undo/queue/batch become free later).

### 5.3 Event bus (Observer) — progress, metrics, telemetry

```cpp
enum class EventType { BlockStarted, BlockCompleted, ChunkProgress, StageEntered, CodecSelected, ChecksumVerified };
struct CompressionEvent { EventType type; uint64_t bytesIn, bytesOut; CodecId codec; uint8_t progressPct; };

class EventBus {                                 // domain-owned, lock-free-ish pub/sub
    void subscribe(EventType, std::weak_ptr<IEventListener>);
    void publish(CompressionEvent&&);
};
```

- Codecs **publish** events; nothing prints.
- CLI adapter subscribes → prints; WinUI adapter subscribes → marshals to UI
  thread → progress ring / toast; benchmark adapter subscribes → aggregates
  timings. This is the "event bus for parallelism" case from the original ask:
  parallel chunk jobs publish `BlockCompleted`; the UI coalesces them.

---

## 6. Ports & infrastructure

### 6.1 Ports (owned by domain, implemented by infrastructure)

| Port | Purpose | Implementations |
|---|---|---|
| `IByteSource` / `IByteSink` | data flow without vector returns | `MemorySource/Sink` (tests), `FileSource/Sink`, `BufferSink` |
| `IExecutor` | all concurrency | `InlineExecutor` (tests, deterministic), `ThreadPoolExecutor` (prod; per-worker queues + work-stealing), `AsyncExecutor` |
| `ISystemInfo` | hardware capabilities | `Win32SystemInfo`, `DarwinSystemInfo`, `LinuxSystemInfo` — one TU per OS in `src/platform/`, **zero ifdefs in domain** |
| `ILogger` | diagnostics | `NullLogger`, `StderrLogger`, injected |
| `IProgressSink` | structured progress | console bar, UI adapter, benchmark collector |

`ExtremeCompressor`'s raw `std::async` calls move onto the injected
`IExecutor`; `ParallelCompressor` becomes a **`ParallelCodecDecorator`**
implementing `ICodec` (fixes #5, #6).

### 6.2 Cross-cutting decorators

```cpp
auto codec = make_unique<ChecksumDecorator>(                     // verifies CRC
                 make_unique<TimingDecorator>(                   // metrics
                     make_unique<ProgressDecorator>(             // events
                         registry.create("optimized", opts))));
```

Each concern is one class, composable in any order, unit-tested alone.
Benchmarks and verify mode are built from the same decorators the CLI uses —
no separate code paths.

---

## 7. Patterns map (where each pattern lands)

| Pattern | Where | Why |
|---|---|---|
| **Strategy** | `ICodec`, `StrategySelector` | interchangeable algorithms |
| **Composite** | `PipelineCodec`, `ParallelCodecDecorator` | pipelines wrap as one codec |
| **Template Method** | `CodecBase` | shared skeleton, guarded hooks |
| **Abstract Factory** | `CodecFactory` per profile (fast/default/max) | family of preconfigured codecs |
| **Factory Method** | `CodecRegistry::create` | objects created without coupling to concrete classes |
| **Registry / static registrar** | `CodecRegistry`, `FrameRegistry` | OCP: add algorithm = add file |
| **Facade** | `CompressionService`, `ArchiveService` | one stable client surface |
| **Adapter** | `BitIO`, `BinaryWriter/Reader`, platform TUs, `LegacyV1Frame` | convert interfaces without changing either side |
| **Decorator** | checksum / timing / progress / parallel wrappers | orthogonal concerns, no subclass explosion |
| **Proxy** | `LazyCodecProxy` (defer heavy ctor), `CachingCodecProxy` (memoize repeated identical input — UI re-preview) | performance: avoid rework |
| **Observer / EventBus** | progress, metrics, telemetry | decoupled progress for N consumers |
| **Chain of Responsibility** | `ContentClassifier` analyzers | each analyzer decides "mine?" in turn |
| **Command** | `CompressFileCommand` et al. | bindable UI ops, batch/queue |
| **Dependency Injection** | constructor injection everywhere + one Composition Root | testability, no global state |
| **Value Objects** | `CompressionOptions`, `CompressResult`, `CodecOptions` | explicit config, no boolean-arg soup |
| **Null Object** | `NullLogger`, `NullProgressSink`, `NullCompressor` | default no-op deps |

Explicitly **not** used (pattern tax, with reason):
- *Singleton state* — only immutable registries; no mutable globals (`crc32Calculator` becomes a constexpr table + free function).
- *Service Locator* — hidden deps break testability; constructor injection only.
- *Visitor over codecs* — new algorithms would edit the visitor; registry beats it.
- *Flyweight/Memento/Mediator/Bridge* — no current problem they solve; revisit if archive *editing* (insert/delete entries) is added (then: Mediator for the edit session).

---

## 8. Error handling & validation

```cpp
class CompressionError : public std::runtime_error { ... };      // base, keeps 1.x catch-sites working
class CorruptDataError : public CompressionError { ... };        // decode-time, checksum
class InvalidFormatError : public CompressionError { ... };      // bad magic/version
class UnsupportedVersionError : public CompressionError { ... }; // future format
class ConfigurationError : public CompressionError { ... };      // bad options
class IoError : public CompressionError { ... };                 // adapter-level, rethrown by facade
```

Rule: domain throws typed errors; adapters catch `IoError`-family and map to
exit codes / UI dialogs. Facade is the only place adapters see domain types —
the UI never catches raw `std::runtime_error` strings.

---

## 9. Reusable client state (what CLI and WinUI share)

This is the payoff for "reusable state": both clients consume the *same*
objects, not copies:

1. **Facade** — identical calls; zero duplication of factory/framing/CRC logic.
2. **Event bus** — same progress model; CLI prints it, UI binds it to a
   progress bar. No `#ifdef` UI code anywhere in the lib.
3. **Commands + Results** — `CompressResult` is exactly the shape a
   ViewModel's `ObservableProperty` wants.
4. **Registry-driven UI** — the WinUI strategy dropdown is generated from
   `CodecRegistry`; a new algorithm appears in CLI *and* UI in one commit.
5. **Golden format tests** — one shared corpus guarantees CLI and UI produce
   byte-identical output (critical for file-association UX).
6. **Optional C API** (`compress.h`) — a later thin wrapper enables FFI
   (Python/Rust/Go bindings) without touching domain.

---

## 10. Testing & quality strategy

| Layer | Approach |
|---|---|
| Unit (domain) | GoogleTest per class; `InlineExecutor` for determinism; injected mocks via constructor |
| Format | **golden vector tests** — frozen byte fixtures per format version; any serialization change is a visible diff |
| Round-trip | property-style: generated corpora (text/random/structured/empty/huge) × every codec |
| Security | **fuzzing** (libFuzzer/AFL) on all decode paths — decompression of untrusted bytes is the attack surface; hostile-header corpus from #2 hardening as seeds |
| Concurrency | TSan runs of parallel encode/decode; stress test with `InlineExecutor` race assertions |
| Performance | benchmark regression gate in CI (existing `BENCHMARKS.md` as baseline) — ratio & throughput thresholds; decorator timings feed it for free |
| Static | clang-tidy + cppcheck + `-Werror` in CI; clang-format enforced (fixes #12 style drift) |
| Sanitizers | ASan/UBSan CI job per OS (this also serves as the "no Linux/macOS pollution" gate) |
| API | a **facade contract test suite** that any future client repo (WinUI, bindings) runs against a pinned lib release |

---

## 11. Migration plan (non-breaking until the facade ships)

| Phase | Content | Breaking? |
|---|---|---|
| **M0 — Foundations** | `ByteView`, `BinaryWriter/Reader`, typed errors, `IExecutor` + platform TU split, `IByteSource/Sink`, style baseline (clang-format, guards) | No — pure additions |
| **M1 — Registries** | `CodecRegistry` (migrate both duplicated factories into it), `FrameRegistry` + `LegacyV1Frame`; unify composite framing; delete ad-hoc markers | No — 1.x bytes stay readable via `LegacyV1Frame` |
| **M2 — Facade** | `CompressionService`/`ArchiveService` + `Options`/`Result` value objects + EventBus + Commands | No — new layer; CLI still works |
| **M3 — Adapters** | Rewrite CLI onto facade (main.cpp → ~100 lines of parsing); benchmark adapter; event sinks | CLI behavior preserved (same args, same output) |
| **M4 — `.cza` archive** | `ArchiveWriter/Reader` per win11-ui spec; block index; per-block checksums; parallel block decompress | No — additive format |
| **M5 — Deep refactor** | `ITransform`/`IEntropyCoder` split; `PipelineCodec` composites; `Extreme` onto `IExecutor`; `ParallelCodecDecorator`; decorator suite | Internal API only — facade unchanged |
| **M6 — Payoff demo** | `AnsCoder` as a *new-file-only* addition via registry (proves OCP); fuzz + golden gates green; release 2.0.0 | Additive |

Versioning policy: 1.x line frozen; 2.0.0 when M2 lands; semver + `SOVERSION`
for the shared-lib option + API changelog per release. The WinUI repo pins to
tagged lib releases (`find_package` or `FetchContent`), per the separate-repos
decision.

---

## 13. Implementation progress

| Phase | Status | Notes |
|---|---|---|
| M0 — Foundations | ✅ done | `core/ByteView.hpp`, `core/Errors.hpp` (typed hierarchy), `core/BinaryIO.hpp` (FourCC, BinaryReader/Writer, LE + bounds-checked), `core/Executor.hpp` (IExecutor + InlineExecutor; ThreadPool now implements IExecutor with `execute()`/`shutdown()`), `core/ByteSource.hpp` + `src/core/ByteSource.cpp` (Memory/File sources & sinks, IoError on I/O failure), platform TU split (`src/platform/SystemInfo_{windows,darwin,linux}.cpp`, zero ifdefs, selected per-OS in CMake), `.clang-format`. 41 new tests in `tests/core/`, full suite green, CLI round-trip verified. |
| M1 — Registries | ✅ done | `codec/CodecRegistry` (self-registering central factory, keyed by `format::AlgorithmID`, `create`/`contains`/`idOf`/`nameOf`/`all`; ConfigurationError on unknown) — **merged three duplicated factories** (`app/main.cpp`, `src/ParallelCompressor.cpp`, `app/benchmark.cpp`) into it. `format/FrameRegistry` + `LegacyV1FrameCodec` (header encode/decode via BinaryIO, byte-identical to the 1.x layout — golden fixture test, typed errors, chunk-count hardening hoisted from ParallelCompressor); `FileFormat::serializeHeader/deserializeHeader` now delegate to the registry (still inline-free, out-of-line in `src/format/FileFormat.cpp`). Composite sub-framing (Optimized/Ultra/Extreme markers) deliberately **deferred to M5** — changing it would break files written by released 1.x; M5 will register them as versioned legacy sub-frames. 21 new tests; full suite 174/174 green. |
| M2 — Facade | ✅ done | `CompressionService` (`include/compression/app/CompressionService.hpp`, `src/app/CompressionService.cpp`) — the single stable client surface: `compressFile`/`decompressFile`/`compress`/`decompress` returning `CompressResult`/`ExtractResult` value objects (in/out bytes, ratio, CRC, elapsed); `CompressionOptions` value object (codec id + threads, 0 = auto). Codec creation goes through `CodecRegistry`, framing through `ParallelCompressor` (byte-identical single-thread path), event bus injected via constructor (DI). `app::EventBus` (Observer): thread-safe weak-subscriber bus + `CompressionEvent` + `IEventListener`; service emits `OperationStarted`/`CodecSelected`/`OperationCompleted` (per-block events arrive with M5 decorators). `app::ICommand` + `CompressFileCommand`/`DecompressFileCommand` + `CommandContext` (bindable, expose last `result()`). **Scope adjustment:** `ArchiveService` deferred to M4 with the `.cza` archive domain it depends on. 17 new tests (service round trips incl. multithread/auto-threads/corrupt-rejection, event bus incl. thread-safety + expired-subscriber drop, command round trips); full suite 191/191 green. |
| M3 — Adapters | ✅ done | CLI rewritten onto the facade: `app/main.cpp` shrank from 274 lines of factory/framing/CRC/read-write logic to ~180 lines of pure parsing + dispatch + rendering. Same argument grammar, exit codes (1 on error, 0 on success) and error messages; strategy list in `--help`/usage is now **generated from CodecRegistry** (new algorithm appears in CLI automatically); `ProgressPrinter` adapter subscribes to the `EventBus` (renders progress from the facade). Compressed output is byte-identical (single-thread path via `ParallelCompressor` == old framing). Console chatter replaced by a clean `CompressResult`/`ExtractResult` summary (input/output/ratio/crc/elapsed). Benchmark already decoupled in M1 (registry) — no further change needed this phase. 191/191 green; CLI round trips verified for default/bwt, `--threads N`, `--threads=N`, plus all error paths. |
| M4 — `.cza` archive | ✅ done | Seekable block-indexed container per the win11-ui spec. `archive/Archive.hpp` (value objects: `ArchiveEntry`, `ArchiveListing`, `ArchiveBuildOptions`, `BlockVerifyResult`; magic `CZA1`, version 1). `ArchiveWriter` (`include/compression/archive/ArchiveWriter.hpp`): packs entries into fixed-size blocks (default 1 MiB), each block compressed independently via `CodecRegistry`, tables written after the payloads (ZIP-style central-directory layout); container header (magic/version/blockSize/codec) at offset 0; per-entry + per-block CRC32. `ArchiveReader`: opens by seeking to the tail footer (no decompression for listing), validates tables/offsets with typed errors + hostile-input caps, `extract(id)` decompresses **only the spanned blocks in parallel** (ThreadPool, sequential pre-read for thread safety), `verify()` per-block checksums. `app/ArchiveService` facade: `create`/`list`/`extract`/`verify` over file IO with a **path-traversal guard** (rejects absolute paths and `..`). 22 new tests (round trips, multi-block spans, block sharing, empty entry/archive, metadata, verify/corruption, footer-magic rejection, traversal). Full suite 213/213 green. |
| M5 — Deep refactor | ✅ done | **Pipeline abstraction** (`codec/pipeline/`): `ITransform`/`IEntropyCoder` interfaces (stable stage ids), `PipelineCodec` (Composite + Template Method) emitting a self-describing `PLIN` frame; new raw `RleTransform` + `BwtTransformAdapter` + `ArithmeticCoderAdapter` stages. **OCP proof**: a brand-new codec `bwt2` (BWT → RLE → Arithmetic) registered in `CodecRegistry` under new `AlgorithmID::PIPELINE_BWT_COMPRESSOR` — it appeared in the CLI usage and works with zero code changes outside registration. **Decorators** (`codec/decorator/`): `TimedCodec`, `ChecksummedCodec` (CRC trailer + verify), `ProgressCodec`. **`ParallelCodecDecorator`** (`codec/`) implements `ICompressor` on the injected `IExecutor` port (M0 payoff) with the same 1.x framing + hostile-header hardening, and **replaces/retires `ParallelCompressor`** (LSP violation removed, one parallel impl left). `CompressionService` now uses the decorator — the CLI renders **real per-chunk progress** (0→25→50→75→100%). Legacy Optimized/Ultra/Extreme sub-framing stays untouched for backward compatibility (files from 1.x remain readable); rewriting them onto pipelines is noted as future work. 21 new tests (RLE/pipeline round trips + frame corruption + unknown-stage, decorator composition + checksum rejection, parallel single/multi-chunk/auto/corruption/invalid-chunk-count, bwt2 registration). Full suite 234/234 green. |
| M6 — Payoff demo | ✅ done | `AnsCoder` (**rANS entropy coder**) added as a *new-file-only* codec via `CodecRegistry` under `AlgorithmID::ANS` — appears in the CLI automatically, works under `ParallelCodecDecorator`/`CompressionService`/CLI with zero changes outside registration (final OCP proof). **Golden-format gates** (`tests/format/`): byte-for-byte frozen fixtures for the ANS payload, the BWT→RLE pipeline payload, and `[CPRO]` ANS + optimized frames (any format drift fails the build). **Fuzz gates** (`tests/security/FuzzGatesTest.cpp`): seeded deterministic mutation fuzz over every registry codec + parallel decorator framing — no crash, no hang, only `std::runtime_error`-family exceptions. The gates exposed a pre-existing 1.x robustness hole (`ArithmeticCompressor::decompress` decoded up to ~4 billion symbols from a hostile 16-byte header) — fixed with a measured two-byte-past-end slack limit so decode work is bounded by input size. `AnsCoder` itself fixed two bugs the tests caught (uint16 frequency overflow → infinite renorm loop, and decode-order correction). 27 format+security tests; full suite 261/261 green; CLI `ans` round trip verified; version bumped 1.6.1 → 2.0.0. |

---

## 12. Open decisions (need your call)

1. **C++ standard**: stay C++17 (`gsl::span` shim) or move to C++20
   (`std::span`, concepts for `ByteSink`)? Recommendation: C++20 if all client
   toolchains allow it.
2. **Allocation policy**: ByteSink-into-pooled buffers (perf) vs. simple
   `std::vector` sinks (simplicity) — hybrid: hot path pooled, adapters simple.
3. **Shared library**: static-only (today) vs. offer shared build with
   visibility macros? Recommendation: static first; shared with ABI policy at 2.0.
4. **Legacy CLI args**: keep 1.x CLI byte-compatible (recommended) or accept a
   flag-style break (`--codec` etc.)?
5. **Event bus threading**: lock-free per-thread ring buffers vs. mutexed
   vector of weak listeners? Recommendation: mutexed weak-listener for v1
   (correctness), profile before optimizing.
