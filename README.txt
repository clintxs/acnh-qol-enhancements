QoL Enhancements 0.4.23 — ACNH 3.0.3 — Eden integration test

CORE IS REQUIRED. Install and enable “QoL Enhancements - Core (REQUIRED)”
with any feature module. All packages in this ZIP belong to the same release.

Pockets consume test: ZR also supports Eat, Eat 1 and Drink. Hold/Wear/Put Away
take priority when available; consuming is the fallback. Eat 10 Turnips is not
included. The hint is now Use Item, editable via qol_pockets.hold_wear in the
Pockets messages INI (English fallback). Restart after editing captions.
Shopping Card: combined funds, Savings -> Pockets (Bell bags) -> Wallet.
Native 16:9 placement; display caps at 999,999,999 without limiting spending.

0.4.23: Shopping Card now pays Tom Nook’s storage-expansion fees from combined
funds, savings first. Includes the combined-balance display during this dialogue.

0.4.22: Fixed hold-Y hoovering for all seasonal weeds. Glowing moss has
its own category: weed holds skip moss, and moss holds skip weeds.

0.4.21: Swimming is now its own optional module. Boost propulsion starts 0.5s
into the arm stroke and fades over 1.5 seconds; stroke cooldown remains 2s.

INSTALL
Fully close ACNH. Disable the standalone Catalog Aware Shops, QoL Pockets
Interactions, Able Sisters Bag Prototype, all Faster Interactions versions,
Amiibo Scan - 5 Minutes, and the historical Optimized UI/Grab 5 patches.

For the full suite, copy all nine “QoL Enhancements” folders into the game's
mods folder, then enable them in Eden. For selected features, copy Core plus
only the feature folders you want. Restart the game after changing modules.

Core uses subsdk6, leaving subsdk9 available for Jubbsticks Additions.
This avoids that executable-slot collision; compatibility with its eventual
ACNH v3 release still needs testing. Other mods must also avoid subsdk6 and
our hook/layout files. Core supplies the suite's only main.npdm.

0.4.20: Borrowed traversal tools restore on landing, even if you release the
stick and remain still.

0.4.19: Controls automatic vaulting, handheld/placed climbing (including vines),
and silent tool borrowing/restoration are now enabled by default.

0.4.18: Catalog adds a star to undonated fish, bugs and sea creatures in catch
and item-name dialogue, pocket tooltips, home storage and ground item balloons.
Donations by any resident count. Catching a species does not remove its star;
donating it removes the star the next time its name is displayed. Install the
updated Core with Catalog enabled. Live Eden testing is still required.

0.4.16: Added hold_to_hoover=true to Controls. Set it to false for ordinary
field Y behavior, independently of hold_to_advance_dialogue. Restart after edits.

0.4.15: Hold-to-advance dialogue now belongs to Controls, with
hold_to_advance_dialogue=true by default. Install updated Core and Controls.
Faster Dialogue is optional: without it, releasing Y restores vanilla text
speed. Set the new key to false to keep hold-Y pickups without dialogue hurry.
Regular controls.ini settings load first, then controls.custom.ini overrides.
Choices, explicit input pauses and first-catch announcement stops are retained.

0.4.14: New optional Controls module: hold Y after picking up an item,
harvesting a crop or pulling a weed to repeat that action while moving.
The first successful action locks the category until release. Item pickup
cannot harvest crops or pull weeds; crop harvesting works across crop species.
An empty/unsupported/failed initial press does not arm it. Native targeting,
animations and capacity checks remain. Placed furniture, flowers, trees,
vines, fences, ladders and ground designs are excluded from repeats.
Install Core plus Controls and fully restart. Live Eden validation is pending.

0.4.13: Added Y / Set quantity to the cooking item 3D preview, including
the localized controller hint. X / Favorite matches the workbench preview.
Uses cooking’s native quantity selector; quantity wraparound is unchanged.

0.4.12: Y waits for manual advancement on the first-catch announcement
("Yes!" in English) for fish, bugs and sea creatures. Exact message IDs are
used across all locales. Text speed and subsequent catch messages still hurry.

0.4.11: Held Y also resumes explicit input pauses within a message, such as
the Sunday turnip-price dialogue, without needing a fresh button press.

0.4.10: Fix Y-to-Hurry input detection and allow completed NPC pages waiting
for input to advance. Choice menus retain their separate protection.

0.4.9: Preserve the recycling grid's native pause/resume during quick Claim,
so it restores focus after the claimed item becomes an empty slot.

0.4.8: Keep the hand cursor on the emptied recycling-box slot after quick Claim,
while still dismissing the old item's name balloon.

0.4.7: Hold Y during dialogue for nominal 30x text and automatic page advances.
Stops at choices/end; release Y to resume normal control. Text-only prototype:
no animation speedup or important/new-dialogue detection yet.

0.4.6: Dismiss the recycling item's name balloon after a successful quick Claim.

0.4.5: A automatically opens a pocket item if Open is its ONLY menu option.
Recycling-box selection also bypasses a sole Claim option. Native capacity
checks and full-pockets messages remain. Other one-option menus are unchanged.

0.4.4: Use native Drop Item and Hold labels for the Pockets trigger hints,
including the game's existing translations. Actions remain unchanged (Release,
Wear and Put Away still work). The same two INI keys allow optional overrides.
Remove older Drop / Release and Equip / Unequip overrides to use native labels.

0.4.3: ZR also selects native Put Away to unequip items; its hint now reads
Equip / Unequip. Custom footer captions now send native text-change
notifications so dependent background sizing can update. Trigger glyphs are
unchanged: controller-font mods may display L2/R2 instead of ZL/ZR.

0.4.2: Pockets trigger shortcuts: ZL selects native Release or Drop Item;
ZR selects Hold or Wear. Both bypass the item menu and do nothing when the
matching action is unavailable. Normal pockets show Drop / Release and
Hold / Wear controller hints; Y-to-storage remains available in your home.
The two new captions are editable in the Pockets messages INI (USen fallback).
Test the five-hint footer in Eden, especially with long localized captions.

0.4.1: Added A / Select to the DIY Recipes app item list when the DIY preview
is enabled. Uses the native localized caption; hides it while previewing.

0.4.0: Release test build. Automatic UI/unnamed dialogue boosting defaults to
true; crafting speed defaults to 6; dialogue speeds accept 1..30. Removed the
unreleased faster_player alias. Core logging defaults to versions/settings only,
with warnings and errors always retained. log_versions/log_settings/log_debug
are configurable in core.ini or core.custom.ini. More Eden testing is required
before public release. Replace Core and all installed feature folders together.

0.3.75: Fixed the B shortcut rejecting an extra flag in live controller input.
Removed temporary diagnostic logging. Update Core and Fitting Room, restart,
and retest B twice at the wear question.

0.3.74: Diagnostic for the B shortcut not activating in Eden. Reproduce with
clothes tried on, press B twice at the wear question, and collect eden_log.txt.
This adds bounded WEAR_BACK logging; it is not a confirmed fix.

0.3.73: Bag-only purchases skip the wear question after confirming payment.
Trying on any clothes keeps that question. Update Core and Fitting Room,
then restart; verify this shortcut in Eden.

0.3.72: At the fitting-room wear question, B moves to “No thanks.”; press B
again to select it. Holding B does not confirm. Update Core and Fitting Room,
then restart. Local input tests pass; verify the popup behavior in Eden.

0.3.71: Restored the fixed Bag hint and added action captions for all 16 locales.

0.3.67: Corrected the preview shortcut to Y / Set quantity. Favorite uses X
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
Controls: Hold Y to repeat the first item pickup, crop harvest or weed pull.
Optional hold-to-advance dialogue is enabled by default and works independently
of Faster Dialogue.
Crafting: DIY app 3D previews and configurable crafting/cooking speed ramp.
Catalog: catalog/recipe stars, undonated creature stars, catalog badges and furniture-variation markers.
Pockets: Grab 5/10, direct storage withdrawal, Y-to-storage and footer hints.
Faster Dialogue: faster default reveal; B/R boost; optional NPC/UI/unnamed boost.
Fitting Room: Y toggles a shopping bag; native bulk checkout includes bag items.
The footer always shows the localized Y: Bag action. Edit the
plain UTF-8 captions in the Fitting Room
folder at romfs/qol-enhancements/messages/fitting-room.ini, then restart ACNH.
Use [USen] with fitting_room.add_to_bag=Bag, or edit the supplied locale
sections. Missing translations fall back to USen, then built-in English.

Shopping Card: Tom Nook storage expansions also use combined funds, savings first.
The card balance appears during the storage payment dialogue. Other Tom Nook
fees retain native payment handling.

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
  version = 0.4.23
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
Catalog, Fitting Room and Shopping Card have only enabled/version settings.
Pockets captions
remain a separate translation resource, outside module configuration layering.
Whole-line #/; comments, a UTF-8 BOM, CRLF and valid UTF-8 text up to 4 KiB per file
are supported. Versions use numeric major.minor.patch.

Pockets settings (pockets.ini or pockets.custom.ini), all default true:
  bypass_menu_open_containers = true
  bypass_menu_claim_recycle_box = true
  bypass_menu_withdraw_home_storage = true
Set false to restore the normal menu for sole Open actions, recycling-box Claim,
or home-storage withdrawal respectively. Disabling withdrawal also restores the
Select hint. Other shortcuts are unaffected. Restart after changing settings.

CRAFTING MODULE / SETTINGS (0.3.63)
Install Core plus “QoL Enhancements - Crafting”. This replaces the former
“QoL Enhancements - Faster Crafting” folder; remove that old folder when updating.
Move any personal faster-crafting.custom.ini preferences to crafting.custom.ini.
Remove old version overrides in that custom file, or update them to 0.4.23.
Old faster-crafting.ini/custom paths are no longer read.
DIY app previews are now optional Crafting behavior, not always-on Core behavior.

Regular settings: romfs/qol-enhancements/config/crafting.ini
Personal overlay: romfs/qol-enhancements/config/crafting.custom.ini
Defaults in the regular file:
  enabled = true
  version = 0.4.23
  diy_app_preview = true
  workbench_preview_tilt = true
  crafting_quantity_wrap = true
  crafting_speed = 6

Active settings:
- workbench_preview_tilt: right-stick up/down tilt in workbench previews, up to
  60 degrees each way at the DIY preview movement speed. Defaults to true.
  Set false for vanilla controls; works independently of diy_app_preview.
  Wallpapers, floors, rugs and fences keep their native fixed poses.
- diy_app_preview: enable/disable the existing DIY app 3D preview independently.
- crafting_speed: whole-number 1 through 30; the peak of the existing logarithmic
  ramp over 60 state updates. Starts at 1x and restarts for each crafting,
  customization or cooking action. Default is 6; a setting of 3 preserves the
  confirmed curve. A setting of 30 is ten times vanilla’s 3x peak, and 1 holds at 1x. Values above 30 clamp to 30.
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

CONTROLS SETTINGS
The Controls module includes both hold-Y pickups and dialogue hurry.
romfs/qol-enhancements/config/controls.ini:
  enabled = true
  version = 0.4.23
  hold_to_advance_dialogue = true
  hold_to_hoover = true
  auto_vault = true
  auto_ladder = true
  auto_equip_traversal = true

Set hold_to_hoover=false for ordinary field Y behavior. The two hold-Y settings
default to true. All Controls settings support controls.custom.ini overrides.
Restart after editing.
Set hold_to_advance_dialogue=false to disable only dialogue hurry.
The same key works in controls.custom.ini, overriding the regular setting.

AUTOMATIC TRAVERSAL: Enabled by default. Push firmly into a vaultable river
or a climbable cliff face/edge through the normal hop delay. Controls silently
borrows a pocket pole/ladder, traverses, then restores your previous tool or
returns to empty hands. Placed ladders and vines need no pocket ladder.
Ordinary narrow hops keep priority. Pole, uphill/downhill ladder, silent tool
restoration and placed climbing were confirmed working in Eden.

Set auto_vault=false to disable automatic pole vaulting. Set auto_ladder=false
to disable automatic handheld and placed climbing. Set auto_equip_traversal=false
to require an already equipped tool; placed ladders and vines still work.
Manual tool selection cancels restoration. Existing controls.custom.ini values
still override the defaults. Restart after editing.

Y TO HURRY PROTOTYPE: With Controls enabled and hold_to_advance_dialogue=true,
hold Y for nominal 30x text and automatic completed-page advancement. Choices open normally; Y never
selects an answer. Release Y to stop; keep holding after making a choice to
hurry the next message. Fish/bug catch bubbles are included.
Vanilla dialogue speed resumes when Y is released, or your Faster Dialogue
base/boost/category preferences if that optional module is installed.
Native timed waits, text protection and explicit speed overrides remain.
First-catch announcements wait for manual advancement, even while Y remains
held: SYS_Get_Fish:001, SYS_Get_Insect:001, SYS_Get_DiveFish:001. These IDs are
shared by all 14 supplied locales; their translated text is not matched.
Animations are unchanged. Other important/renovation/collection-completion
announcements are not detected and will still hurry. Live Eden testing of the
Controls migration is required; the first-catch stop is confirmed in game.

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
Both speed keys accept whole numbers 1 through 30; defaults remain 2/4.
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
No preferences in either file: base 2, held 4, automatic UI/unnamed boost on,
and an empty NPC list.

Keep the personal mod folder when replacing the nine suite folders. Do not
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
faster_ui and faster_unnamed default to true. Set either to false in your
personal INI to disable that automatic category. The broader unnamed classifier
needs a live test with the reported storage/mirror/Niko/amiibo/stretching/drop-off messages.
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

CORE LOGGING

Core settings: romfs/qol-enhancements/config/core.ini
Personal overrides: romfs/qol-enhancements/config/core.custom.ini
Both files are optional. Core has no enabled/version metadata. Restart after edits.

Defaults:
  log_versions = true
  log_settings = true
  log_debug = false

All three accept true/false. log_versions controls Core and installed module
version summaries. log_settings controls the final settings after both INI
layers, including disabled-module preferences and the Faster Dialogue NPC list.
log_debug enables installation and gameplay diagnostics. Warnings and errors
are always logged, even with all three switches off. Default output contains
only versions/settings and warnings/errors; absent optional modules are quiet.

Preferences load after RomFS mounts, so early startup chatter stays suppressed.
Early startup errors still log. Special dialogue MESSAGE/SELECT tracing needs
the diagnostic build and log_debug=true in core.custom.ini. Trace limits remain.
Invalid values inherit; duplicate keys restore the value from before that file.

Swimming module / auto_swim (prototype 12): enabled by default at the surface and
underwater. Stick travel gradually controls speed beyond a small neutral dead
zone. Starting movement requests one opening arm stroke. Press A for a
boost with a roughly two-second cooldown; hold A to repeat when ready. Further
arm strokes require A. Each boost starts half a second into the arm stroke,
adds 62.5% of full cruise speed and then fades over 1.5 seconds. Ready strokes interrupt the kick animation, with a two-second
cooldown measured from actual arm entry. Quiet kicking and smooth cruise
continue between strokes.
Stopping/restarting or diving/surfacing does not bypass the cooldown. Y keeps
priority for diving/surfacing/catching. Set auto_swim = false in swimming.ini
or swimming.custom.ini and restart for ordinary swimming movement. In-game validation
of this prototype is pending.

Swimming is independent of Controls. Install Core plus the Swimming folder to
use the overhaul. Disable/remove Swimming, or set enabled=false in swimming.ini
or swimming.custom.ini, to use vanilla swimming. Move any personal auto_swim
setting from controls.custom.ini to swimming.custom.ini. Controls no longer
owns this setting. Swimming defaults to enabled=true and auto_swim=true.

camera_tilt_while_swimming=true is also enabled by default in Swimming. The
right stick selects the normal three tilt positions while swimming forward.
This works independently of auto_swim. Set camera_tilt_while_swimming=false
in swimming.ini or swimming.custom.ini and restart for the native camera
restriction. Other camera locks remain active.
