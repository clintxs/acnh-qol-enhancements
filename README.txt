QoL Enhancements 0.3.69 — ACNH 3.0.3 — Eden integration test

CORE IS REQUIRED. Install and enable “QoL Enhancements - Core (REQUIRED)”
with any feature module. All packages in this ZIP belong to the same release.
Shopping Card: combined funds, Savings -> Pockets (Bell bags) -> Wallet.
Native 16:9 placement; display caps at 999,999,999 without limiting spending.

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

0.3.69: Corrected the preview shortcut to Y / Set quantity. Favorite uses X
in the workbench preview; recipe-grid controls keep their original bindings.

0.3.66: Press X / Set quantity from the workbench 3D item preview to open
Craft Multiple directly. The existing localized hint appears for supported
recipes with enough materials. B returns to the item preview.

0.3.65: Craft Multiple wraparound uses the native number transition and the
up/down sound for the pressed direction. The 300 ms pause and held-input
stopping behavior are retained for crafting and cooking.

0.3.64: With Catalog enabled, a crafting/cooking villager's current item name
gets a star only when you have not learned their recipe. Other item dialogue
keeps catalog ownership checks. This needs an in-game test.

MODULES
Crafting: DIY app 3D previews and configurable crafting/cooking speed ramp.
Catalog: catalog/recipe stars, catalog badges and furniture-variation markers.
Pockets: Grab 5/10, direct storage withdrawal, Y-to-storage and footer hints.
Faster Dialogue: faster default reveal; B/R boost; optional NPC/UI/unnamed boost.
Fitting Room: Y toggles a shopping bag; native bulk checkout includes bag items.
The footer shows Y: Add to bag. Edit its plain UTF-8 caption in the Fitting Room
folder at romfs/qol-enhancements/messages/fitting-room.ini, then restart ACNH.
Use [USen] and fitting_room.add_to_bag=Add to bag, or add Pockets-style locale
sections. Missing translations fall back to USen, then built-in English.
Shopping Card: Nook's Cranny/Able Sisters purchases use combined funds; Nook's
sales deposit to savings and use the ordinary “Thank you! Please come again!”
message. 0.3.3 removes the misleading pockets-full/wiring explanation. Includes
fitting checkout and the optional fitting bag. Nook Shopping/catalog orders for
yourself use combined funds at Nook Stop and through the phone app. Catalog gift
orders have a separate native payment path and are not covered in this prototype.
Always active when this module is enabled. No item, menu addition or unlock yet.
The top-right display shows a green shopping card and combined balance
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
Displayed balance changes use native count-up/down animation and sound.
The display caps at 999,999,999. The full total remains available for purchases.
The hidden native cash counter cannot duplicate the card animation/sound.
Entering shops, switching residents and unchanged balances are silent.
0.3.4 corrects the HUD path missed in 0.3.2–0.3.3, which
left the regular in-shop counter showing cash. Pocket/price counters keep cash;
0.3.2 fixes their false change animation/sound after closing pockets.
Purchases check savings + pocket Bell-bag items + wallet Bells before payment.
Payment priority is Savings -> Pockets (Bell bags) -> Wallet. If the combined
total is insufficient, no source is charged. For a 500,000-Bell price, 300,000
savings + 200,000 in bags + 99,000 wallet leaves the wallet at 99,000 and the
other two sources empty. Native denomination change may repack remaining cash
between the wallet and ordinary Bell bags. Space checks conservatively do not
assume spending bags will free slots; change can occupy those slots again.
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
DIY RECIPE PREVIEWS (CRAFTING)

0.3.44's right-stick turning and tilting are confirmed working in-game.
0.3.45 locks wallpapers, floors, rugs and fences to the workbench renderer's
native poses. These item types ignore right-stick movement. Other items turn
and tilt twice as fast; the dead zone and 60-degree tilt limit are unchanged.
Reopening resets the pose. The accepted cream background is unchanged.

Local checks cover real item categories, native pose/matrix preservation,
fixed-to-rotating transitions, B cleanup and doubled movement speed.
The user accepts 0.3.45. The module move/settings need a Steam Deck test.

The DIY app keeps its existing materials panel. Press A on a recipe to open the
3D preview in place of the grid; press B to return to the same grid. Category,
selection and scroll position are retained. This view has no crafting button
or crafting action. In preview mode, left/right on the D-pad or left stick moves
through the current sorted/filtered recipe list, across rows and scrolling pages.
Up/down leaves the recipe selection alone. Holding repeats after a short delay; navigation stops at the first/last recipe.
Materials follow the model. B returns to the grid with the current card selected
and visible. Previews start from the game's item-specific workbench orientation.
Changing recipes or reopening resets rotation/tilt to that pose; fixed types
retain their native framing.

Crafting adds a small capture surface to the loaded MenuRecipe layout in memory,
preserving existing panes and installed layout/aspect-ratio edits. No workbench
layout or additional feature folder is used. If the capture binding or renderer
is unavailable, the original grid remains usable. Crafting and Pockets share Core’s
page-input hook so Quick Store can remain enabled.

Local ARM checks reproduce the native animation overwrite, verify the changed
drawable matrix and exercise native layer color conversion and clearing. These
do not establish live GPU output. Fully restart when updating.

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
  version = 0.3.63
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
Modules other than Faster Dialogue and Crafting have only enabled/version
settings. Pockets captions
remain a separate translation resource, outside module configuration layering.
Whole-line #/; comments, a UTF-8 BOM, CRLF and valid UTF-8 text up to 4 KiB per file
are supported. Versions use numeric major.minor.patch.

CRAFTING MODULE / SETTINGS (0.3.63)
Install Core plus “QoL Enhancements - Crafting”. This replaces the former
“QoL Enhancements - Faster Crafting” folder; remove that old folder when updating.
Move any personal faster-crafting.custom.ini preferences to crafting.custom.ini.
Remove old version overrides in that custom file, or update them to 0.3.63.
Old faster-crafting.ini/custom paths are no longer read.
DIY app previews are now optional Crafting behavior, not always-on Core behavior.

Regular settings: romfs/qol-enhancements/config/crafting.ini
Personal overlay: romfs/qol-enhancements/config/crafting.custom.ini
Defaults in the regular file:
  enabled = true
  version = 0.3.63
  diy_app_preview = true
  workbench_preview_tilt = true
  crafting_quantity_wrap = true
  crafting_speed = 3

Active settings:
- workbench_preview_tilt: right-stick up/down tilt in workbench previews, up to
  60 degrees each way at the DIY preview movement speed. Defaults to true.
  Set false for vanilla controls; works independently of diy_app_preview.
  Wallpapers, floors, rugs and fences keep their native fixed poses.
- diy_app_preview: enable/disable the existing DIY app 3D preview independently.
- crafting_speed: whole-number 1 through 30; the peak of the existing logarithmic
  ramp over 60 state updates. Starts at 1x and restarts for each crafting,
  customization or cooking action. Default 3 preserves the confirmed curve;
  30 is ten times vanilla’s 3x peak, and 1 holds at 1x. Values above 30 clamp to 30.
  Zero and malformed values retain the prior setting and report a warning.
  This scales the working animation; dialogue/transitions retain native timing.
  Short actions may finish before the ramp reaches its peak.

- crafting_quantity_wrap: defaults to true. Fresh down at 1 wraps to 10; fresh
  up at 10 wraps to 1 (or the native maximum when lower). Holding stops at the
  endpoint; release before pressing again to wrap. Supports the left stick and
  d-pad in crafting and cooking. A fresh endpoint wrap pauses scrolling for
  300 ms, then resumes if the direction stays held. Releasing during the pause
  allows another press normally. Return the stick to center to release it.
  Wraps use the native number slide and the directional up/down sound.
  Set false for vanilla quantity controls.

DIY right-stick horizontal rotation now matches the workbench. Workbench
previews retain their displayed tilt during recipe switches to prevent flicker.
Hotel request banners draw over the DIY preview’s bottom-right corner,
returning to their native placement in the grid. During recipe switches, the
outgoing DIY item retains its displayed pose until the replacement is framed.

The regular marker must exist; a custom file alone cannot enable Crafting.
Omitted custom values inherit the regular settings. Setting enabled=false
turns off previews, workbench tilt, quantity wrap and speed behavior; removing Crafting restores the
ordinary DIY app. Core's amiibo setting is independent and remains in Core.

The ramp counts game updates, not wall-clock time. Suspended/busy updates pause
its native counter. No synthetic A presses, menu confirmations, skips, recipe
or item changes. Native animation readiness gates and exit resets remain active.
NPC animations retain native behavior. Fully restart after updating or editing
settings. Local ARM/config checks cover the new module and settings; live Deck
confirmation of this reorganization is still required.

PERSONAL FASTER DIALOGUE SETTINGS
Install the optional separate faster-dialogue-custom-example ZIP alongside
Core and Faster Dialogue to try the requested preset: base 2x, B/R 4x, automatic
4x for Timmy, Tommy, Orville, Wilbur, Kapp'n and UI dialogs and unnamed gameplay messages.
The personal mod contains:
  romfs/qol-enhancements/config/faster-dialogue.custom.ini

Edit this file to customize:
  base_speed = 2
  boost_speed = 4
  faster_ui = true
  faster_unnamed = true
  faster_npc_list = Timmy, Tommy, Orville, Wilbur, Kapp'n

NPC lists accept localized names shown above the dialogue bubble and internal
IDs, for special NPCs and villagers. Add Blathers in English, Thibou in French,
Sócrates in Spanish, or フータ in Japanese; owl works in every language.
Use the game's spelling, accents and punctuation. ASCII case is ignored;
non-ASCII case folding and Unicode normalization are not performed.
Timmy/Tommy/Orville/Wilbur/Kapp'n aliases still work across languages, mapping
to rcm/rct/dod/doc/kpp; Kapp'n also includes kppB. Name capture is new in 0.3.17
and needs a live test. Missing/invalid name labels leave ID matching usable.
Both speed keys accept whole numbers 1 through 10; defaults remain 2/4.
The old parser accepted only 2/4: 8 was rejected and inherited the earlier
boost (normally 4). Version 0.3.62 applies each selected speed to both native
text-rate paths. For a stronger comparison, try base_speed=4, boost_speed=8.
Holding B/R selects boost_speed without stacking it on an automatic boost.
0.2.2 fixes an early window check that rejected recognized DIY/catch messages.
0.2.3 fixes NPC identity lookup by reading the speaker's owned entity handle.
The previous lookup incorrectly treated voice-position coordinates as a handle.
The user confirmed the corrected diagnostic build works in game.
Speeds preserve the game's language/FPS-adjusted fast rate; animations and
scripted waits can limit conversation speed. Native overrides take priority.
No preferences in either file: base 2, held 4, automatic UI/unnamed/NPC boost off.

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

faster_ui covers blue system dialog windows. Independent faster_unnamed covers
regular gameplay bubbles whose current native name preparation confirms there
is no displayed name. This includes player thoughts and other unnamed messages,
regardless of their English wording, inserted item names or message group.
The observed result belongs to the current message bank/label, context and
speaker handle; an empty cached name or failed text conversion is not enough.
Named speakers keep NPC matching. Unknown names retain base speed unless a
legacy fallback applies: valid player speakers or the reviewed FTR_WorkingTable,
SYS_Player, SYS_Get_Fish, SYS_Get_Insect and SYS_Get_DiveFish banks.
A confirmed name overrides those fallbacks. With unknown name metadata,
reviewed self banks retain their previous priority over window color.
Add faster_unnamed = true to your personal INI and restart. It defaults to false
unless inherited. faster_player remains an alias for old personal files.
Use one spelling per file; both in the same file count as a duplicate and retain
that setting's earlier-layer value. Either spelling can override the other in
a later file. The broader unnamed classifier is new in 0.3.22 and needs a live
test with the reported storage/mirror/Niko/amiibo/stretching/drop-off messages.
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

FITTING ROOM RENDERING FIX (0.3.60)
Replace Core AND Fitting Room, then fully restart ACNH. The basket now uses
one picture instead of 70 rectangles. Core supplies the required extra layout
allocation. Bag capacity, prices, delivery and icon animation are unchanged.
Please retest adding more than 16 items and scrolling between clothing rows.
