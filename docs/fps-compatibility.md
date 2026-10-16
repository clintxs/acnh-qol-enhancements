# 1.0.3 FPS compatibility

Version **1.0.3** is based on released **1.0.2** (`370ae0b`) plus the FPS
compatibility fix. It excludes later 1.1 development features. The preceding
1.0.2 revision-2 test build was reported working by the user on 2026-10-16;
that report does not identify every platform/rate combination as tested.

## Supported inputs

The locally supplied **SpeedFix v1.4.1** main patches for **ACNH 3.0.3**, build
`FF1D1C05670DB6021C85B624A710B963`:

- 45 FPS emulator and Switch packages
- 60 FPS emulator and Switch packages
- 120 FPS emulator package

The exact input hashes are recorded in [fps-patch-inputs.json](fps-patch-inputs.json).
The older-game-version patches bundled with those downloads are not targets.
Use one FPS package and its matching resources, following that mod's own
installation instructions. Neither the FPS patches nor their resources are
included in QoL's source or binary archives.

## Confirmed failure and fix

The 1.0.2 Pockets installer expects the original instructions at `0308c590`.
SpeedFix changes the load at `0308c594` and redirects `0308c598` through helpers
at `03088644` and `03090d04`. This rejects `pockets.quick_store` and its dependent
hints/hold selection. Controls independently rejects the changed dialogue timer
subtraction at `01afa53c`; the 45 FPS variant also changes `01afa540`.

Only these two complete vanilla fingerprints gain audited alternatives. The
profile also checks stable FPS markers and both menu-helper instruction blocks.
All other fingerprints retain exact matching. Unknown changes at checked sites
remain rejected.
Hook reservations retain the actual supported instructions for repeated checks;
we do not restore vanilla instructions over SpeedFix.

Accepting the menu fingerprint alone is insufficient. The external helper jumps
back to `0308c59c`, inside Exlaunch's possible 20-byte entry patch. For SpeedFix,
`UpdatePocketMenu::Original` therefore runs a small C++ implementation of this
fully checked native function. It preserves the signed age threshold (2 updates
for 45/60, 4 for 120), result code 1, and the resident's -1-to-7 requested-state
transition. Vanilla still uses the original trampoline. No FPS helper is edited.

## Patch order

The FPS package's IPS/PCHTXT edits are loader-time changes to the executable.
They are already present when QoL's `subsdk6` starts. QoL then installs its
startup hook and activates its feature hooks after the game's RomFS mount.
Consequently its validation and FPS detection inspect the patched code.
Changing folder order cannot make loader-time edits run after QoL's runtime
hooks. The compatibility adapter preserves the supported FPS behavior when
QoL hooks the affected function.

## Timing

Detected nominal update rate is initialized before feature installation.

- Multiselect activation already uses **250 ms of system ticks**. It is unchanged.
- Quantity wrap pause and swimming cooldown/boost timing already use system ticks.
- Crafting/cooking's 60-update ramp is normalized to 30 Hz units: **two seconds**
  at the supported nominal rates.
- DIY preview loading timeout, held-navigation repeat, diagnostic delay, model
  rotation and workbench tilt are scaled to retain their 30 FPS timing/speed.
- Amiibo timeout settings retain their 30 Hz interpretation. The default 9,000
  becomes 13,500 / 18,000 / 36,000 updates, retaining **five minutes**. The native
  counter is 16-bit; very large custom settings saturate at 65,535 updates and
  produce an unconditional warning instead of wrapping.

The later **100 ms selection-ring blink is not in released 1.0.2**. Its development
implementation already uses system ticks; no 1.1 feature is backported here.
Rate scaling follows the verified package's nominal rate, not measured display
FPS. Engine slowdowns and FPS-mod behavior outside QoL are not compensated.

## Verification and live coverage

Run `python3 tools/check.py fps` with the downloaded packages in `../external`.
The test parses the enabled 3.0.3 PCHTXT records and IPS32 records (including the
NSO header offset), applies every patch to local native text, and executes the
compiled suite. Checks cover all five packages at two ASLR bases, all-module and
isolated Controls/Pockets startup, held-Y native dialogue advance/choices/release,
1,120 compiled menu comparisons against actual patched native instructions,
crafting ramp and amiibo durations, preserved FPS patch bytes outside declared
hook spans, and unknown/corrupted patch rejection. An explicit audit checks
external branches into our hook spans.

The host preview/session tests sample 30/45/60/120 Hz for loading timeout,
workbench angular speed and the unchanged multiselect delay. Existing full
regression groups continue to test vanilla 30 FPS behavior. Tests resolve the
calling checkout's ELF so isolated worktrees do not accidentally test another
branch's binary.

The compatibility test has a positive user live report. Additional emulator/Switch
coverage remains useful. Test at each chosen rate:
ordinary dialogue hold-Y and release; normal home pockets Y/ZL/ZR and hints;
container menus; hold-A selling/storage selection; Fitting Room bag controls;
crafting/cooking ramp; preview loading/navigation/rotation; automatic traversal;
and swimming stroke timing/speed. These offline tests model UI/render/world
services and cannot certify those live interactions.

Install 1.0.3 **instead of the existing QoL Core**, retaining
only one copy of `subsdk6`. For a clean comparison, replace the selected suite
folders from the matching full release ZIP, keep personal custom configuration
separate, and restart the game. Preserve the original 1.0.2 ZIP for rollback.
