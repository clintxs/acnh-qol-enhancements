# Internal feature interfaces: stage 0 baseline

Recorded on 2026-10-13 for the authorized stage 0 of the QoL Enhancements
internal architecture refactor. Later stages, integration and release are separate
work. No gameplay, implementation, configuration or release-version changes are
part of this baseline.

## Source preservation

- Starting branch: `main`.
- Starting commit: `35ec0c865065dda9f4b09660434370d90ac57373`
  (`Restore the original blue Island Inverted Shopping Card palette`).
- Initial tracked/untracked status: clean (`git status --short` had no entries).
  Ignored local build outputs, assets and releases existed and were preserved.
- Initial worktrees: only this checkout; initial branches: only `main`.
- Implementation branch: `refactor/internal-feature-interfaces`, created directly
  from the starting commit. `main` remains at that commit.
- Release version at baseline: `0.4.32`, targeting ACNH `3.0.3` on Eden.

The plan used was the user-supplied `qol-api-refactor-plan.md` at
`/Users/clint/.codex/visualizations/2026/10/13/01a13407-be26-7132-9581-3dd313c361ea/qol-api-refactor-plan.md`.
The user's explicit request authorized stage 0; the document alone did not
authorize implementation. Repository `AGENTS.md` was read before acting.

## Local comparison artifacts

Before the clean build, 151 files were copied with their relative paths into
`dist/refactor-baseline-35ec0c8/`: existing `project.elf`, `project.nso`,
`project.npdm`, and files under `build/`, `deploy/`, `out/` and `modules/`.
This preserves the previous binary, generated outputs and module resources
independently of the rebuild. It is a local ignored snapshot, not a Git artifact.

- `snapshot-sha256.json`: hashes of all 151 saved files.
- `existing-dist-sha256.json`: hashes of all 2,816 pre-existing files in `dist/`;
  existing releases remain in their original locations.
- `fresh-build/`: an independent copy of the newly built ELF, NSO and NPDM.
- `fresh-build-sha256.json`: verified hashes of those fresh outputs.
- `build.log`, `check.log`, `check-unrestricted.log`, `toolchain.log`: local
  build, initial test attempt, unrestricted test run and compiler evidence.

The saved snapshot and pre-existing dist files were verified against their
manifests after building, with zero mismatches. No packaging command was run,
release archive replaced, or installed Eden mod updated. These local artifacts
are not backed up by the branch: retain `dist/` through acceptance of the refactor.

## Build and test prerequisites

The inspected entry points are `sh tools/build.sh` and `python3 tools/check.py`.
The build performs `make clean` followed by `make -j4` in a network-disabled
Docker container. Normal mode sets all three diagnostic switches to zero:
`CAS_VERBOSE_DIALOGUE_TRACE`, `CAS_VERBOSE_HURRY_TRACE` and
`CAS_AUTO_VAULT_DIAGNOSTIC`.

| Component | Recorded baseline |
| --- | --- |
| Docker client | 27.4.0, build bde2b89 |
| Container | `devkitpro/devkita64:20260219@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282` |
| Target compiler | devkitA64 GCC/G++ 15.2.0 |
| Target mode | Module / `subsdk6`, ARMv8-A, Cortex-A57, PIC, `-O3 -g`, GNU C++23, no RTTI/exceptions |
| Host compiler | Apple clang 21.0.0 (`clang-2100.3.34.2`), arm64 macOS; host tests use C++20 and warnings as errors |
| Host Python | 3.14.7 (also supplies `compression.zstd`) |
| ARM harness dependencies | Unicorn 2.1.4, Capstone 5.0.9, Keystone Engine 0.9.2 |

The harness dependencies are available in `../Grab 5/working/python`; the check
runner sets child-process `PYTHONPATH` to that directory. They are not installed
in the default Python environment. Local original game code
`../Grab 5/working/main.raw` and original assets `../vanilla-3.0.3/Layout` are
present. ARM tests also need the freshly built `project.elf`, module resources
and those local game assets. Do not distribute original game assets in source.

## Results and limits

- Normal clean build: passed, exit 0.
- Initial restricted check attempt: all 12 host suites passed; the first ARM
  suite (`logging_arm_test.py`) terminated with SIGILL. The Docker socket was
  also unavailable under the sandbox. Both operations were retried with approved
  unrestricted execution; no dependency installation or source fix was needed.
- Full unrestricted regression run against fresh outputs: passed, exit 0. All
  12 host suites and all 39 regression scripts across Core, Shopping, Dialogue,
  Crafting, Controls, Pockets, Fitting and Catalog completed. The first ARM
  suite passes outside the sandbox, consistent with an execution-environment
  restriction rather than a gameplay or source regression. No baseline code
  correction was made.

Offline ARM tests model native services, hooks and rendering behavior; they do
not establish live engine effects or GPU visual correctness. No new live Eden
test was performed in stage 0. Existing live-test limits in `VALIDATION.md`
continue to apply; the saved pre-build binary is available for live comparison.

## Stage 0 exit

The implementation branch is isolated, the starting source is recoverable from
`main`, baseline build and regression checks passed, and both the prior binary
and fresh baseline outputs are preserved locally. Stage 0 is complete. Continue
future authorized refactor work on `refactor/internal-feature-interfaces`.
