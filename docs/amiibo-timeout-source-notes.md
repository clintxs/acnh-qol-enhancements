# Amiibo Scan — 5 Minutes

ACNH **3.0.3**, v1.0.0. A single-instruction ExeFS patch extends the
shared event Amiibo scan countdown from **450 to 9,000 updates**: approximately
**15 seconds to 5 minutes** at normal 30 Hz game speed.

## Install in Eden

1. Fully close the game.
2. Extract `dist/ACNH_3.0.3_Amiibo_Scan_5_Minutes_v1.0.0.zip` into the game's mod directory.
3. Enable **Amiibo Scan - 5 Minutes** and restart the game.

The resulting path is:

```text
Amiibo Scan - 5 Minutes/exefs/FF1D1C05670DB6021C85B624A710B963.ips
```

No exlaunch, subsdk, or RomFS changes are required. Disable the mod and restart
the game to remove it. The archive uses emulator mod-folder layout.

Target title: `01006F8002326000`  
Target main build ID: `FF1D1C05670DB6021C85B624A710B963`

Do not use this build with a different main build ID.

## Coverage and limitations

Resident Services (ABD/Nook Stop), the HHP office, the Roost, and the Hotel all
call `Obj_Common_AmiiboProcess::NFC_ReadProcess`, which invokes
`EventFlowSystemActor.ReadAmiibo`. They should therefore share this patched
countdown. The HHP cast-selection flow also invokes `ReadAmiibo` directly.
Other scanning paths, such as Photopia's continuous scanning, are not claimed
as verified coverage.

The timer counts game updates, not wall-clock time. Pauses, game speed changes,
and mods that change update frequency may change the real elapsed time.
The five minutes apply to the reader's waiting state; opening/closing animations
can add a small amount of time.

## Validation

- Freshly decompressed the supplied extracted main and confirmed it exactly
  matches the binary used for research.
- Checked build ID, full decompressed NSO hash, patch bytes, and IPS32 format.
- Executed the original and patched ARM initialization/countdown instructions
  using Unicorn at two module bases with executable memory set read-only.
- Original expires on update 450; patched expires on update 9,000, never earlier.
- When the reader leaves the waiting state, the timer branch is bypassed.
- Verified the countdown changes only its two-byte counter within the test object.

**User-confirmed in-game on October 6, 2026:** a stopwatch measured the timeout
at exactly five minutes, and an Amiibo scanned after roughly one minute was
accepted. The scanning location was not specified.

Early B cancellation, retry behavior, and coverage at each of the four locations
have not yet been individually confirmed. The ARM instruction test does not
emulate the complete UI or NFC service.

## Rebuild

Supply the original **decompressed NSO**, including its 0x100-byte header:

```sh
python3 tools/build.py /path/to/main.uncompressed
```

The builder refuses a mismatched or already-modified binary. Run the focused ARM
test with `capstone` and `unicorn` available:

```sh
python3 tests/check_countdown.py /path/to/main.uncompressed
```

This workspace already has those libraries under `../Grab 5/working/python`.
Unicorn's CPU initialization requires execution outside the desktop filesystem
sandbox on this host.

## Patch

| Item | Value |
|---|---|
| Module-relative instruction | `0x00BC4140` |
| IPS32 record offset | `0x00BC4240` (includes NSO header) |
| Original | `48 38 80 52` — `mov w8, #450` |
| Replacement | `08 65 84 52` — `mov w8, #9000` |
| Record length | 4 bytes |
| Complete IPS32 file | 19 bytes |

See [research/NOTES.md](research/NOTES.md) for the trace and source fingerprints.
