# Native bindings and compatibility validation (stage 1)

These are internal implementation interfaces for QoL Enhancements, targeting
the supplied ACNH 3.0.3 main. They are not an external API or a packaged library.

## Boundaries and contracts

`program/core/native/executable_view.hpp` has no native-runtime dependency.
`ExecutableView` borrows a mapped readable text segment from module discovery.
It checks the complete requested instruction range before reading any bytes,
using subtraction for segment bounds and address-overflow checks. Reads use
`memcpy`, including for unaligned views. It cannot prove an arbitrary pointer is
mapped or alive; its segment must come from a supported context.

`Check(Fingerprint<N>)` returns a bounded `CodeResult`, with the binding/site
name, text-relative offset, expected words, actual words when readable, and
`Match`, `OutOfRange` or `Mismatch`. It supports the current one-word and
five-word checks without allocating memory. Out-of-range checks retain expected
words but do not perform partial reads. `runtime.hpp` supplies the main text
view and reports compatibility evidence separately from normal feature
ineligibility. Data-slot range checks are separate from instruction checks.

`FunctionBinding<Signature>` gives each migrated direct native call one named
signature and address. `Resolve` is an internal ABI operation: the caller must
first pass the relevant installation guards and use the correct execution
context. Resolution itself does not validate code, object lifetime or calling
conventions. Existing synchronous callbacks, native virtual dispatch and hook
ordering remain in place; no per-frame executable scanning was introduced.

Version-specific evidence is grouped by domain:

- `program/game/v303/inventory.hpp`: shared resident/item bindings, their
  instruction fingerprints, and the nontrivial eight-byte held-item return.
  The wrapper's destructor preserves the native AArch64 hidden X8 result ABI.
- `program/pockets/game/quick_store_profile.hpp`: quick-store addresses,
  live-object field offsets, menu/feedback ABI types and instruction checks.
- `program/controls/game/traversal_profile.hpp`: traversal bindings, live-object
  offsets and the original pole/ladder/placed-ladder/tool-swap validation groups.
- `program/crafting/game/preview_profile.hpp`: preview bindings, field and item
  table offsets, request types, and DIY/workbench checks. The 48-byte model
  matrix return remains a native indirect result through X8.
- Pockets and Catalog's existing build profiles use the same bounded validator
  and return structured failures to Core activation.

Address, live-object field and item-table declarations remain separate.
Native menu/action identifiers and controller states retain their existing
meaning; they are not interchangeable with code addresses or input bits.
Fingerprints are compatibility evidence, not full build-ID, object-layout or
ABI verification. Existing build preparation still checks the source build ID.

`program/game/inventory.hpp` supplies a borrowed `ResidentView` used by both
quick-store and traversal. Obtain it only from a supported callback's verified
resident byte; the view must not outlive that byte or callback. Temporary
resident arguments are rejected. Callers retain their existing capacity,
slot, controller, state and identity checks. `ItemAt` immediately copies the
complete eight-byte item, including customization/durability bits, and reports
whether native lookup returned a value. No native item pointer is exposed or
retained. `Held`, `IsEquipped` and `ToolKind` preserve their original native calls.

The domain-specific native headers contain the low-level field access helpers.
Stage 4 moves quick-store lifecycle mechanics into the pocket-command service
and its native adapter; see [pocket-commands.md](pocket-commands.md). Preview
session extraction remains planned for stage 6. Stage 2's verified hook ownership and explicit activation
results build on this foundation; see [feature-activation.md](feature-activation.md).

## Additional compatibility guards

Quick-store now checks both inventory accessor prologues before registering
input or installing hooks; these prologues were already verified by traversal.
DIY preview now checks `RecipeLeave`, whose original code is verified against
local `main.raw`. Preview also bounds its known global pointer slots and the
renderer mode slot against main's data segment before publication. A mismatch
declines the affected feature instead of proceeding through unchecked native
access. Existing independent activation paths remain intact. No gameplay rule,
setting, release version, native address or hook site was changed.

## Verification

Commands used for this stage:

```sh
sh tools/build.sh
QOL_BASELINE_ELF=dist/refactor-baseline-35ec0c8/fresh-build/project.elf python3 tools/check.py
c++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined -I source tests/native_bindings_test.cpp -o /tmp/qol-native-bindings-test
/tmp/qol-native-bindings-test
```

Use the toolchain and local asset prerequisites recorded in
`internal-feature-interfaces-baseline.md`. Docker and Unicorn require the
approved unrestricted execution environment on this machine.

- Clean normal-mode build: passed.
- Host boundary tests under AddressSanitizer and UndefinedBehaviorSanitizer:
  passed. They cover every truncated size, exact boundaries, overflow, null
  text, unaligned views, relocated bases and structured mismatches.
- Compiled production installers reject genuinely unmapped zero-length text
  without probing instructions or installing hooks, at two load addresses.
  Changed accessor/recipe-leave instructions and truncated data-slot ranges
  are rejected with compatibility diagnostics.
- With `QOL_BASELINE_ELF` set, the ARM comparison checks individual migrated
  feature combinations and their combined hook sites/readiness against the
  preserved stage 0 ELF at two bases. It also replays the same 978 pocket
  callback assertions against both ELFs and compares their results.
- Full regression run: passed, exit 0; all 13 host suites and all 40 regression
  scripts completed across Core, Shopping, Dialogue, Crafting, Controls, Pockets,
  Fitting and Catalog. The finalized native comparison test was also rerun
  separately and passed all newly guarded dependencies and baseline scenarios.

The pocket harness previously put generated adapters at module+0x80000. Module
growth moved its message buffer across that address, causing a write-protection
failure in the comparison test. Adapters now occupy a dedicated page beyond
all loaded segments, including BSS. Main text, module text and adapter code
remain RX; behavioral assertions are unchanged. Both baseline and new ELF pass
the corrected fixture. This test-only repair is a separate commit.

Local build and test logs are in `dist/refactor-stage1/`. No live Eden test was
performed: native service models and render tests do not establish live engine
effects or visual correctness. Keep stage 0 binaries and release archives for
live comparison, and retain the limits already recorded in `VALIDATION.md`.

Stage 1 completed on 2026-10-13 on `refactor/internal-feature-interfaces`.
`main` remains at `35ec0c865065dda9f4b09660434370d90ac57373`. Hash checks
confirmed all 151 saved pre-build files, all 2,816 pre-existing dist files and
the three saved fresh stage 0 outputs remain unchanged. Stage 1 outputs are
saved separately with `dist/refactor-stage1/build-sha256.json`. No release
packaging, installation update or integration into `main` was performed.
