# Repository Separation Proposal: Engine vs. UI Applications

Status: **Proposed** (awaiting approval)
Author: opencode, on behalf of the project owner
Date: 2026-08-23

---

## 0. Decision in one paragraph

This repository (`compressor`) terminates as the **engine repository**: the
CompressionLib C++ library, the reference CLI, the `.cza` archive engine, and
all quality gates. The macOS and Windows UIs are built in **two new separate
repositories** (`compressor-macos`, `compressor-windows`) that consume the
engine as a versioned, pinned dependency — never as code in this tree. The
engine repo keeps its own version line (1.x); the `v2.0.0` tag stays reserved
for the coordinated UI-deployment milestone.

---

## 1. Why separate repositories (summary)

- **Toolchain reality**: macOS UI (Xcode/Swift or Qt) and Windows UI
  (Visual Studio/WinUI/C# or Qt) do not share the engine's CMake/C++17 build;
  a library must be compiled on the same platform/toolchain as its consumer
  (Philips PACS practice; NWCpp component-development consensus).
- **Release cadence**: the engine moves slowly (ABI, byte formats, gates);
  UIs move fast (features, app-store cycles). One repo forces one cadence.
- **Ownership of risk**: signing/notarization, sandboxing, app packaging, and
  interop wrappers are UI concerns; they must not pollute the engine's gates.
- **The `v2.0.0` convention**: the user reserves `v2.0.0` for full UI
  deployment. Separate version lines make this natural: the engine tags
  `v1.x` now; `v2.0.0` is cut on the engine **at the moment the UIs deploy**,
  pinned by both app repos.

## 2. Clean termination of this repository as the engine

### 2.1 Scope contract (this repo owns)

| Owned here | Not owned here |
|---|---|
| CompressionLib (public API: `core/`, `format/`, `codec/`, `archive/`, `events/`, `app/`) | UI code, view models, windowing |
| CLI `compress_app` (reference adapter + smoke tool) | File dialogs, drag & drop, platform integration |
| `.cza` archive engine, facades, registry | Sandboxing, entitlements, signing/notarization |
| Golden-format gates, fuzz gates, unit tests | Interop wrappers (Swift/C#/C++/CLI) |
| CMake package (`find_package(CompressionLib)`) | App icons, app-store packaging |
| Benchmarks, docs, Doxygen, release pipeline | Anything toolchain-specific to a UI |

### 2.2 Termination mechanics

1. **README**: add a "Scope" section (engine only) with links to the two UI
   repos once they exist, and a "consuming the engine" pointer to the
   porting guide (§4).
2. **Issue labels**: scope labels (`engine`, `ui-macos`, `ui-windows`) so
   UI-tracked issues are not filed here.
3. **Engine release checklist** (the "Definition of Done" for every tag):
   - [ ] Full suite green: `ctest` 261/261 (this number updates with the suite)
   - [ ] Golden gates green (byte-format freeze)
   - [ ] Fuzz gates green (no crash/hang)
   - [ ] CLI round trips verified (all strategies + `ans`)
   - [ ] `cmake --install` smoke: consumer app builds via `find_package`
   - [ ] 3-OS CI green (build-test + sanitizers)
   - [ ] Tag pushed; `release.yml` produced packages + checksums
   - [ ] CHANGELOG entry exists for the version

### 2.3 Optional: repository rename

`compressor` → `compressionlib` would align the repo name with the artifact.
Recommended: **keep `compressor`** — no external consumers exist, so the
benefit is cosmetic; renaming burns links and history-churn for zero
functional gain. Decide once; never again after the UIs pin it.

## 3. Proper versioning

### 3.1 Engine version line (correction)

The engine currently carries `project VERSION 2.0.0` (CMakeLists) and a
CHANGELOG `2.0.0` entry. Under the agreed convention this is wrong: `v2.0.0`
is the UI-deployment milestone, not the engine refactor.

**Action**: roll the engine version line back to **1.7.0**:
- `CMakeLists.txt`: `project(CompressionLib VERSION 1.7.0)`
- `CHANGELOG.md`: retitle the top entry `[1.7.0]` (the M0–M6 refactor, ans
  codec, gates, robustness fixes, API hygiene all ship as 1.7.0)
- Keep the `v2.0.0` convention documented in `docs/VERSIONING.md` (below)

### 3.2 Versioning policy (new `docs/VERSIONING.md`)

- **Semver** on the engine only. `MAJOR.MINOR.PATCH`, tags `vX.Y.Z`.
- **Same-MAJOR guarantees**: file formats (`.cpro`, `PLIN`, `.cza`) stay
  readable; the installed public API stays source-compatible; byte layouts
  stay frozen (enforced by the golden gates).
- **MAJOR bump** = format/API break. **MINOR** = new codec/feature (like
  `ans` in 1.7.0). **PATCH** = fixes only.
- **The `v2.0.0` milestone**: the engine does **not** tag `2.0.0` until the
  UI-deployment milestone. At that point, in one coordinated release:
  engine tags `v2.0.0`; `compressor-macos` tags `v1.0.0`; `compressor-windows`
  tags `v1.0.0`. All three CHANGELOGs reference each other.
- **Pin policy for UI repos**: `FetchContent` pinned to an exact tag (never a
  branch), or the installed package at an exact version. Pins move only via
  an intentional bump reviewed against the engine CHANGELOG.

### 3.3 Release mechanics (already in place)

`release.yml` builds Ubuntu/Windows/macOS on tag push, runs `ctest` (do not
tag a broken release), produces `compressor-<OS>.tar.gz` + SHA-256 sums, and
publishes them to the GitHub release. No change needed; the version line
correction feeds it `1.7.0`.

## 4. Clean porting (no corruption at the boundary)

"Porting" = the UIs consuming the engine. Data-integrity risks live exactly
at the interop boundary, so the contract below is the porting checklist.

### 4.1 The porting contract (UI repos MUST)

1. **Byte-identical round trips**: any data the UI compresses must decompress
   byte-identical. Reference vectors come from the engine: the golden
   fixtures (`tests/format/GoldenFixtures.inc`) and the CLI. A UI interop
   test must run the same payloads through its bindings and compare against
   the CLI output bytes.
2. **Surface CRC verification**: the engine verifies CRC32 on decompress
   (`ExtractResult::verified`, `ChecksummedCodec`, archive `verify()`). UIs
   must surface failures to the user and never silently ignore them.
3. **Typed errors, loud failures**: engine throws typed errors
   (`InvalidFormatError`, `CorruptDataError`, ...). UIs must map them to
   user-visible errors, never swallow them.
4. **Version skew handling**: streams carry format/version markers (`PLIN`
   version byte, `CPRO` magic/version, `CZA1` magic). A stream from a newer
   engine must fail loudly, not be mis-decoded.
5. **Interop hygiene**: length fields as `uint64_t`/`size_t` end-to-end
   (no truncation to 32-bit), byte-buffer ownership explicit, no signed/
   unsigned mismatches in marshaling. The engine's `core::ByteView` +
   `core::IByteSink` are the boundary types.

### 4.2 Porting starter kit (add to the engine repo)

1. **`examples/consumer/`** — a minimal CMake project, two modes:
   - `FetchContent` pinned to a tag (recommended for UI repos), and
   - `find_package(CompressionLib)` (installed-artifact mode).
   It round-trips data through `CompressionService` and creates/lists/verifies
   a `.cza` archive — the exact smoke test proven earlier.
2. **`docs/PORTING.md`** — the contract in §4.1 as a checklist, plus the
   FetchContent snippet, plus "how to bump the pin" and "what to do when a
   stream is rejected".
3. **Engine release notes** — each engine tag's CHANGELOG states whether the
   pin can be bumped freely (PATCH/MINOR) or requires testing (MAJOR).

### 4.3 Anti-corruption invariants already enforced by the engine

- Golden-format gates freeze byte layouts (any drift fails the build).
- Fuzz gates guarantee no crash/hang on hostile input.
- Hostile-header caps bound decode work (the 1.x DoS hole is closed).
- Per-block CRC + `verify()` in the archive engine.
- `release.yml` computes checksums for every distributed artifact.

## 5. Implementation plan (ordered)

| # | Step | Repo | Output |
|---|---|---|---|
| 1 | Version correction to 1.7.0 (CMakeLists + CHANGELOG) | engine | commit + tag `v1.7.0` → 3-OS release assets |
| 2 | Add `examples/consumer/` (FetchContent + find_package smoke) | engine | merged PR |
| 3 | Add `docs/VERSIONING.md` + `docs/PORTING.md`; README "Scope" section | engine | merged PR |
| 4 | Release checklist appended to `docs/architecture-proposal-2.0.md` or its own `docs/RELEASE.md` | engine | merged PR |
| 5 | Create `compressor-macos` and `compressor-windows` repos (private), scaffold from `examples/consumer` | new repos | two repos, CI green |
| 6 | UI repos pin `v1.7.0`; porting contract tests added per UI | new repos | interop tests green |
| 7 | (Later) coordinated `v2.0.0` / `v1.0.0` / `v1.0.0` release | all three | UI deployment |

## 6. Decision points for the owner

1. **Version correction**: OK to roll the engine back to `1.7.0` (with the
   M0–M6 work as the 1.7.0 release), keeping `v2.0.0` reserved? (Recommended:
   yes.)
2. **Repo rename**: keep `compressor`? (Recommended: keep.)
3. **UI stacks**: SwiftUI+interop and WinUI+interop (or Qt for both)? The
   engine contract is stack-agnostic; the UI repos decide.
4. **New repos public or private?** (Recommended: private until the v2.0.0
   milestone.)
5. **Approve steps 1–4 as one PR batch** or as separate PRs?