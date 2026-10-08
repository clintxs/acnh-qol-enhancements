# Unified QoL validation — 1.0.0 integration test

Validated locally on October 8, 2026 against the supplied original ACNH 3.0.3
main (build FF1D1C05670DB6021C85B624A710B963). The unified build has **not yet
been tested in Eden**. No running game or save files were accessed by these tests.

## Build and loader

- Pinned devkitPro Docker build, networking disabled: passed with `-Wall -Werror`.
- Core deploys only `subsdk6` plus the existing minimally adjusted `main.npdm`.
  No package contains subsdk9; avoiding the slot does not prove future
  Jubbsticks Additions hook/NPDM compatibility.
- All 32 combinations of five feature packages at two ASLR bases passed.
  `exl_main` installs only the content-mount continuation hook and performs no
  configuration reads. Activation executes the real compiled registry/parser and
  installers; a second activation performs no additional reads or installations.
- Full suite installs 44 disjoint modeled hook ranges (Core 1, Catalog 16,
  Pockets 16, Fitting Room 11), the private fitting input vtable entry and eleven
  runtime instruction records (ten dialogue, one Amiibo).
- Per-feature absent, disabled, invalid, missing-version, oversized, below-minimum
  and newer-version configurations were checked. Newer valid packages warn and
  run. Below-minimum packages leave feature code untouched. Open/read/size errors
  close handles where appropriate and disable requested features. Occupied
  startup/patch locations fail their instruction checks; dialogue validation
  rejects an altered helper before modifying any other dialogue record.
- Final compiled dialogue writes exactly match the standalone hold-to-boost 0.3.0
  payload, captured independently in `tests/baseline_hashes.json`.

The loader harness models FS services, code-hook allocation, writable aliases
and cache services. It does not execute Eden's loader or establish that the
content-mount continuation is safe in a live process. Exlaunch's existing inline
adapter is selected with full SIMD-register preservation for discovery.

## Existing behavior against the unified ELF

The inherited tests were adjusted for the new source/translation paths and
combined ELF. The Pockets fixture's synthetic adapters were moved beyond the
larger module's BSS; this was a test-memory placement change, not a gameplay fix.

- 192 version comparisons plus valid/invalid config boundaries passed.
- 392 stack-menu cases and 864 storage-policy cases passed.
- All fourteen caption locales, fallback handling, UTF-8, capacity and atomic
  parser rejection passed.
- 372,075 fitting-bag capacity/policy cases passed.
- 630 compiled Pockets ARM cases and 190 storage-hint cases passed at two bases:
  native Split, menu dispatch, translated captions at the new path, withdrawal,
  Y-to-storage lifecycle, native footer allocation and caption routing.
- Fitting Room native checkout tests passed at two bases: exact variants,
  outfit/bag overlap, wear/send choices, storage/pocket/mixed delivery, tickets,
  purchase records, metrics, repeat guards and preflight rejection. Engine/save
  APIs are simulated. Native scale/dirty-transform, alpha/removal and cursor-tap
  feedback checks also passed.
- 1,926 dialogue/ground catalog cases, 44 UI-result source cases, 148 variation
  label cases, 32 Miles name-binding cases and 54 counted-name cases passed.
  Real fence metadata/name types were exercised. Imported catalog fixtures also
  exercise their prerequisite native query, naming and badge checks.
- 26,077 fitting catalog-star cases passed, including all 5,207 clothing IDs,
  native selection frames for eight kinds, real USen names and two load bases.

These tests use original local game instructions/resources and modeled engine
boundaries. They do not confirm rendering, physical input, audio, real purchases,
real delivery/catalog saves, or complete emulator behavior.

## Packaging

- Exactly six mod folders: Core, Catalog, Pockets, Faster Dialogue, Fitting Room,
  Amiibo. Five versioned INIs; only Core has ExeFS files. Optional modules are
  absent from Core's own RomFS, so removing/disabling one removes its marker.
- Six layout payloads match the current standalone binary ZIPs byte for byte,
  checked against independently captured SHA-256 values.
- Catalog badge builders verify original resource hashes, archive changes and
  hidden initial badge panes. Fitting builders preserve other archive members.
- Binary ZIP contents and SHA-256 manifest are checked. Matching source excludes
  game resources, binaries, build outputs, Git data and caches. Packaging refuses
  to overwrite an existing release.

## Reproduce

From the workspace root, build with `sh qol-enhancements/tools/build.sh`.
Then run `python3 qol-enhancements/tools/check.py` and
`python3 qol-enhancements/tests/package_test.py`. The ARM emulator requires JIT
access outside the Codex filesystem sandbox on this Mac. Its Python packages are
currently at `Grab 5/working/python`; original local main/resources and the zstd
CLI are also required. None of those original game assets are redistributed.

## Required live checks

1. Launch Core alone. Confirm ENTRY, WAITING_FOR_CONTENT and BOOT_OK with zero
   enabled modules, and unchanged gameplay. Launch all five modules and confirm
   all package versions, ENABLED lines and BOOT_OK without installation errors.
2. Disable/re-enable each module in Eden, fully restarting each time. Verify
   code behavior and resource replacements follow the module toggle. Check a
   below-minimum INI and a newer INI produce the documented error/warning.
3. Check Catalog badges, scrolling, variants, Miles, recipe/ground stars and
   dialogue item sources. Check Pockets Grab 5/10, caption translations, native
   storage withdrawal, Y storage and footer hints with each module alone and
   together.
4. Test Faster Dialogue no-input speed, held B/R boost, release/disconnect,
   multi-page dialogue, choices and normal A/B behavior. The standalone 0.3.0
   hold-to-boost build also still had live validation pending when integrated.
5. Use the existing save backup for Fitting Room purchases: bag plus outfit,
   overlap, 17–20 items, storage overflow, wear/send and Bell deduction/delivery.
   Confirm badge animation and catalog-star interaction in the fitting UI.
6. Check Amiibo late scans, timeout, B cancellation and retry. The standalone
   timer was confirmed at five minutes; unified runtime installation is new.

Switch hardware and other emulators/game versions are not validated targets.
