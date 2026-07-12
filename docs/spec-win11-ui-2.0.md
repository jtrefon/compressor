# Compressor 2.0 — Windows 11 Native UI & Seekable Archive

Status: Draft (research-complete, pre-implementation)
Scope: Major version `2.0.0` (the `1.x` CLI line is frozen at `1.6.0`).
Language policy: **Native C++ only.** UI is WinUI 3 via **C++/WinRT**; it links
`CompressionLib` directly. No C#, no managed code, no separate/bridged repo.

---

## 1. Goals & Non-Goals

### Goals
- A modern **Windows 11 native desktop app** (WinUI 3 / Windows App SDK) for the
  existing `CompressionLib`.
- A new **seekable, block-indexed archive container (`.cza`)** so archives can be
  *opened and listed without decompressing the whole payload*, and individual
  files can be *extracted by seeking to their blocks* — the headline performance
  feature.
- **Shell integration**: right-click compress/extract, `.cza` file association.
- **Fluent / Win11 UX**: Mica backdrop, NavigationView, DataGrid, CommandBar,
  progress + toast notifications, light/dark.

### Non-Goals for 2.0.0 (deferred)
- Encryption / password protection
- Archive splitting / multi-volume
- Cloud / remote sources
- Cross-platform builds

These are explicitly out of scope for the first major release.

---

## 2. Technology Stack (decided)

| Concern        | Choice                                              | Notes |
|----------------|-----------------------------------------------------|-------|
| UI framework   | **WinUI 3 (Windows App SDK), C++/WinRT**            | Microsoft's stated preferred framework (Build 2026). Native, no managed interop on hot path. |
| App language   | **C++17/20** (no C#)                                | Links `CompressionLib` (native C++) directly. |
| Installer     | **WiX Toolset → MSI** (primary)                     | Free, XML-defined, CI-friendly, supports shell extensions + file associations. |
| Alt installer  | **MSIX** (secondary, Store/modern)                  | Optional later; needs code-signing cert. |
| Archive format| **New `.cza` seekable, block-indexed container**    | Industry-aligned with Zstandard seekable format + ZIP central-directory ideas. |
| Versioning    | Unified **2.0.0** (library + app share the version) | `CMakeLists.txt` and the UI project both at `2.0.0`. |

### Build topology
- The existing CMake build (`src/`, `app/`, `tests/`) is unchanged and remains the
  CI gate (`build-test.yml`).
- The WinUI 3 app lives in a **separate `ui/` directory** as an MSBuild project
  and is **not** added to the CMake tree (the Windows App SDK is not available in
  the current CI/lib build). A dedicated CI job builds it once the SDK is present.

---

## 3. Archive Container Format — `.cza` (seekable, block-indexed)

### 3.1 Rationale & industry alignment
- **Zstandard seekable format** (facebook/zstd `contrib/seekable_format`): data is
  split into *independently compressed frames* with a trailing **seek table**
  enabling random access by decompressed offset and parallel decompression.
- **ZIP central directory**: metadata (file list + offsets) lives at the end, so
  the listing is read without scanning the whole archive.
- `.cza` combines both ideas: a **file table** (what's inside) + a **block table**
  (where each compressed block lives) + independently compressed blocks.

### 3.2 On-disk layout
```
+--------------------------------------------------------------+
| Magic "CZA1"            (4 bytes)                            |
| Header: version, flags, blockSize, default compressor ID    |
| File Table:                                               |
|   N entries: name, rawSize, mtime, attrs,                  |
|              blockRanges[], entryChecksum                   |
| Block Table:                                              |
|   M entries: compressorID, compOffset, compLen,            |
|              rawLen, blockChecksum                         |
| Blocks:                                                   |
|   M independently-compressed blocks (Huffman / LZ77 /     |
|   BWT / Arithmetic / Optimized)                            |
| Footer:                                                   |
|   offset+size of File Table, offset+size of Block Table,   |
|   magic "CZA1" (mirror) for robust tail read              |
+--------------------------------------------------------------+
```
- **Open**: read tail → footer → load File + Block tables (tiny vs. payload).
- **List**: purely from the File Table — **no decompression**.
- **Extract one file**: map its `blockRanges` → seek to each block's
  `compOffset`, decompress only those blocks, reassemble. Other blocks untouched.
- **Verify**: per-block `blockChecksum` (CRC32 or xxHash) — corrupt block detected
  without reading the rest.
- **Parallel decompress**: blocks are independent → one task per block.

### 3.3 Compression strategy
- Default: split input into fixed-size blocks (e.g. 1–16 MiB, configurable).
- Each block compressed with a selectable algorithm (default: `Optimized`, which
  already picks best of the suite per block/region).
- Optional **solid grouping**: group related small files into one block with a
  shared dictionary for better ratio (tradeoff: no per-file seek within the group).
- Block size is a tunable that balances ratio vs. partial-extract granularity.

### 3.4 Backward compatibility
- 1.x single-blob output (CLI `--format raw`) remains readable by a thin
  `LegacyBlobReader` shim; a one-time "import / re-save as .cza" helper is
  provided in the UI.

---

## 4. Application Architecture

```
XAML (MainWindow, dialogs)
   │  (C++/WinRT data binding)
ViewModels (C++/WinRT, INotifyPropertyChanged)
   │
Archive API  (new native C++ class, owns .cza read/write)
   │  uses
CompressionLib (existing compressors, per block)
```

- **Archive API** (`src/archive/` or `ui/` internal lib): a native C++ `Archive`
  class wrapping `.cza` read/write, delegating per-block work to `CompressionLib`.
  Pure C++, unit-testable without UI.
- **Async**: long ops run via `winrt::resume_background` / `IAsyncAction`,
  reporting progress through `IProgress<T>` so the UI never blocks.
- **No managed boundary**: the UI calls the Archive API directly (both native C++).

---

## 5. Features & UX Journeys

### 5.1 Core journeys (v2.0.0)
1. **Open archive** — pick/launch `.cza` → File Table loads → file list shows
   instantly (name, size, ratio, modified). *Zero decompression.*
2. **Create archive** — add files/folders → choose algorithm + level + block size
   + solid/seekable toggle → progress + toast on completion.
3. **Extract** — *all*, *selected files*, or *drag-drop* to Explorer. Single-file
   extract seeks only its blocks.
4. **Preview / Peek** — open text/small files in an inline viewer **without
   extracting** (seek + decompress just that entry).
5. **Search / filter** — filter the in-memory file list; (later) content search.
6. **Verify** — per-block checksum scan; report corrupt blocks.
7. **Settings** — default algorithm/level/block size, thread count, theme (Mica/
   light/dark), shell-integration toggle.
8. **Shell integration** — right-click → "Compress to .cza" / "Extract here";
   double-click `.cza` opens the app.

### 5.2 Win11 Fluent UX primitives
- `MicaController` backdrop; `NavigationView` (Home / Archive / Settings).
- `DataGrid`-style `ListView`/`GridView` for the file list with column sort.
- `CommandBar` for extract/create/verify; `ProgressRing` + `InfoBar` for status;
  `ToastNotification` on completion.

---

## 6. Installer / Packaging

### 6.1 WiX MSI (primary)
- Install `CompressorUI.exe` + WinUI runtime dependencies.
- **File association** `.cza` → app.
- **Shell extension** (context menu) registered via registry (compress/extract).
- Start Menu shortcut; optional PATH entry.
- XML source (`installer/CompressorUI.wxs`) — version-controllable, CI-buildable.

### 6.2 MSIX (secondary, optional)
- For Microsoft Store / modern sideload; needs a code-signing certificate
  (~$200–300/yr). `broadFileSystemAccess` capability for unrestricted file ops.
- Not required for the first 2.0.0 drop.

---

## 7. Testing

- **Archive API unit tests** (C++/GoogleTest): round-trip, partial (single-file)
  extraction equality, corrupt-block detection, large/small/empty inputs, mixed
  file sets. Added to the existing `tests/` CMake target.
- **Lib regression**: existing `ctest` suite must stay green.
- **UI**: manual smoke + (later) WinAppDriver-driven smoke for open/list/extract.
- **CI**: keep `build-test.yml` for lib; add a separate `ui-build.yml` that builds
  the WinUI project once the Windows App SDK is available in the runner image.

---

## 8. Milestones

| Milestone | Deliverable | Depends on |
|-----------|-------------|------------|
| **M1** | `.cza` format + Archive API (C++) + unit tests | lib (done) |
| **M2** | WinUI 3 shell: Open → List → Extract wired to Archive API | M1, WinUI SDK |
| **M3** | Create archive + Settings + progress/toasts | M2 |
| **M4** | Shell integration + WiX MSI installer | M3 |
| **M5** | Polish, optional MSIX, **release 2.0.0** | M4 |

---

## 9. Open Questions / Risks
- **Dev environment**: Windows App SDK + VS 2022 desktop C++ workload must be
  installed to build the UI (not present in the current sandbox).
- **Extension / magic bytes** for `.cza` to be finalized in M1.
- **Block-size default** to be tuned via benchmark (ratio vs. partial-extract
  granularity).
- **Shell extension** implementation choice (registry-based static handler vs.
  IExplorerCommand) decided in M4.

---

## 10. Branch & Version
- Branch: `feature/win11-ui` (off `main` @ `1.6.0`), bumped to `2.0.0`.
- Library `CMakeLists.txt` `VERSION` → `2.0.0`; UI project version → `2.0.0`.
