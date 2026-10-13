# Pockets: readable feature behavior

Work started from `e2cef90c5aaaa98c4c6b3e0232954adb64ab8b93` on
`refactor/internal-feature-interfaces`, with a clean working tree. The
continuation branch is `refactor/semantic-feature-api`. `main` and the earlier
refactor branch remain comparison points. This work stops at Pockets.

## Read in this order

1. [features/pockets.hpp](../source/program/pockets/features/pockets.hpp) contains
   the actual policies: Grab 5/10 placement, shortcut action preference, explicit
   sole-action checks, hold-A intent, and fixed footer hints. These handlers are
   used in production, not illustrative replacements for a second implementation.
2. [features/pockets.cpp](../source/program/pockets/features/pockets.cpp) assembles
   the existing feature dependencies and multiselect setting. The small feature
   registration files connect those handlers to supported native lifecycles.
3. [features/quick_store.cpp](../source/program/pockets/features/quick_store.cpp)
   shows Y/ZL/ZR bindings and the feedback policy: storage confirms after success,
   trigger actions confirm before their first dispatch, and Open adds no second
   confirmation. Successful returning shortcuts request a cursor tap.
4. [game/item_menu.hpp](../source/program/pockets/game/item_menu.hpp),
   [game/stack_menu.hpp](../source/program/pockets/game/stack_menu.hpp), and
   [game/pocket_footer.hpp](../source/program/pockets/game/pocket_footer.hpp)
   define the small operations and their ownership/failure contracts.
5. [services/pocket_commands.cpp](../source/program/pockets/services/pocket_commands.cpp)
   owns the existing deferred command lifecycle. The files ending in
   `_native.cpp` under `game/` own layouts, hook phases, native calls and
   compatibility fingerprints. Native callback names/namespaces are retained
   where useful for comparison with the inherited ARM fixtures.

There is no general `Execute` interface. A menu policy chooses an index from one
callback-scoped snapshot. The adapter rejects an invalid or unsupported result,
then copies native eligibility evidence into the existing request lifecycle.
This is deliberately narrower than an arbitrary menu executor. Policies cannot
manufacture a native row ID or an eligible request. Services do not include the
feature policy header or choose equipment/consume groups themselves.

## A real before/after

Before, `InstallQuickStore` declared `ZR -> EquipOrConsume`, but understanding
what that meant required reading `Matches`, `IsConsume`, and the native row
selection loop in `pocket_commands_native.cpp`.

Now `EquipOrConsume` visibly makes two passes. The first chooses the first
executable Wear/Hold/Put Away entry. The second considers Eat/Eat 1/Drink. The
adapter supplies those verified meanings, preserving native menu order. A menu
with Eat before Wear still chooses Wear; a disabled Wear falls back to Eat.
Eat 10 Turnips has no supported consume meaning. The old selection loop is gone.

Before, storage's sole-action decision mixed its native action number and
message key with `count == 1`. Now `WithdrawDirectly` visibly checks the number
of entries and compares `ItemAction::MoveToPockets`. The adapter retains the
native number/key recognition and existing context/flag checks.

## Description-to-code map

| Player behavior | Feature policy / declaration | Underlying operation and native authority |
| --- | --- | --- |
| Grab 5/10 | `AddGrabFiveAndTen` | `StackMenu::Contains`, `Space`, `InsertAfter`; native BuildStackMenu determines Grab 1 availability, stack count and free pockets. Native Split still moves items and runs normal cleanup. |
| ZL Drop/Release | `DropOrRelease`; Y/ZL/ZR declarations in `quick_store.cpp` | Adapter builds actual eligible rows; command service queues and revalidates the chosen action before original native Command UPDATE. |
| ZR equipment before consumption | `EquipOrConsume` | Same actual-row probe and deferred lifecycle; native action/capacity/equipment restrictions remain authoritative. |
| Y home storage | `StoreAtHome` | Adapter retains the own-home query and actual Store row check; original native storage handler moves/removes the item. |
| Hold A to select/deselect | `CanHoldSelect`, `HoldSelection::Release`, `Begin`, `InitialClick`, `WantsToggle` | Adapter resolves supported picker context and fresh cursor movement, rejects blocked/ineligible widgets or existing native requests, then queues the native click. Native browsing enforces selection/storage limits and toggles the item. |
| Direct withdrawal | `WithdrawDirectly` | Storage adapter borrows the verified native menu builder, records entry intent, reprobes on update, and calls original selection processing with a temporary confirmed row. |
| Direct Open | `OpenContainerDirectly` | Adapter recognizes all three supported Open IDs plus message keys. Existing pocket menu entry/update lifecycle preserves the input lock and defers dispatch. |
| Direct recycling Claim | `ClaimDirectly` | Recycling adapter validates the native record and menu, pauses the grid at entry, compares the copied record on update, invokes original selection processing and dismisses the name only after removal. |
| Fixed Y/ZL/ZR hints | `AddShortcutHints` | Scoped `PocketFooter::Append` retains the five-slot pool, duplicate/type guards and native descriptor copying/sorting. Y appears only at home. |
| Captions and feedback | Feedback declarations in `quick_store.cpp`; held selection's native feedback adapter | Existing locale/INI provider and message keys remain. Storage/withdrawal/drop use native translations where already supported; Use Item remains editable. Held moves use Decide only when a native selection request was queued; otherwise Select is replayed. |

`SelectionContext::ItemSelection` corresponds to the existing native cItemSelect
mode used by selling. It does not claim that mode identifies selling exclusively.
Home storage put-away is a separate native context. The existing settings gates,
accepted syntax, resources, translations, controls and release version are
unchanged. The three bypass settings retain their existing native guard points;
multiselect retains its assembly-time installation gate.

## Preserved execution, ownership and cancellation

Menu snapshots are synchronous borrows. They include unsupported rows so an
extra disabled/unknown entry still prevents a sole-action bypass. No snapshot or
entry reference survives into another callback. Function pointers and feedback
declarations are copied at startup; incompatible repeat registrations are
rejected before publication. Alternate supported button mappings still work.

A shortcut success path remains: fresh unconflicted input -> actual native menu
probe -> copied owner/resident/slot/exact item/row/generation evidence -> queued
Command -> revalidation -> original native UPDATE. Started multi-frame commands
continue without restarting or revalidating mutations made by their own action.
Feedback occurs in the declared phase, and a cursor tap resolves its widget anew
on native Play entry. Changing the resident/item/selection/generation before
start cancels instead; input is not consumed for a rejected probe. Chords,
blocked input and held triggers retain the existing suppression behavior.

Storage withdrawal keeps its **narrower baseline contract**: entry stores only
controller intent; update reprobes the current menu, consumes intent before
calling native code, restores temporary menu state and clears selection. An
added menu option falls back to original menu entry. It does not gain exact item
identity correlation or an empty-custom-callback guarantee merely by using
`MenuEntry`. Its original action/message/flags/extra checks remain. That further
research/correction would need separate native evidence and tests.

Recycling entry copies the full 12-byte item/record identity. Update rejects a
changed item or slot and restores ordinary entry. Native Claim checks pockets
capacity, inserts before removal, and supplies the full-pockets message on
failure. Name dismissal follows actual removal. No second insertion or removal
is introduced. Existing supported entry callbacks remain the correlation
boundaries; none of these interfaces proves arbitrary engine pointer lifetime.

Grab menu handles are action identities scoped to the synchronous list. They
remain usable after insertion, while references/indices can move. The operation
rejects missing/ambiguous anchors, duplicate new IDs and capacity overflow.
Compared with the baseline, malformed menus containing duplicate Grab 1 anchors
now receive no extension, and a pre-existing Grab 10 is not inserted twice.
These bounded-operation safeguards are explicit and tested; ordinary menu
ordering/eligibility is unchanged.

## Verification and retained outputs

The starting executable was independently rebuilt from `e2cef90` in a temporary
source export. Its ELF SHA-256 is
`2d7533904fcab4f196ba81a56af0bfcbcc8230d39d2027382397a4e295ce15f8`, matching
the earlier saved stage-7 candidate. Separate baseline ELF/NSO/NPDM copies and
output preservation hashes are under ignored `dist/native-first-pockets/baseline/`.
No release archives or installed Eden files are replaced.

The build/check tools are the existing pinned, network-disabled Docker build and
`tools/check.py`. Sandbox-only attempts encountered the previously documented
Docker socket denial and Unicorn SIGILL; approved execution outside that sandbox
resolved those environment restrictions. Logs are retained under
`dist/native-first-pockets/`.

New checks exercise action priority/fallback, sole-action rejection, insertion
handles/collisions, held selection direction/context/release and fixed hint
order. The compiled registration fixture starts with fresh callback registries,
uses the production feature installers, executes stack and deferred-storage
behavior, and checks changed-menu cancellation, once-only confirmation and
incompatible policy rejection at two load addresses. It also calls the real
menu adapter with an out-of-range selection, flagged row and unknown action;
all are rejected. Inherited fixtures still model their own registration for
broader callback coverage. No original observable assertion is removed.

Live Eden verification remains required for actual audio, cursor focus,
rendering and gameplay integration. Retest Grab 5/10 including full pockets and
turnips; ZL critter release/drop; ZR equipment and consumption; Y storage at home
and away; sole Open versus multi-option menus; storage and recycling capacity
failures; and hold-A select/deselect, backtracking and release after a blocked
picker. Compare against the retained baseline, including module toggles and
Pockets/Fitting Room captions. No live qualification is claimed.

## Final results — 2026-10-13

Executable source commit: `9c19246df5f05240a5d4c288349680e70a7b84a2`.
Documentation is committed separately. No source changes followed the tested
normal build.

| Check | Result |
| --- | --- |
| Pinned normal Docker build | Passed, exit 0; no compiler warnings/errors. `build.log`. |
| Focused Pockets run | Passed: all 20 host suites and all 7 Pockets ARM scripts. `focused-pockets.log`. |
| Full regression run | Passed, exit 0: all 20 host suites and all 47 ARM/resource scripts. `full-check.log`. Includes all 128 module subsets at two bases and Pockets/Fitting Room caption coexistence. |
| Baseline comparison | Passed: original 978 Pockets scenarios and migrated hook sites/readiness match the separately rebuilt starting ELF. |
| Preservation | Passed: 3,126 pre-existing saved distribution/comparison files unchanged; `main` and the original refactor branch unchanged. `preservation.log`. |
| Candidate | Matching ELF/NSO/NPDM and unchanged Pockets configuration/messages saved in `dist/native-first-pockets/candidate/`, with source identity and SHA-256 manifest. |
| Live Eden | Not run; audio, rendering and actual gameplay qualification remain pending. |

The source and tests are ready for review at the Pockets boundary. Other modules
retain their implementations; the only Core change delegates Pockets assembly
to its own module entry.
