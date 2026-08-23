# Versioning Policy

## Semver on the engine only

`MAJOR.MINOR.PATCH` (tags `vX.Y.Z`), applied to this repository.

- **MAJOR** — breaking change: byte formats (`.cpro`, `PLIN`, `.cza`) stop
  being readable, or the installed public API breaks source/ABI compatibility.
- **MINOR** — new user-facing feature (new codec, new archive capability),
  backward compatible.
- **PATCH** — fixes, hardening, internal refactoring. No new features.

## The `v2.0.0` convention

`v2.0.0` is **reserved** for the full UI deployment milestone. Until then the
engine releases on the 1.x line (patch-level bumps are the norm while the
engine hardens). At the deployment milestone, in one coordinated release:

- `compressionlib` tags `v2.0.0`
- `compressor-macos` tags `v1.0.0`
- `compressor-windows` tags `v1.0.0`

Each CHANGELOG references the others.

## Guarantees within a MAJOR line

- Files written by any engine version in the same MAJOR line decompress
  correctly on any other (enforced by the golden-format gates).
- The installed public surface (`core/`, `format/`, `codec/`, `archive/`,
  `events/`, `app/`) stays source-compatible.
- Streams from a newer MAJOR are rejected loudly, never mis-decoded.

## Pin policy (consumers)

UI repositories pin the engine by **exact tag** (FetchContent `GIT_TAG`
`vX.Y.Z`, never a branch). Bumps are reviewed against the CHANGELOG; PATCH
and MINOR bumps require a round-trip test; MAJOR bumps require a stream
compatibility review.

## Release mechanics

Pushing a `vX.Y.Z` tag triggers `.github/workflows/release.yml`: build +
test on Ubuntu/Windows/macOS (do not tag a broken release), package
`compressor-<OS>.tar.gz` + SHA-256 sums, publish to the GitHub release.

Every tag must pass the release checklist in
`docs/repo-separation-proposal.md` §2.2.