# QoL Enhancements

**0.4.3 release test build** for ACNH **3.0.3 on Eden**. One Core
executable in **subsdk6** activates six optional RomFS feature packages. The
standalone source projects/releases remain intact. Read [README.txt](README.txt)
for installation, version policy and user-facing limits.

This build includes the revised defaults: automatic UI/unnamed dialogue boost
is on, crafting speed defaults to 6, and dialogue speed settings accept 1..30.
Core logging defaults to version/settings summaries with warnings and errors
always visible. Live Eden testing is still required before public release.

The DIY Recipes app now shows **A / Select** while browsing recipes with
Crafting’s DIY preview enabled. It uses the existing localized caption and
hides the hint while a preview is open.

## Included implementations

| Package | Source baseline | Resources |
|---|---|---|
| Catalog | Catalog Aware Shops 0.8.1 | Four catalog row layouts |
| Pockets | QoL Pockets Interactions 0.1.4 | Editable Grab 5/10 captions |
| Faster Dialogue | C++ hook rewrite of Faster Interactions 0.3.0, with conditional NPC/UI boost | Optional personal configuration |
| Fitting Room | Able Sisters Bag prototype 0.3.7 | Fitting row badge and original counter layout |
| Crafting | DIY/workbench previews, quantity wrap + configurable crafting/cooking speed | Configuration only |
| Shopping Card | Combined savings/cash routing prototype | Configuration + card currency layout |

Faster tool-wheel animations and other workspace mods are not bundled in this
six-feature suite. Historical source notes under `docs/` describe each
baseline; their old installation/packaging instructions refer to standalone mods.

Core includes the amiibo timeout fix. DIY Recipes app previews now belong to
the optional Crafting module, alongside the former Faster Crafting feature.

Development releases advance numerically (for example, 0.9.0 → 0.10.0).
Version 1.0.0 is reserved for the initial release.

## Architecture

`source/program/bootstrap.cpp` validates the original content-mount call and
continuation, then installs one SIMD-preserving discovery hook at main+037B5464.
Only after the game's `nn::fs::MountRom("content", ...)` call does Core read the
six known configuration paths and install enabled features. Activation is
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

Faster Dialogue now lives in `source/program/dialogue/`: seven SIMD-preserving
C++ hooks implement current B/R input, eligible speed selection/forwarding, and
main/renderer multipliers. The active speaker is resolved through its owned
entity handle, validated against the live registry. NPC resource IDs and the
resolved localized name tag are both accepted. The name copy is cleared for every label preparation and tied to its manager, source
and entity generation; the live entity still passes the existing identity checks.
Native restrictions and scripted speed overrides remain in force. No binary replacement array or game
code-cave helper remains. Amiibo's existing 9,000-update timeout is likewise a
C++ hook; its behavior is unchanged. Expected instruction words are validation
fingerprints only, checked before each feature's first hook.

## Crafting module

Install **QoL Enhancements - Crafting** with Core. This replaces the former
Faster Crafting package; remove the old folder and rename personal
`faster-crafting.custom.ini` preferences to `crafting.custom.ini`. Remove any old
`version` override from that personal file (or update it to 0.4.3). The retired
regular/custom paths are no longer read. Core alone no longer enables DIY previews.

`romfs/qol-enhancements/config/crafting.ini` supplies these defaults:

```ini
enabled = true
version = 0.4.3
diy_app_preview = true
workbench_preview_tilt = true
crafting_quantity_wrap = true
crafting_speed = 6
```

`diy_app_preview` controls the existing DIY app preview independently of speed.
`crafting_speed` accepts whole-number peaks **1 through 30** for the existing
60-update logarithmic ramp, restarting at 1x each action. Values above 30 clamp
to 30. The default is 6; a setting of 3 preserves the previous curve exactly.
A setting of 30 gives a peak ten times vanilla’s 3x peak, and 1 keeps it at 1x. This scales the working animation;
fixed dialogue and transition time remain. A short action may end before the ramp
reaches its peak. Zero and malformed values retain the prior setting.
Crafting, customization and cooking share this setting. Native readiness,
suspension and exit/reset behavior remains intact.

`workbench_preview_tilt=true` adds right-stick up/down tilt to workbench previews,
up to 60 degrees each way at the DIY preview's movement speed. Set it to `false`
to retain vanilla workbench controls. Wallpapers, floors, rugs and fences keep
their native fixed poses. This setting works independently of `diy_app_preview`.
`crafting_quantity_wrap=true` lets a fresh down press at quantity 1 scroll to 10,
and a fresh up press at 10 scroll to 1 (or the native maximum when lower). Hold
either direction to reach an endpoint and stop; release before pressing again
to wrap. A fresh endpoint wrap pauses scrolling for 300 ms, then resumes if
you keep holding that direction. Releasing during the pause allows another
press normally. Return the stick to center; easing its pressure does not count
as a release. Applies to the left stick and d-pad in crafting and cooking selectors.
Wraps use the native number slide and the up/down sound for the pressed direction.
Set it to `false` for vanilla quantity controls.
Hotel request banners now draw over the bottom-right corner of the DIY preview,
returning to their native placement in the grid. DIY previews start from the
game's item-specific workbench orientation, with right-stick rotation matching the workbench horizontally and tilt
relative to that pose. Changing recipes or reopening restores the native pose. During recipe changes,
the outgoing item retains its displayed pose until its replacement is framed.

A personal `crafting.custom.ini` may override only the desired keys; omitted or
invalid preferences inherit the regular file. Duplicate keys retain the value
from before that file. Crafting's regular marker must be installed and compatible;
`enabled=false` disables its previews, tilt, quantity wrap, direct quantity
shortcut and speed hooks together. Core remains required. Fully restart after changing module installation or settings.

From the workbench’s large 3D item preview, press **Y / Set quantity** to
open Craft Multiple directly. The hint uses the existing localized label and
appears for recipes that support multiple crafting and have enough materials.
**X / Favorite** toggles the recipe favorite in this preview; grid controls
keep their original bindings. B returns to the item preview. This shortcut is included whenever Crafting is
enabled, independently of `crafting_quantity_wrap`.

## DIY Recipes app previews (Crafting)

The user confirmed visible right-stick turning and tilting in 0.3.44 and
accepted the cream background. 0.3.45 preserves the native workbench renderer's
poses for wallpapers, floors, rugs and fences; right-stick movement is disabled
for those item types. Other items turn and tilt twice as fast (6° per update at
full stick), retaining the dead zone and 60° tilt limit. Reopening resets the pose.

Local checks use real ItemParam rows and execute compiled input/renderer hooks,
checking native matrix preservation, fixed-to-rotating transitions, B cleanup
and the doubled speed. The user accepted 0.3.45. The module move and new
settings need a Deck test.

The existing DIY app keeps its materials panel, category, selection and scroll
position. A opens the 3D preview in place of the grid; B releases the model and
restores the grid. There is no crafting button or crafting action. Left/right on
the D-pad or left stick moves to the previous/next recipe in the current sorted
or filtered list, crossing row and page boundaries. Up/down leaves the recipe
selection alone. Holding repeats after a short delay; movement stops at either end of the list. The material panel updates with
the model, and B returns to the grid with that recipe selected and visible.
Changing recipes resets rotation/tilt and preserves fixed poses for locked types.

Crafting adds a small capture surface to the **loaded MenuRecipe layout in memory**.
Existing panes, animations and material/texture indices are preserved, including
installed aspect-ratio edits. No workbench layout or extra RomFS file is used.
If the capture binding or renderer is unavailable, the ordinary grid stays usable.
Crafting and Pockets share Core’s page-input hook, preserving Quick Store behavior.

Compiled ARM checks cover native transform/clear behavior under explicitly
modeled scheduling, input scope, B and restoration. Diagnostics are checked for
both rejected and successful updates and stop observing before release. Those
checks do not establish the DIY app's real rendering schedule or GPU output.
See `research/recipe-preview/NOTES.md` and `VALIDATION.md`.

The Exlaunch SIMD context declaration now matches its assembly stack layout;
compile-time assertions and execution of the real adapter cover that correction.
Ordinary GP-only callbacks retain their original context layout.

## Shopping Card prototype

Enable **Core** and **Shopping Card**, then fully restart ACNH. The card is
always active inside both versions of Nook's Cranny and Able Sisters, and for
Nook Shopping/catalog orders for yourself at Nook Stop or in the phone app. Purchases
check the current player's combined ABD savings, pocket Bell-bag items and wallet
Bells. If that total covers the price, payment uses **Savings → Pockets (Bell
bags) → Wallet**. A 500,000-Bell purchase with 300,000 savings, 200,000 in bags
and 99,000 in the wallet leaves the wallet at 99,000 and empties the other two
sources. The game’s native cash routines handle denomination change and may
repack remaining cash between the wallet and ordinary Bell bags. Nook's
counter sales go directly to
savings and use the ordinary localized “Thank you! Please come again!” sale
message. Version 0.3.3 replaces the wire message, which incorrectly explained
that the player’s wallet and pockets were full. Native savings limits
apply (999,999,999 Bells, including the usual deposit clamping at the cap).
An insufficient combined total leaves all three sources unchanged. Existing
affordability/error text may still refer to pocket Bells.

This includes native fitting-room purchases with or without the Fitting Room
module. When Fitting Room is enabled, its bag preflight reads the same account
as native checkout. Tailor tickets retain their normal discounts and use.
Native item delivery, cancellation, purchase records and milestones remain in
the original transaction routines. Space checks no longer assume spending
Bell bags frees pocket slots. Harv's Island, other vendors, the drop-off box,
ABD transfers and ordinary pocket-money actions retain their existing paths.

The top-right currency display uses a green shopping card and a separate
combined balance inside the two shops and while the catalog is open. The display
saturates at **999,999,999**, while purchases still use the full combined funds;
the prototype label is English (“Bells”), on a white badge with green text.
The green oval has a thick white rim. The centered card has a dark edge and
a white backing behind the oval; the white Bells badge sits at the lower right.
0.3.10 restored the compact layout after 0.3.9 made both savings and
ordinary cash counters disappear; the user confirmed the recovery works.
0.3.11 replaced the stepped corners with native rounded geometry but rendered
all three card silhouettes white: this drawing path skips pane vertex colors.
0.3.12 corrected those material colors, but its live screenshot still shows
split, distorted geometry. 0.3.13 replaces that unused shape path with two
ordinary textured pictures, using the native Bells icon's material and UV
settings. One foreground image contains the entire green card, dark border,
stripe, chip and details; its white backing remains behind the oval. Smooth
corner coverage is baked into the textures. The component is 24 panes/63 sections.
Original cash texture pixels/settings, panes, materials, fonts and animations
are preserved. Local checks decode the packaged image byte order and execute
native texture-name lookup. The user reported the entire counter disappeared in 0.3.13. Its expanded
archive is 463,528 bytes, exceeding the native RSTC allowance of 77,952 bytes.
0.3.14 raises only this counter's runtime allocation floor to 1 MiB, preserving
larger overrides and other resource lookups. This support remains active when
the package is installed but its behavior is disabled, so ordinary cash can
still load. Removing the package removes this support on restart. No shared
RSTC replacement is shipped. Native size lookup and SDK relocation now have
regression coverage. The user confirmed 0.3.14 works. 0.3.16 moves the two
card-detail lines up another 1.5 layout units (one screen pixel at 1280 width)
and makes the border a visibly lighter blue-green after the previous shade still
appeared black in-game.
Its final appearance still needs an Eden check.
The badge remains shifted left by about 16 screen pixels at 1280-pixel width.
Displayed balance changes count up/down with the native increase/decrease/finish animation
and sound feedback. First display, resident/room switches and unchanged balances
stay silent. Outside those shops, closing the catalog returns to the ordinary Bells presentation. Pockets and other shared cash counters keep
showing pocket Bells. The 0.3.2 change removes savings substitution from their
refresh paths, which caused a false money animation/sound after closing pockets.
The two card textures are appended to the embedded texture container; no original
texture pixels or fonts are replaced.
The default package preserves the original 1920×1080 (16:9) canvas, root and
anchor transforms. No 16:10-specific offset is bundled. Any future adjustment
for a 16:10 mod should be a separate optional replacement of this layout, merged
after suite updates; the counter must be checked in both aspect ratios before release.
0.3.4 connects the savings display to the regular in-shop Bells HUD. The
0.3.2–0.3.3 display hook covered an auxiliary counter and left that HUD showing
cash. Both controller paths now select the same savings subtree, while their
native cash refreshes stay consistent to avoid the pocket-close animation.

No new item, save flag, message resource, SD file, filesystem permission or
menu entry is added. Disabling/removing Shopping Card and restarting restores
normal transaction routing; ordinary money/item changes from completed
transactions remain saved. This prototype has local ARM regression coverage,
including native encrypted savings writes. The user confirmed Nook’s Cranny
transactions in 0.3.1, the corrected savings HUD in 0.3.4 and the balance
animation in 0.3.5. Able Sisters still needs an Eden playtest. Version 0.3.7 added regular catalog
checkout, but the user reported its HUD still showed cash outside Nook’s Cranny.
0.3.8 follows the catalog’s native opening, closing and destruction events
instead of assuming it shares the HUD’s UI context. This display correction
was followed by a catalog screenshot showing the savings presentation.
Catalog checkout and the 0.3.16 icon adjustments still need live verification. Catalog gift orders use a separate
native sending/payment path and are not covered by this prototype; their
payment remains unchanged. Nook Stop and phone catalog purchases, insufficient
combined funds, cancellation, order limits, delivery and screen transitions need live
verification.
See [research and follow-up scope](research/shopping-card/NOTES.md).

## Core logging

Edit `romfs/qol-enhancements/config/core.ini`, or keep personal preferences in
`core.custom.ini` alongside it. Both files are optional; restart ACNH after edits.
Core has no `enabled` or `version` keys.

```ini
amiibo_timeout_frames = 9000
log_versions = true
log_settings = true
log_debug = false
```

`log_versions` controls Core and installed module version summaries.
`log_settings` controls effective settings after regular/custom overlays, including
settings for disabled modules and the configured Faster Dialogue NPC list.
`log_debug` enables detailed installation, resource, input, purchase and gameplay
traces. All three switches accept `true` or `false`. Warnings, compatibility or
hook errors, caption fallbacks and partial-startup failures are always logged,
even when all three switches are false. The default output contains only version
and settings summaries plus warnings/errors. Missing optional modules are quiet.

Core loads logging preferences after RomFS mounts. Early framework startup and
mount-wait chatter are suppressed before that point; startup failures still log.
Special dialogue MESSAGE/SELECT traces still require the diagnostic build as
well as `log_debug = true`. Existing trace limits remain in effect.

Logging preferences use the same defaults → regular INI → custom INI order as
the amiibo timeout. Invalid values inherit; duplicate keys restore the value
from before that file. Core files are limited to 4 KiB of valid UTF-8.

## Shared configuration overlays

Every module loads `content:/qol-enhancements/config/<slug>.ini` first, then an
optional `<slug>.custom.ini` in the same directory. This is Core behavior, so
future module settings inherit the convention by registering a parser rather
than implementing another file loader. Both files support the same keys for
that module. Defaults apply first; regular values replace defaults; custom values
replace only the keys supplied. `enabled` and `version` follow the same rule.
The regular installation marker must exist and contain valid `enabled` and
`version` keys; the custom file may omit either or both. An absent/invalid regular
file cannot be rescued by a custom file. Setting `enabled=false` overrides code
activation only; Eden still controls resource replacements through mod toggles.

For example, `faster-dialogue.ini` can contain:

```ini
enabled = true
version = 0.4.3
base_speed = 2
boost_speed = 4
faster_ui = true
faster_unnamed = true
```

A `faster-dialogue.custom.ini` containing just `faster_ui = false` keeps all the
other values and overrides that one setting. An NPC list replaces the previous
list; an empty list clears it. Custom files are read once per restart, even if
regular `enabled` is false, so a custom `enabled=true` can re-enable installed
code. An unreadable, oversized or invalid metadata overlay leaves the regular
configuration intact. Neither malformed metadata nor duplicate metadata is
partially applied. Each file is limited to 4 KiB of valid UTF-8 text, with whole-line
comments, CRLF and an optional UTF-8 BOM.

Catalog, Pockets, Fitting Room and Shopping Card expose just `enabled` and
`version`. Their overlays are `catalog.custom.ini`, `pockets.custom.ini`,
`fitting-room.custom.ini` and `shopping-card.custom.ini`. Crafting preferences
use `crafting.custom.ini`; Core uses the optional settings files described above.
Pockets and Fitting Room caption files are separate translation resources.

## Conditional Faster Dialogue

All five preferences below work in either Faster Dialogue INI. Personal
preferences are best kept in a separate mod so suite updates cannot replace them.
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
faster_unnamed = true
faster_npc_list = Timmy, Tommy, Orville, Wilbur, Kapp'n
```

Enable that personal mod alongside Core and Faster Dialogue. Keep it when
replacing the suite folders, and restart after editing. Do not reinstall the
example preset over edited preferences. Only one mod should supply the custom
path; normal RomFS precedence applies if there are duplicates. The custom file
cannot activate an absent Faster Dialogue module.

Preferences absent from both files retain base 2 / held B/R 4, with UI/unnamed
boost on and an empty NPC list. A listed NPC, confirmed unnamed message or UI uses `boost_speed` automatically; holding B/R selects that
same tier without stacking. Both speed keys accept **whole numbers 1 through 30**;
a boost below the base is raised to the base. The defaults remain 2/4. The
original fast rate represents nominal 2x; both
main and renderer rate hooks multiply it by the selected speed divided by two.
These are nominal tiers relative to the live native fast rate,
including language/FPS adjustments, not fixed wall-clock multipliers. Native
scripted waits, protected messages and nonnegative overrides retain priority.
Earlier versions rejected 8 and retained the earlier boost (normally 4), so
4/8 loaded as 4/4. On 0.3.62, try `base_speed = 4` and `boost_speed = 8` in your
existing personal file, fully restart, and compare an unlisted named NPC with
and without B/R. Automatically boosted categories already use the boost speed.

The list now accepts **the localized name shown above the dialogue bubble**, or
an internal resource ID, for special NPCs and villagers. For example, in English:

```ini
faster_npc_list = Timmy, Tommy, Orville, Wilbur, Kapp'n, Blathers
```

`Blathers` follows the current language: use `Thibou` in French, `Sócrates` in
Spanish, or `フータ` in Japanese. The ID `owl` works in any language. Existing
friendly aliases for Timmy/Tommy/Orville/Wilbur/Kapp'n remain available across
languages; Kapp'n still includes its known `kppB` profile. Other entries match the
resolved name or ID exactly, ignoring ASCII letter case. Preserve non-ASCII
spelling, accents and punctuation as shown in game; Unicode normalization and
non-ASCII case folding are not performed. Up to 64 entries of 95 UTF-8 bytes
are accepted. Unknown names simply never match. Native name tags are copied
once when prepared, with formatting/grammar metadata removed; unavailable or
invalid labels leave ID matching usable. This name capture is a prototype and
needs a live Eden check, especially Blathers and villager names.

Bad values/keys warn; invalid values retain the earlier layer's value, and
invalid list entries are ignored. Duplicate preference keys revert that setting
to its value before the file. A supplied list replaces the earlier one. The
boost/base relationship is normalized after both files have loaded.

`faster_ui` covers native system dialog windows (the blue dialogs). Independent
`faster_unnamed` covers regular gameplay bubbles whose current native name
preparation confirms that no name is displayed. This includes player thoughts
and other unnamed messages, regardless of wording, language or inserted item
names. Storage sheds, mirrors/dressers, Niko's notes, amiibo-device thoughts,
group stretching and shop drop-off prompts need no per-message allowlist.

The captured result is checked against the current message bank/label, context
and speaker handle. An empty cache or failed name conversion does not prove a
message is unnamed. Named speakers keep NPC matching; blue dialogs keep their
independent UI setting. When name preparation is unknown, the previous valid
player-speaker and five reviewed bank fallbacks remain: `FTR_WorkingTable`,
`SYS_Player`, `SYS_Get_Fish`, `SYS_Get_Insect`, and `SYS_Get_DiveFish`. Confirmed
names override those guesses. Unknown names alone never trigger a boost.

`faster_ui` and `faster_unnamed` default to true. Set either to false in your
personal INI to disable that automatic category. Automatic categories select
the same `boost_speed`; holding B/R never stacks another boost. The broader classifier is new in
0.3.22 and needs live confirmation with the reported interactions. See
[research notes](research/dialogue/NOTES.md).
There is no song/travel skipping, forced reveal, voice cleanup or emote filtering.

To investigate these failures, build/package the diagnostic Core with:

```sh
sh tools/build.sh --verbose-dialogue
python3 tools/check.py core dialogue
python3 tools/package_dialogue_diagnostic.py
```

Set `log_debug = true` in `core.custom.ini` to enable diagnostic traces.
The diagnostic ZIP replaces Core only. Keep the Faster Dialogue module and your
personal settings, including `faster_unnamed = true`, then fully restart. Test a
mirror, storage shed, DIY notes, amiibo-device, stretching or drop-off prompt,
plus a listed NPC, briefly holding B/R for comparison.
Collect Eden's `QoL Enhancements/Faster Dialogue` lines before another session
replaces the log. `DIAGNOSTIC: unnamed-v1` identifies this build; `settings:`
shows loaded settings, `MESSAGE` shows active/cached banks, labels and reveal
gates, and `SELECT` shows the classification, speed and entity handle details. `name_tag` is 0 for unknown, 1 for named, and 2 for
confirmed unnamed.
MESSAGE runs in the input
hook even when native reveal gates prevent SELECT. Logs stop after 256 MESSAGE
or 128 SELECT transitions; repeated frames do not consume the limits. This
diagnostic includes the unnamed classifier and bounded observations. Restore normal
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

- `dist/qol-enhancements-v0_4_3_for_acnh_v3_0_3.zip`
- `dist/qol-enhancements-v0_4_3_for_acnh_v3_0_3_source.zip`
- `dist/qol-enhancements-v0_4_3_for_acnh_v3_0_3_faster-dialogue-custom-example.zip`

The binary ZIP contains a conspicuous installation text file, README, Core and
six independently selectable feature folders, plus a SHA-256 manifest. Source
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

## Pockets trigger shortcuts (0.4.2)

In normal Pockets, **ZL** selects **Release** or **Drop Item**, and **ZR**
selects **Hold**, **Wear**, or **Put Away**, without opening the item menu. A shortcut does
nothing if its native menu action is absent or disabled. It does not substitute
Place Item, Show Off, Eat, or other actions. If both matching actions were
present, the first native menu row wins. The selected action and item are
revalidated before dispatch; native placement/equipment restrictions remain.

Press once per item. Holding a trigger does not repeat, and simultaneous
shortcut buttons or navigation/A/B/X/L/R input suppress the shortcut. Special
selling/donation modes and drag states are excluded. Releasing a critter is
still destructive, just as selecting Release normally is; ZL intentionally
skips the item menu, not any downstream native dialogs.

Hints read **Drop / Release** and **Equip / Unequip**. Alongside A/B and the existing
home-only Y storage hint, these fill the native footer's five available slots.
These are fixed labels, not per-item availability indicators. Customize
`qol_pockets.drop_release` and `qol_pockets.hold_wear` in the Pockets messages
INI. The shipped new labels use USen fallback; native button glyphs retain
compatibility with controller-font replacements. No LayoutMsg override is added.

Pockets and Fitting Room share one Core caption-initialization hook, with
independent caption providers and files; either module can be enabled alone.

## Validation

See [VALIDATION.md](VALIDATION.md) for actual results and pending Eden checks.
The unified loader and suite require a fresh live test, particularly startup,
module toggles, Pockets translations, fitting checkout, speaker/UI classification
and held dialogue boost. Diagnostic builds use `sh tools/build.sh --verbose-dialogue`
for bounded SELECT logs. Future behavior changes use C++ hooks per AGENTS.md.

Exlaunch is pinned to upstream f9f4b0dd07b68f97958cb9c79228bbca22ca80d5.
Keep GPL-2.0 and framework third-party notices and distribute matching source
with the binary. Game assets are never part of the source archive.

## 0.3.60 Fitting Room rendering fix

Replace both **Core** and **Fitting Room**, then fully restart the game. The
basket now uses one antialiased texture instead of 70 rectangle panes. Its
shape, padding, mint color and group animation are preserved. Purchases and
capacity limits are unchanged.

Core shares one resource-size hook between Shopping Card and Fitting Room,
with independent exact-path allowances when their packages are installed.
This includes disabled feature configurations because their RomFS layouts
remain installed. No global resource table is replaced. The 16+ item flashing
report still needs a Steam Deck retest; local checks do not execute GPU draws.

## Fitting Room caption (0.3.68)

The native fitting-room footer now includes **Y: Bag**. Y still toggles
membership, including removing an already bagged item. Edit the Fitting Room
module's `romfs/qol-enhancements/messages/fitting-room.ini` to change the label:

```ini
[USen]
fitting_room.add_to_bag=Bag
```

Like Pockets, captions use UTF-8 and ACNH locale sections. Lookup uses the active
locale, then USen, then built-in English. Missing, unreadable or invalid files
fall back safely; text must be a plain single line without game control codes.
The shared parser accepts at most 127 UTF-16 units per label; short captions
are recommended for the footer. Restart ACNH after editing. This message file
belongs to Fitting Room and does not require the Pockets module to be enabled.
Update both Core and Fitting Room to receive the feature and default INI.

## Basket appearance (0.3.69)

The basket body now has diagonal sides, a flat bottom and rounded corners.
The handle, icon scale, circle and animation remain the same. It still renders
as one texture picture, with no additional drawing panes.

## Fixed localized Bag hint (0.3.71)

Restores the fixed-caption behavior of 0.3.69. The Add/Remove wording experiment
could retain the wider label's width, so the footer now always describes the Y
action as **Bag**. Y still adds/removes exact item variants. All 16 supported
locale sections include action wording (put into a bag, rather than the noun).
Edit `fitting_room.add_to_bag` in fitting-room.ini; the old removal key is unused.
The built-in English fallback is also Bag. The trapezoid icon is unchanged.
Translations are initial UI wording; live readability and native-speaker review
remain useful, particularly for languages that require a longer phrase.

## Fitting Room checkout B shortcut (0.3.72)

At “Do you want to wear what you picked out?”, B moves the cursor to
“No thanks.”. Press B again to choose it; if No is already focused, one B
chooses it. A and directional selection retain their normal behavior.
This applies with or without bag items. Update Core and Fitting Room, then
restart. Local ARM input/focus/answer tests pass; Eden verification is pending.

## Bag-only checkout (0.3.73)

Confirming a purchase with bag items and no clothes currently tried on skips
the wear question and sends the purchase through the normal delivery path.
Trying on any item keeps the wear question and its two-step B shortcut.
The purchase confirmation, funds checks and capacity checks remain in place.
Update Core and Fitting Room, then restart. Eden verification is pending.

0.3.74 adds up to 48 WEAR_BACK log records to investigate the B shortcut not
activating in Eden. It is a diagnostic build, not a confirmed fix. The user
confirmed native Mabel dialogue already skips outfit praise for bag-only buys.

0.3.75 fixes rejection of live B input carrying the accompanying bit-16 flag
(0x10020). Other simultaneous controls are still excluded. Temporary diagnostic
logging is removed. Native ARM tests replay the captured input; Eden retest is
pending. Replace Core and Fitting Room, then restart.
