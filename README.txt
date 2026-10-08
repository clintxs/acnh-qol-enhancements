QoL Enhancements 0.2.1 — ACNH 3.0.3 — Eden integration test

CORE IS REQUIRED. Install and enable “QoL Enhancements - Core (REQUIRED)”
with any feature module. All packages in this ZIP belong to the same release.
Conditional Faster Dialogue prototype; in-game validation is pending.

INSTALL
Fully close ACNH. Disable the standalone Catalog Aware Shops, QoL Pockets
Interactions, Able Sisters Bag Prototype, all Faster Interactions versions,
Amiibo Scan - 5 Minutes, and the historical Optimized UI/Grab 5 patches.

For the full suite, copy all six “QoL Enhancements” folders into the game's
mods folder, then enable them in Eden. For selected features, copy Core plus
only the feature folders you want. Restart the game after changing modules.

Core uses subsdk6, leaving subsdk9 available for Jubbsticks Additions.
This avoids that executable-slot collision; compatibility with its eventual
ACNH v3 release still needs testing. Other mods must also avoid subsdk6 and
our hook/layout files. Core supplies the suite's only main.npdm.

MODULES
Catalog: catalog/recipe stars, catalog badges and furniture-variation markers.
Pockets: Grab 5/10, direct storage withdrawal, Y-to-storage and footer hints.
Faster Dialogue: faster default reveal; B/R boost; optional automatic NPC/UI boost.
Fitting Room: Y toggles a shopping bag; native bulk checkout includes bag items.
Amiibo: shared event scan timeout extended to 9,000 updates (~5 minutes at 30 Hz).

CONFIGURATION AND UPDATES
Each feature supplies romfs/qol-enhancements/config/<feature>.ini with:
  enabled = true
  version = 0.2.1
No configuration editing is needed for normal installation. Disable a feature
through Eden to remove BOTH its code activation and its resource replacements.
Setting enabled=false stops its code only; its RomFS resources remain loaded.

Replace entire suite folders when updating. Preserve any customized Pockets
captions from romfs/qol-enhancements/messages/pockets.ini before replacement.
The legacy optimized_ui.grab_five/grab_ten translation keys are retained.
Config files accept enabled and version, whole-line #/; comments, a UTF-8 BOM,
CRLF, and ASCII text up to 4 KiB. Both keys are required; duplicate/unknown keys,
invalid booleans or versions are rejected. Versions use numeric major.minor.patch.

PERSONAL FASTER DIALOGUE SETTINGS
Install the optional separate faster-dialogue-custom-example ZIP alongside
Core and Faster Dialogue to try the requested preset: base 2x, B/R 4x, automatic
4x for Timmy, Tommy, Orville, Wilbur, Kapp'n and recognized UI/player narration.
The personal mod contains:
  romfs/qol-enhancements/config/faster-dialogue.custom.ini

Edit this file to customize:
  base_speed = 2
  boost_speed = 4
  faster_ui = true
  faster_player = true
  faster_npc_list = Timmy, Tommy, Orville, Wilbur, Kapp'n

The five internal IDs are rcm, rct, dod, doc, kpp; other actual NPC resource
IDs work too. Kapp'n also covers its kppB profile. Matching ignores case.
This prototype accepts speed tiers 2/4.
Holding B/R selects boost_speed without stacking it on an automatic boost.
Speeds preserve the game's language/FPS-adjusted fast rate; animations and
scripted waits can limit conversation speed. Native overrides take priority.
No custom file: base 2, held 4, automatic UI/player/NPC boost off.

Keep the personal mod folder when replacing the six suite folders. Do not
reinstall the example preset over your edited preferences. Enable only one
mod providing the custom file. Fully restart after changes. The custom file
has no version/enabled key and cannot enable an absent/disabled module.
Bad custom settings warn and fall back per setting; invalid IDs are ignored.
Use whole-line #/; comments, at most 4 KiB, and up to 64 ASCII resource IDs.
Do not put personal preferences in the versioned faster-dialogue.ini.

faster_ui covers blue system dialog windows. Independent faster_player covers
valid player speakers and the reviewed FTR_WorkingTable, SYS_Player,
SYS_Get_Fish, SYS_Get_Insect and SYS_Get_DiveFish banks. This includes DIY
prompts/results, recipe learning, catches and other SYS_Player self narration.
It reads the active message bank, so no speaker entity is needed. Known self
banks take priority over window color. Unknown banks/missing speakers alone
never imply player; balloon/unknown windows retain base speed.
Add faster_player = true to your existing personal INI and restart. Missing
faster_player defaults to false. In-game coverage still needs testing.
The reported NPC recognition problem is not fixed by this addition.
Kapp'n songs/travel retain native scripting; no skip is added.

LOGS
Core logs its built-in version, each installed package version and minimum,
absent/disabled features, newer-package warnings, and installation results.
Enabled packages below Core's per-feature minimum are rejected. Newer packages
produce a warning and are attempted if their settings and hooks are supported.
Missing/invalid/unreadable config files do not activate feature code.

Look for WAITING_FOR_CONTENT, followed by BOOT_OK with enabled_modules/errors.
BOOT_PARTIAL means at least one requested module had a configuration or
installation error. Some hooks may have installed before a feature-specific
failure; restart after fixing or disabling the affected module. RomFS assets
remain controlled by Eden even when Core rejects their feature code.

TESTING AND SUPPORT
Test the unified build in Eden before publishing it. Existing standalone
validation does not confirm the new post-mount activation point or combined UI.
For a bug report, include Core/feature startup logs, game version, enabled mods,
and steps to reproduce. Package versions identify the supplied INI; they do not
prove that every accompanying resource file was replaced correctly.

Fitting Room retains the existing prototype's transaction behavior and limits:
256 distinct bag selections plus the tried-on outfit; storage first, pockets
for overflow; tailor tickets cover the native outfit, not bag extras. Purchases
above sixteen entries use a preview in the fixed native purchase report while
full delivery/catalog/milestone loops process all items. Use your existing save
backup when testing purchases. See the source README for remaining live checks.

Built with Exlaunch (GPL-2.0). LICENSE is inside Core; the matching source ZIP
includes framework notices and reproducible build/package tools. Original game
assets are required locally for rebuilding and are absent from the source ZIP.
