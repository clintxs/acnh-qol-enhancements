QoL Enhancements 0.3.20 — ACNH 3.0.3 — Eden integration test

CORE IS REQUIRED. Install and enable “QoL Enhancements - Core (REQUIRED)”
with any feature module. All packages in this ZIP belong to the same release.
Shopping Card: recovery of the shared counter after it disappeared in 0.3.9.
Native 16:9 placement; regular catalog self-orders use savings.

INSTALL
Fully close ACNH. Disable the standalone Catalog Aware Shops, QoL Pockets
Interactions, Able Sisters Bag Prototype, all Faster Interactions versions,
Amiibo Scan - 5 Minutes, and the historical Optimized UI/Grab 5 patches.

For the full suite, copy all seven “QoL Enhancements” folders into the game's
mods folder, then enable them in Eden. For selected features, copy Core plus
only the feature folders you want. Restart the game after changing modules.

Core uses subsdk6, leaving subsdk9 available for Jubbsticks Additions.
This avoids that executable-slot collision; compatibility with its eventual
ACNH v3 release still needs testing. Other mods must also avoid subsdk6 and
our hook/layout files. Core supplies the suite's only main.npdm.

MODULES
Faster Crafting: automatic 1x-to-3x ramp for crafting and cooking.
Catalog: catalog/recipe stars, catalog badges and furniture-variation markers.
Pockets: Grab 5/10, direct storage withdrawal, Y-to-storage and footer hints.
Faster Dialogue: faster default reveal; B/R boost; optional NPC/UI/player boost.
Fitting Room: Y toggles a shopping bag; native bulk checkout includes bag items.
Shopping Card: Nook's Cranny/Able Sisters purchases use ABD savings; Nook's
sales deposit to savings and use the ordinary “Thank you! Please come again!”
message. 0.3.3 removes the misleading pockets-full/wiring explanation. Includes
fitting checkout and the optional fitting bag. Nook Shopping/catalog orders for
yourself use savings at Nook Stop and through the phone app. Catalog gift
orders have a separate native payment path and are not covered in this prototype.
Always active when this module is enabled. No item, menu addition or unlock yet.
The top-right display shows a green shopping card and full savings balance
inside these shops and while the catalog is open; closing the catalog outside
these shops restores ordinary Bells. The white English “Bells” badge
has green text and sits at the lower right.
The oval has a thicker white rim; the centered card has a dark edge, with
its white backing behind the oval. 0.3.10 restored the counter; the user
confirmed it works. 0.3.11's rounded card lost its colors because native shape
drawing skips pane vertex colors. 0.3.12 corrected colors but still rendered
split, distorted geometry. 0.3.13 uses an ordinary textured picture for the
complete card and a separate white backing behind the oval. The card textures
include smooth corner coverage. Original cash texture pixels/settings, panes,
materials and animations are preserved. Packaged image decoding and native
texture-name lookup pass. The user reported the whole counter missing in
0.3.13: the archive exceeded its native resource-size allowance. 0.3.14 adds
an allocation floor for this counter only, preserving other resources and
larger mod overrides. This support also keeps cash visible when Shopping Card
behavior is disabled but its assets remain installed. No shared RSTC file is
replaced. Update both Core and Shopping Card, then fully restart the game.
The user confirmed 0.3.14 works. 0.3.16 moves the two card-detail lines up
one more screen pixel at 1280 width and makes the border a lighter blue-green
after the previous shade still appeared black in-game. Native size lookup and SDK
relocation pass; the final cosmetic changes still need an Eden check.
The Bells badge retains its shift about 16 screen pixels left at 1280-pixel width.
Savings changes use native count-up/down animation and sound.
Entering shops, switching residents and unchanged balances are silent.
0.3.4 corrects the HUD path missed in 0.3.2–0.3.3, which
left the regular in-shop counter showing cash. Pocket/price counters keep cash;
0.3.2 fixes their false change animation/sound after closing pockets.
Pocket Bells/Bell bags stay untouched. No cash fallback for insufficient savings.
Tailor tickets still work. Native savings cap/deposit clamping still applies.
Harv, other vendors, the drop-off box and ABD transfers retain normal behavior.
No new save fields, SD files, MSBT replacements or permissions are required.
Disable/remove Shopping Card and restart for a clean uninstall; completed
transactions remain ordinary saved money/item changes. Nook transactions were
confirmed in 0.3.1, the savings HUD in 0.3.4 and animation in 0.3.5; the
0.3.16 icon adjustments, Able Sisters and catalog checkout need live testing.
0.3.7 still showed cash outside Nook’s Cranny; 0.3.8 follows native catalog
open/close/destruction events independently of the shop or HUD’s UI context.
The latest catalog screenshot shows the savings presentation.

ASPECT RATIO
The default currency layout retains the native 1920x1080 (16:9) canvas and
original root/anchor placement. No 16:10-specific height shift is included.
Any future 16:10 placement override should be a separate optional layout file,
merged after updating the suite. Check in-game at 16:9 and 16:10 before release;
this package does not supply an aspect-ratio patch.

CORE SETTINGS / AMIIBO TIMEOUT
From 0.3.19, the amiibo timeout fix is built into Core and enabled by default.
Remove the old “QoL Enhancements - Amiibo” folder when updating; its amiibo.ini
and amiibo.custom.ini files are no longer read.
Core supplies romfs/qol-enhancements/config/core.ini with:
  amiibo_timeout_frames = 9000
This extends the shared event scan timeout to about 5 minutes at 30 updates
per second. Set it to 0 to install no timeout hook and retain native behavior.
Whole numbers from 0 to 65535 are supported (native unsigned 16-bit counter).
Invalid values and duplicate keys retain the previous value and log a warning.
Core always starts with 9000, then reads optional core.ini and core.custom.ini
in that order. Either file accepts the same settings; no enabled/version
metadata is required. Missing, unreadable or invalid files retain prior values.
The custom file works even if core.ini is absent. To keep personal settings
across suite updates, place core.custom.ini in your own mod at the same path.
Fully restart ACNH after editing settings. Core remains required for features
regardless of whether the timeout fix is disabled.

CONFIGURATION AND UPDATES
Each feature supplies romfs/qol-enhancements/config/<feature>.ini with:
  enabled = true
  version = 0.3.20
No configuration editing is needed for normal installation. Disable a feature
through Eden to remove BOTH its code activation and its resource replacements.
Setting enabled=false stops its code only; its RomFS resources remain loaded.

Replace entire suite folders when updating. Preserve any customized Pockets
captions from romfs/qol-enhancements/messages/pockets.ini before replacement.
The legacy optimized_ui.grab_five/grab_ten translation keys are retained.
Every module loads <feature>.ini first, then optional <feature>.custom.ini in
romfs/qol-enhancements/config/. Both files accept the same module settings.
Only keys included in the custom file override regular values; omitted keys
inherit them. This includes enabled and version. Both are required in the
regular file, optional in the custom file. Invalid/duplicate metadata rejects
that entire file; an invalid custom file retains the regular configuration.
An absent/invalid regular marker cannot be activated by a custom file.
Other modules currently have only enabled/version settings. Pockets captions
remain a separate translation resource, outside module configuration layering.
Whole-line #/; comments, a UTF-8 BOM, CRLF and valid UTF-8 text up to 4 KiB per file
are supported. Versions use numeric major.minor.patch.

FASTER CRAFTING / COOKING
New in 0.3.18. Install Core plus “QoL Enhancements - Faster Crafting”.
Enabled by default, independently of Faster Dialogue. The existing native
mash-A maximum (3x) is reached with a logarithmic ramp from 1x over 30
state updates, restarting for each crafting/cooking action (new in 0.3.20).
Speed is about 2.20x after 10 updates, 2.48x after 15, and 3x after 30.
Suspended/busy updates pause the native counter. This counts game updates,
not wall-clock time. No synthetic A presses, menu confirmations, skips or
recipe/item changes. Native animation gates and exit resets remain active.
This includes workbench customization through the shared crafting state.
NPC animations retain native behavior.
For ordinary speed with manual mash-A, set enabled = false in:
  romfs/qol-enhancements/config/faster-crafting.ini
Or preserve that preference across updates in a personal mod containing:
  romfs/qol-enhancements/config/faster-crafting.custom.ini
The custom file needs only enabled = false. Keep the regular module installed.
Fully restart the game after changing settings. Native ARM/config checks pass;
crafting, customization and cooking still need an Eden test.

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

NPC lists accept localized names shown above the dialogue bubble and internal
IDs, for special NPCs and villagers. Add Blathers in English, Thibou in French,
Sócrates in Spanish, or フータ in Japanese; owl works in every language.
Use the game's spelling, accents and punctuation. ASCII case is ignored;
non-ASCII case folding and Unicode normalization are not performed.
Timmy/Tommy/Orville/Wilbur/Kapp'n aliases still work across languages, mapping
to rcm/rct/dod/doc/kpp; Kapp'n also includes kppB. Name capture is new in 0.3.17
and needs a live test. Missing/invalid name labels leave ID matching usable.
This prototype accepts speed tiers 2/4.
Holding B/R selects boost_speed without stacking it on an automatic boost.
0.2.2 fixes an early window check that rejected recognized DIY/catch messages.
0.2.3 fixes NPC identity lookup by reading the speaker's owned entity handle.
The previous lookup incorrectly treated voice-position coordinates as a handle.
The user confirmed the corrected diagnostic build works in game.
Speeds preserve the game's language/FPS-adjusted fast rate; animations and
scripted waits can limit conversation speed. Native overrides take priority.
No preferences in either file: base 2, held 4, automatic UI/player/NPC boost off.

Keep the personal mod folder when replacing the seven suite folders. Do not
reinstall the example preset over your edited preferences. Enable only one
mod providing the custom file. Fully restart after changes. The custom file
may include enabled/version and can override a disabled regular configuration.
It cannot enable an absent module. Bad preference values and duplicate keys
warn and retain their earlier value; invalid list entries are ignored. A supplied
NPC list replaces the earlier list; an empty list clears it. Boost below base
is raised to base after both files are loaded. Up to 64 entries of 95 UTF-8 bytes are supported.
All five dialogue preferences also work in the regular faster-dialogue.ini;
keep personal edits in the custom file to preserve them across suite updates.

faster_ui covers blue system dialog windows. Independent faster_player covers
valid player speakers and the reviewed FTR_WorkingTable, SYS_Player,
SYS_Get_Fish, SYS_Get_Insect and SYS_Get_DiveFish banks. This includes DIY
prompts/results, recipe learning, catches and other SYS_Player self narration.
It reads the active message bank, so no speaker entity is needed. Known self
banks take priority over window color. Unknown banks/missing speakers alone
never imply player; known self banks also work in balloon windows.
Add faster_player = true to your existing personal INI and restart. Missing
faster_player defaults to false unless inherited from the regular file.
The player-window and NPC fixes were confirmed working by the user.
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
