# QoL Enhancements

**0.3.17 localized NPC dialogue matching** for ACNH **3.0.3 on Eden**. One Core
executable in **subsdk6** activates six optional RomFS feature packages. The
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
| Shopping Card | Always-enabled native savings routing prototype | Configuration + savings currency layout |

Faster tool-wheel animations and other workspace mods are not bundled in this
six-feature suite. Historical source notes under `docs/` describe each
baseline; their old installation/packaging instructions refer to standalone mods.

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

The Exlaunch SIMD context declaration now matches its assembly stack layout;
compile-time assertions and execution of the real adapter cover that correction.
Ordinary GP-only callbacks retain their original context layout.

## Shopping Card prototype

Enable **Core** and **Shopping Card**, then fully restart ACNH. The card is
always active inside both versions of Nook's Cranny and Able Sisters, and for
Nook Shopping/catalog orders for yourself at Nook Stop or in the phone app. Purchases
use the current player's ABD savings exclusively, even with empty pockets;
pocket Bells and Bell bags stay untouched. Nook's counter sales go directly to
savings and use the ordinary localized “Thank you! Please come again!” sale
message. Version 0.3.3 replaces the wire message, which incorrectly explained
that the player’s wallet and pockets were full. Native savings limits
apply (999,999,999 Bells, including the usual deposit clamping at the cap).
Insufficient savings does not fall back to cash. Existing affordability/error
text may still refer to pocket Bells.

This includes native fitting-room purchases with or without the Fitting Room
module. When Fitting Room is enabled, its bag preflight reads the same account
as native checkout. Tailor tickets retain their normal discounts and use.
Native item delivery, cancellation, purchase records and milestones remain in
the original transaction routines. Space checks no longer assume spending
Bell bags frees pocket slots. Harv's Island, other vendors, the drop-off box,
ABD transfers and ordinary pocket-money actions retain their existing paths.

The top-right currency display uses a green shopping card and a separate
savings amount inside the two shops and while the catalog is open. It fits the full 999,999,999-Bell balance;
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
Balance changes count up/down with the native increase/decrease/finish animation
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
savings, cancellation, order limits, delivery and screen transitions need live
verification.
See [research and follow-up scope](research/shopping-card/NOTES.md).

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
version = 0.3.17
base_speed = 2
boost_speed = 4
faster_ui = false
faster_unnamed = true
```

A `faster-dialogue.custom.ini` containing just `faster_ui = true` keeps all the
other values and overrides that one setting. An NPC list replaces the previous
list; an empty list clears it. Custom files are read once per restart, even if
regular `enabled` is false, so a custom `enabled=true` can re-enable installed
code. An unreadable, oversized or invalid metadata overlay leaves the regular
configuration intact. Neither malformed metadata nor duplicate metadata is
partially applied. Each file is limited to 4 KiB of valid UTF-8 text, with whole-line
comments, CRLF and an optional UTF-8 BOM.

Other modules currently expose just `enabled` and `version`, with names
`catalog.custom.ini`, `pockets.custom.ini`, `fitting-room.custom.ini` and
`amiibo.custom.ini` and `shopping-card.custom.ini`. Pockets' caption file is a separate translation resource,
not a module settings file.

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

Preferences absent from both files retain base 2 / held B/R 4, with UI/unnamed/list boost off. A listed
NPC, confirmed unnamed message or UI uses `boost_speed` automatically; holding B/R selects that
same tier and never stacks to 8. This prototype supports **2 and 4** for both
speed keys; a boost below the base is raised to the base. These are nominal tiers relative to the live native fast rate,
including language/FPS adjustments, not fixed wall-clock multipliers. Native
scripted waits, protected messages and nonnegative overrides retain priority.

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

Add `faster_unnamed = true` to your **existing personal INI** and restart. The
old `faster_player` key remains a compatibility alias. Use one spelling per
file: supplying both counts as a duplicate and restores the earlier layer's
value. Either spelling can override the other in a later file. The default is
false unless inherited. Automatic categories select the same `boost_speed`;
holding B/R never stacks another boost. The broader classifier is new in
0.3.22 and needs live confirmation with the reported interactions. See
[research notes](research/dialogue/NOTES.md).
There is no song/travel skipping, forced reveal, voice cleanup or emote filtering.

To investigate these failures, build/package the diagnostic Core with:

```sh
sh tools/build.sh --verbose-dialogue
python3 tools/check.py core dialogue
python3 tools/package_dialogue_diagnostic.py
```

The diagnostic ZIP replaces Core only. Keep the Faster Dialogue module and your
personal settings, including `faster_unnamed = true`, then fully restart. Test a
mirror, storage shed, DIY notes, amiibo-device, stretching or drop-off prompt,
plus a listed NPC, briefly holding B/R for comparison.
Collect Eden's `QoL Enhancements/Faster Dialogue` lines before another session
replaces the log. `DIAGNOSTIC: unnamed-v1` identifies this build; `READY`
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

- `dist/qol-enhancements-v0_3_4_for_acnh_v3_0_3.zip`
- `dist/qol-enhancements-v0_3_4_for_acnh_v3_0_3_source.zip`
- `dist/qol-enhancements-v0_3_4_for_acnh_v3_0_3_faster-dialogue-custom-example.zip`

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

## Validation

See [VALIDATION.md](VALIDATION.md) for actual results and pending Eden checks.
The unified loader and suite require a fresh live test, particularly startup,
module toggles, Pockets translations, fitting checkout, speaker/UI classification
and held dialogue boost. Diagnostic builds use `sh tools/build.sh --verbose-dialogue`
for bounded SELECT logs. Future behavior changes use C++ hooks per AGENTS.md.

Exlaunch is pinned to upstream f9f4b0dd07b68f97958cb9c79228bbca22ca80d5.
Keep GPL-2.0 and framework third-party notices and distribute matching source
with the binary. Game assets are never part of the source archive.
