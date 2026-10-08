# Faster Interactions — ACNH 3.0.3

**v0.3.0 hold-to-boost test is ready.** Dialogue uses the working v0.1.0
faster default. Hold **B or R** for another **2x reveal-rate boost**, then
release to return to the faster default. Text still appears progressively.
Scripted speed limits and dialogue that blocks acceleration remain intact.
A advances/selects as usual; B retains its native finished-text behavior.

Download: `dist/ACNH_3.0.3_Faster_Interactions_v0.3.0_Hold_to_Boost_Test.zip`.
Fully close ACNH, **disable every older Faster Interactions version**, extract
the new folder into Eden's game mod directory, enable **Faster Interactions -
Hold to Boost**, and restart. In particular, do not leave the instant prototype
or v0.1.0 enabled alongside it.

7,402 offline ARM cases and exact-package checks passed. In-game validation is
pending. Try a long villager message with hands off, hold R partway through,
then release R. Repeat with B, several pages, and a response prompt. The held
button should increase the already faster reveal, and releasing should slow it
back down. Speech, emotes and scripted waits retain native handling.

This build has no patch overlap with the 14 checked external workspace IPS
files, including legacy Grab 5/Grab 10, or the previously inspected 60 FPS
SpeedFix. It reads native rates at runtime, preserving any FPS correction
before applying its held boost. Combined in-game behavior remains untested.
See [implementation and validation](research/hold-to-boost.md).

The user reported “No luck” with the v0.2.0 instant prototype. It is retained
as historical research; this version uses normal progressive reveal and adds
none of its instant-display, input-history or page/choice guards. The original
working v0.1.0 release remains available below.

## Original acceleration release

**v0.1.0 test:** eligible dialogue uses the game's normal held-B/L/R
acceleration automatically. You still press A to advance messages and choose
responses. This is the first patch in the Faster Interactions mod.

The patch uses the shared dialogue reveal routine. It retains the existing
checks that disable acceleration and honors scripted speed overrides. Delays
that the native held-button path bypasses are also bypassed. This is the game's
accelerated reveal behavior, including its character-animation handling.

## Install in Eden

1. Fully close ACNH.
2. Extract `dist/ACNH_3.0.3_Faster_Interactions_v0.1.0_Test.zip` into the game's mod directory.
3. Enable **Faster Interactions - 3.0.3 Test** and restart the game.

Expected layout:

```text
Faster Interactions - 3.0.3 Test/
  README.txt
  exefs/FF1D1C05670DB6021C85B624A710B963.ips
```

Target title: `01006F8002326000`. Target main build ID:
`FF1D1C05670DB6021C85B624A710B963`. Use only with this build.
Disable the mod and restart the game to remove it.

One four-byte ExeFS change; no replacement main, subsdk, NPDM, or RomFS files.
No byte overlap was found with the 14 existing workspace IPS patches checked.
Other text-speed mods and full runtime combinations have not been verified.

## In-game test

**User confirmed the intended acceleration in-game on October 8, 2026.**
The tested dialogue contexts were not specified. The universal scope comes from
patching the shared routine; individual dialogue contexts still need testing.

Compare the mod with vanilla while holding R. Try villager, shop, airport,
crafting/catching, and announcement dialogue. Check multiple pages and lines
with pauses. Confirm that text accelerates with hands off the buttons, choices
still wait for input, and A advancement and B cancellation work as usual.
Dialogue that normally prevents acceleration should retain that behavior.

## Validation and rebuild

The source matches a freshly decompressed extracted 3.0.3 main, including its
full SHA-256. The builder rejects altered or already-patched inputs.

The real ARM reveal routine passed 4,608 comparison cases at two module bases:
patched unheld and patched held input each matched vanilla held input. Cases
cover reveal states, message modes, acceleration-blocking flags, countdowns,
renderer states, and speed overrides. External mode lookup and character
animation updates are modeled. This checks local instructions, not full game
behavior. Separate controls verify the original slow path, accelerated speed,
skippable delay, protected delay, and scripted override.

Package validation independently decodes and applies the IPS, checks exact ZIP
contents, and checks overlap with the workspace's existing IPS patches.

```sh
python3 tools/build.py '/path/to/main.uncompressed'
python3 tests/check_package.py '/path/to/main.uncompressed'
PYTHONPATH='../Grab 5/working/python' python3 tests/check_dialogue.py '/path/to/main.uncompressed'
```

Input is a decompressed NSO **including its 0x100-byte header**. ARM checks
require Capstone and Unicorn. On this Mac, Unicorn CPU initialization requires
running outside the desktop filesystem sandbox.

See [research/NOTES.md](research/NOTES.md) for the patch trace and fingerprints.
