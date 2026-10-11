# QoL Pockets Interactions

Version **0.1.4**, for Animal Crossing: New Horizons **3.0.3** on Eden.
Standalone successor to the pocket features in Optimized UI 0.5.16.

- Adds **Grab 5** and **Grab 10** beside the native Grab 1 action when the stack
  and free pocket space allow it. Turnips retain native behavior.
- Skips the redundant single-action storage menu when withdrawing an item.
- Labels the normal home-storage A hint **Move to Pockets**, using the game's
  existing localized text. Other Select hints and donation confirmations stay unchanged.
- Uses **Y** for the native Put in Storage action in normal pocket browsing when
  the game's normal location/item/storage conditions allow it.
- Plays the native cursor tap after a successful quick-store refresh (prototype;
  visual timing still needs Eden testing). Sound and transfer timing are unchanged.
- Keeps translated Grab 5/10 captions in an editable messages.ini.
- Adds the native **(Y) Put in Storage** footer hint in normal pockets at home.
  The label reuses the game's existing translation; the icon uses the game's
  Y glyph, so a font that swaps X/Y should display X without another setting.
  This is a screen-level hint, not a promise that every selected item can be stored.

The storage and Y actions retain their existing checks. This mod includes no
catalog stars, shop dialogue changes or multi-select shopping.

## Installation and migration

1. Disable/remove **Optimized UI** in Eden. Keep its ZIP for rollback.
2. Extract `QoL Pockets Interactions 0.1.4.zip` into the game's mod directory so
   the `QoL Pockets Interactions` folder contains `exefs` and `romfs`.
3. If you customized the old captions, copy the contents of the old
   `romfs/OptimizedUI/messages.ini` into the new
   `romfs/QoLPocketsInteractions/messages.ini`. The `optimized_ui.grab_five` and
   `optimized_ui.grab_ten` keys are deliberately retained for compatibility.
4. Enable QoL Pockets Interactions and restart the game. Enable **Catalog Aware
   Shops** separately if you want shop ownership stars too.

Either new mod can be used alone. Pockets uses `exefs/subsdk9`; Catalog Aware
Shops uses `exefs/subsdk8`. Their `main.npdm` files are byte-identical and both
packages are self-contained. Do not rename those executable slots or keep the
old combined mod enabled with the standalone replacements.

Expected startup log: `[QoLPocketsInteractions] BOOT_OK 0.1.4; gameplay_hooks=16`.
All feature diagnostics now use the `[QoLPocketsInteractions]` prefix.

Updating from 0.1.1: replace the files in the existing mod folder; do not enable
both versions. Keep any customized `messages.ini`. The hint requires no new INI
keys, font files, layouts or LayoutMsg replacements. The working Y input and
storage-transfer implementation are unchanged. This first hint build still
needs live testing for placement, spacing and font-mod rendering.

## Build and validation

### Optional quick tool wheel experiment

Latest comparison: `dist/Quick Tool Wheel 4x - test 3.zip`. Build with
`python3 tools/build_fast_tool_wheel.py /path/to/romfs/Layout --speed 4`.
The opening goes from 8 to 2 frames; the 14-frame effect's keys become 3.5 frames
long, with its integer duration rounded up to 4. Slopes are scaled accordingly.
Use only one Quick Tool Wheel version at a time. The pre-opening delay is unchanged.
Test 2 is user-confirmed functional and feels faster; test 3 awaits Eden testing.
Checks cover 700 timing fields, preserved other members, declared compression size,
ZIP integrity, 2/4 frame durations, and exact reproduction of test 2's payload at 2x.

`dist/Quick Tool Wheel 2x - test 2.zip` is a separate LayeredFS add-on, not a
replacement for the working pockets executable. Enable it alongside this mod;
disable it to restore native tool-wheel opening speed.

Test 1 is user-reported broken: the wheel no longer works. Its Zstandard frame
omits the decompressed size (vanilla declares 158292 bytes). Test 2 fixes that
packaging defect using `--stream-size` and `--no-check`, verifies the declared
size and frame descriptor against vanilla, and retains the same animation edits.
Disable/remove test 1 before testing this replacement. Runtime recovery is not
yet confirmed; a successful desktop decompression alone is not a loader test.

Reproduce with `python3 tools/build_fast_tool_wheel.py /path/to/romfs/Layout`
(requires the zstd CLI). It changes only `LRingMenuTools` opening `In` and
`InEffect`: 8→4 and 14→7 frames, all key times halved, Hermite slopes doubled
to preserve the curve shape. No closing/equip/selection or layout changes.
Archive byte-whitelist checks, unchanged-member checks, compression roundtrip
and ZIP integrity pass (700 timing fields). No game assets are committed.

Runtime verification is pending: compare D-pad Up and touchpad opening, immediate
selection/A, B cancellation, reopening, and tool registration. A separate native
input delay may remain even with faster visuals. Conflicts with other replacements
of `romfs/Layout/LRingMenuTools.Nin_NX_NVN.zs`; the supplied 16:10 source does not
replace that asset. The builder refuses to overwrite the existing test ZIP.

### Main executable

`sh tools/build.sh` uses the pinned Docker toolchain with networking disabled.
`python3 tools/package.py` produces versioned binary/source archives and refuses
to replace existing releases. The source project builds independently and does
not include the catalog ownership implementation.

`tests/arm_stack_test.py` exercises compiled callbacks and original game code
using local, undistributed game assets. See `VALIDATION.md` for results and
limits. Test Grab 5/10, storage withdrawal and Y-to-storage alone and with Catalog
Aware Shops enabled. Offline startup checks pass in both orders; live Eden
loading of the newly separated pair still needs verification.

## Reading the implementation

Start with `source/program/bootstrap.cpp`: supported-build check, framework
initialization, then stack, storage-withdrawal and quick-store installation.
Version 0.1.1 is a readability refactor; intended gameplay behavior is unchanged.

| File under `source/program` | Responsibility |
|---|---|
| `features/stack_menu.hpp` | Pure rules deciding when Grab 5/10 appear. No game pointers. |
| `features/stack_splitting.cpp` | Connects native menus to private actions and wraps the game's split operation. |
| `features/split_call.s` | Preserves the quantity in an ARM stack frame so only our split calls change counts. |
| `features/menu_text.cpp` and `text/catalog.hpp` | Load translated captions and replace the native caption text. |
| `features/storage_policy.hpp` | Pure rule for recognizing the single Move to Pockets action. |
| `features/storage_withdrawal.cpp` | Defers a validated single-action withdrawal until the native command update. |
| `features/quick_store.cpp` | Detects a fresh Y press, probes native eligibility, queues a request, revalidates and dispatches it. |
| `features/storage_hint.cpp` | Adds a native Y footer record at home and routes only its private caption token to the existing localized Put in Storage label. |
| `features/pocket_input_probe.cpp` | Retained diagnostic probe; not installed by bootstrap. |

For quick store, read `HandleQuickStoreInput`, `CanQuickStoreItem`, then the
`QuickStoreCommand` callback. `NativeMenuAllowsStorage` asks the game whether its
own Put in Storage action is available; the mod does not replace those rules.
Trace helpers only explain decisions in logs.

`address::...` values locate native code/vtables relative to the main module;
`layout::...` values locate observed fields in live objects. A field name records
what has been verified, not a claim that we know the entire native class. ARM
fingerprint arrays intentionally keep exact hexadecimal instructions because
they detect an unsupported game build or another patch at the same location.

`HOOK_DEFINE_TRAMPOLINE` replaces a native function and exposes `Orig` to call
its original implementation. `HOOK_DEFINE_INLINE` receives saved CPU registers
at an instruction inside a function. Comments describe the relevant registers
and stack positions; the wrapper and callback ABI must stay in agreement.

Both storage shortcuts cross native state transitions. Entry records intent;
Update consumes it, rechecks the selected item, and lets native code perform the
transfer. Request pointers are identity tokens, not owned objects. The comments
explain why moving work into Entry or retaining pointers longer would be wrong.

Formatting is defined in `.clang-format`, including the hook macros. This pass
changes project code under `source/program`, leaving upstream `source/lib` and
`source/rtld` intact. Existing translation keys remain compatible.

## Merged suite 0.4.2: trigger shortcuts

`features/quick_store.cpp` now hosts the shared native command shortcut path:
Y storage, ZL Drop/Release, ZR Hold/Wear. `pocket_shortcuts.hpp` names the input
bits and native menu row IDs. Do not confuse BCSV row indices with the operation
enum stored in each row. `ItemUIContextMenu.bcsv` rows 5/6/30/41 name
Wear/HoldHand/PutUnitIcon/Release and use CmnSubWin 0107/0108/0110/0118.
Native action mapping 0307d490 maps these to callback indices 7/8/10/26 in
04a06740, pointing to 0307464c/03074d90/0307513c/03076698.

Important: Drop's 0307513c callback returns incomplete while +1c517 advances
through its initial six-update sequence. Command entry 0308c5c4 resets that
byte. Keep supplying the selected action on later Command UPDATEs, but restore
the menu's old row/count between calls. Revalidating inventory halfway through
placement or clearing the request after one UPDATE can interrupt native work.
Stop supplying it when native dispatch queues refresh/close/error. Only queue
cursor feedback for transitions back toward browsing, not closing Pockets.

UI button IDs 10/11 have native glyph records initialized at 032f94c4 onward
(glyph message keys 007/008), and UI masks are 1<<button. These are not HID
button-mask values. Existing input ownership/focus checks are reused.

`storage_hint.cpp` appends fixed ZL/ZR hints after the existing home Y hint,
bounded by the native five-record footer. New captions are private INI keys;
USen fallback is shipped and users can add per-locale overrides. Shared
`core/hint_text.cpp` owns 01aea210 once and accepts independent providers from
Pockets and Fitting Room; do not install two hooks at that entrypoint.

## Hold A to select while moving (development test)

Selling and shed put-away support holding A while moving over pocket items.
Entering an unselected, enabled item queues the same request as a native click.
Already selected items remain selected when revisited; separate A taps retain
vanilla toggle behavior. Other pockets modes and single-item pickers are excluded.
Release A after opening the picker or after input is blocked before starting a
new gesture. This adds no new caption or configuration setting.

`UIItemSelectWindowHandling` uses `cItemSelect` for selling and `cPutInStorage`
for shed put-away. Native 3.0.3 handler 00d0fd10 maps these to opening requests
0x0a and 0x60; dispatcher 02d1e950 maps them to pockets modes 1 and 9, and
033598a0 writes the mode at controller +1c4fc. The cSell/cMoveToChest strings
choose eligibility filters, not controller mode numbers.

Core's existing 03c58d24 page-input trampoline now supports observers around
native dispatch. Hold-select captures allowed held A and old focus before the
native pass, then resolves new focus afterward. Native processing consumes all
input rows, so consumption must be checked before dispatch. A suppressing
handler sends a blocked observation and skips native/post dispatch. Shared
binding/focus lookup lives in `pocket_input.cpp`, also used by Quick Store.
The existing Play-entry hook resets the gesture when a picker reopens.

Native click 030884b0 queues slot +1c518 and logical bit 2 at +1c51a; hold-select
mirrors that request only if no native request is pending. Native browsing
update 0308b3f0 retains ownership of mask toggles, selection limits, mode 9
capacity errors, and feedback. Producer/consumer fingerprints are checked at
installation. No global button-repeat or navigation behavior is changed.

`tests/hold_select_arm_test.py` runs compiled observers/shared dispatch and the
original selection code through mask/limit/capacity/refresh handling at two
ASLR bases. Navigation and rendering are modeled. Live Eden testing must still
confirm input/update timing, cursor feedback and storage-full behavior.
