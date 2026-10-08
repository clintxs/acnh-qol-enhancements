# QoL Enhancements

**0.2.1 player dialogue prototype** for ACNH **3.0.3 on Eden**. One Core
executable in **subsdk6** activates five optional RomFS feature packages. The
standalone source projects/releases remain intact. Read [README.txt](README.txt)
for installation, version policy and user-facing limits.

## Included implementations

| Package | Source baseline | Resources |
|---|---|---|
| Catalog | Catalog Aware Shops 0.8.1 | Four catalog row layouts |
| Pockets | QoL Pockets Interactions 0.1.4 | Editable Grab 5/10 captions |
| Faster Dialogue | C++ hook rewrite of Faster Interactions 0.3.0, with conditional NPC/UI boost | Optional personal configuration |
| Fitting Room | Able Sisters Bag prototype 0.3.7 | Fitting row badge and original counter layout |
| Amiibo | C++ hook rewrite of Amiibo Scan 1.0.0 | Configuration only |

Faster tool-wheel animations and other workspace mods are not bundled in this
initial five-feature suite. Historical source notes under `docs/` describe each
baseline; their old installation/packaging instructions refer to standalone mods.

Development releases advance numerically (for example, 0.9.0 → 0.10.0).
Version 1.0.0 is reserved for the initial release.

## Architecture

`source/program/bootstrap.cpp` validates the original content-mount call and
continuation, then installs one SIMD-preserving discovery hook at main+037B5464.
Only after the game's `nn::fs::MountRom("content", ...)` call does Core read the
five known configuration paths and install enabled features. Activation is
once-only and does no recurring filesystem polling. No host mod-folder access,
new mounts, filesystem permissions or module discovery are needed.

`source/program/core/modules.cpp` holds the per-feature minimum package version
mapping. `release.hpp` identifies the Core release. Missing packages are off;
invalid or below-minimum enabled packages are rejected; newer packages warn.
Do not ship optional configuration defaults inside Core: those would mask removal
of feature packages. RomFS replacements remain under emulator control.

Feature namespaces and verified native bindings are retained under `catalog/`,
`pockets/`, and `fitting/`. Core shares one Exlaunch runtime and larger bounded
hook pools (32 KiB trampoline / 16 KiB inline; fake heap remains 20 KiB).
Detailed feature logs identify the suite and feature. Pockets reads translations
at `content:/qol-enhancements/messages/pockets.ini`.

Faster Dialogue now lives in `source/program/dialogue/`: five SIMD-preserving
C++ hooks implement current B/R input, eligible speed selection/forwarding, and
main/renderer multipliers. The active speaker is resolved through the same
native entity delegate used by Animalese; NPC resource IDs are matched without
localized name comparisons or a last-speaker cache. Native restrictions and
scripted speed overrides remain in force. No binary replacement array or game
code-cave helper remains. Amiibo's existing 9,000-update timeout is likewise a
C++ hook; its behavior is unchanged. Expected instruction words are validation
fingerprints only, checked before each feature's first hook.

The Exlaunch SIMD context declaration now matches its assembly stack layout;
compile-time assertions and execution of the real adapter cover that correction.
Ordinary GP-only callbacks retain their original context layout.

## Conditional Faster Dialogue

The suite keeps its normal versioned `faster-dialogue.ini` marker. Personal
preferences are read once, after that enabled feature passes compatibility
checks, from `content:/qol-enhancements/config/faster-dialogue.custom.ini`.
The main ZIP contains only an `.example` file at its root; it never supplies the
actual custom INI. A separate optional preset ZIP contains:

```
My Custom Faster Dialogue Settings/
  romfs/qol-enhancements/config/faster-dialogue.custom.ini
```

```ini
base_speed = 2
boost_speed = 4
faster_ui = true
faster_player = true
faster_npc_list = Timmy, Tommy, Orville, Wilbur, Kapp'n
```

Enable that personal mod alongside Core and Faster Dialogue. Keep it when
replacing the suite folders, and restart after editing. Do not reinstall the
example preset over edited preferences. Only one mod should supply the custom
path; normal RomFS precedence applies if there are duplicates. The custom file
cannot activate an absent or disabled Faster Dialogue module.

Missing preferences retain base 2 / held B/R 4, with UI/player/list boost off. A listed
NPC, recognized player message or UI uses `boost_speed` automatically; holding B/R selects that
same tier and never stacks to 8. This prototype supports **2 and 4** for both
speed keys; a boost below the base is raised to the base. These are nominal tiers relative to the live native fast rate,
including language/FPS adjustments, not fixed wall-clock multipliers. Native
scripted waits, protected messages and nonnegative overrides retain priority.

The five friendly aliases map to `rcm`, `rct`, `dod`, `doc`, `kpp`. The friendly Kapp'n alias
also includes its known `kppB` profile. Other resource IDs, such as villager IDs,
can also be listed; matching is case-insensitive and
exact, without guessing variant IDs. Up to 64 IDs of at most 15 ASCII identifier
characters are accepted. Syntactically valid IDs not used by a live speaker
simply never match. Bad values/keys warn and use defaults for the affected
setting; invalid list entries are ignored. Duplicate keys revert that setting
to its default. An unreadable/oversized personal file uses defaults without
disabling the feature. Whole-line comments, CRLF and UTF-8 BOM are supported.

`faster_ui` covers native system dialog windows (the blue dialogs). Independent
`faster_player` covers valid player speakers and five reviewed self-message
banks: `FTR_WorkingTable`, `SYS_Player`, `SYS_Get_Fish`, `SYS_Get_Insect`, and
`SYS_Get_DiveFish`. These include DIY workbench prompts/results, recipe learning,
fish/bug/sea-creature catches and other SYS_Player self narration. The active
message bank is read directly, so no speaker entity is required. Matching uses
internal bank names across locales, not displayed text. Known self banks take
priority over window color; unknown banks never become self merely because a
speaker is missing. Balloon/unknown windows retain base speed.

Add `faster_player = true` to your **existing personal INI** and restart; an older
personal file keeps this new option off. Each automatic category selects the
same `boost_speed`; holding B/R never stacks another boost. In-game coverage of
these bank paths failed in the first 0.2.1 live test: workbench, fish and bug
messages did not receive the automatic boost. The reported 0.2.0 NPC recognition
failure is also unresolved; see [research notes](research/dialogue/NOTES.md).
There is no song/travel skipping, forced reveal, voice cleanup or emote filtering.

To investigate these failures, build/package the diagnostic Core with:

```sh
sh tools/build.sh --verbose-dialogue
python3 tools/check.py core dialogue
python3 tools/package_dialogue_diagnostic.py
```

The diagnostic ZIP replaces Core only. Keep the Faster Dialogue module and your
personal settings, including `faster_player = true`, then fully restart. Test a
workbench, catch message and listed NPC, briefly holding B/R for comparison.
Collect Eden's `QoL Enhancements/Faster Dialogue` lines before another session
replaces the log. `DIAGNOSTIC: player-bank-v2` identifies this build; `READY`
shows loaded settings, `MESSAGE` shows active/cached banks, labels and reveal
gates, and `SELECT` shows the classification and speed. MESSAGE runs in the input
hook even when native reveal gates prevent SELECT. Logs stop after 256 MESSAGE
or 128 SELECT transitions; repeated frames do not consume the limits. This
diagnostic adds observations without changing the category rules. Restore normal
Core after collecting the log. Old release ZIPs are preserved.

## Build and package

```sh
sh tools/build.sh
python3 tools/package.py
```

Build uses the existing pinned devkitPro Docker image with networking disabled.
Python 3.14 is required for layout compression. Local original assets are expected
at `../vanilla-3.0.3/Layout`; builders verify their hashes and archive structure.
The package script checks that every INI matches the Core release, refuses to
replace existing archives, and creates:

- `dist/qol-enhancements-v0_2_1_for_acnh_v3_0_3.zip`
- `dist/qol-enhancements-v0_2_1_for_acnh_v3_0_3_source.zip`
- `dist/qol-enhancements-v0_2_1_for_acnh_v3_0_3_faster-dialogue-custom-example.zip`

The binary ZIP contains a conspicuous installation text file, README, Core and
five independently selectable feature folders, plus a SHA-256 manifest. Source
includes modules/configurations, translations, code, tools, tests and notices;
generated game resources and binaries are excluded.

To prepare configuration for a fresh original ExeFS extraction:

```sh
python3 tools/prepare.py /path/to/exefs --hactool ../hactool/hactool
```

The prepared NPDM preserves original services/filesystem/META settings and adds
only the existing four Exlaunch process-mapping/session syscall permissions.
Using subsdk6 avoids Jubbsticks Additions' subsdk9 slot, but does not establish
compatibility with that mod's future hooks or NPDM.

## Validation

See [VALIDATION.md](VALIDATION.md) for actual results and pending Eden checks.
The unified loader and suite require a fresh live test, particularly startup,
module toggles, Pockets translations, fitting checkout, speaker/UI classification
and held dialogue boost. Diagnostic builds use `sh tools/build.sh --verbose-dialogue`
for bounded SELECT logs. Future behavior changes use C++ hooks per AGENTS.md.

Exlaunch is pinned to upstream f9f4b0dd07b68f97958cb9c79228bbca22ca80d5.
Keep GPL-2.0 and framework third-party notices and distribute matching source
with the binary. Game assets are never part of the source archive.
