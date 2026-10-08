# Catalog Aware Shops

Version **0.8.1**, for Animal Crossing: New Horizons **3.0.3** on Eden.
Adds catalog-awareness to shop names and catalog icons, plus learned-recipe
markers on individual recipe cards in pockets and home storage, and recipe/catalog
markers on ground-item balloons. The user has confirmed Nook Miles icon badges
and ground-item stars. This build fixes incorrect item-source selection in
UI-result dialogue, including crafting. The correction awaits in-game confirmation.

## Catalog markers

| Item | Catalog history | Marker |
|---|---|---|
| Furniture | Main item never cataloged | ★ |
| Furniture | Main item known, displayed body/color missing | ☆ |
| Furniture | Displayed body/color known | None |
| Clothing | Displayed color's separate item ID missing | ★ |
| Other eligible items | Item ID missing | ★ |

On balloons and dialogue, stars prefix the item name. Bells, Bell vouchers, Nook Miles
Tickets, and cliff, river and path construction permits are excluded throughout the catalog checks. Furniture checks use the game's body/color history,
not fabric patterns, custom designs or Cyrus availability. Recipe books,
individual shop recipe cards and inventory DIY recipe cards are excluded,
including cooking books. Tools and other ordinary items remain eligible.
Unavailable history or unsupported text is left unchanged. Catalog data is read,
never modified; the game records purchases normally.

Supported surfaces:
- Shared shop-list balloons, including Nook's cabinet and the Harv's Island shops
  confirmed by the user with the previous combined mod.
- Able Sisters fitting-room highlighted clothing balloon.
- Individual recipe-card name balloons in pockets and home storage (★ means unlearned).
- Ground-item balloons: recipe knowledge for cards; catalog history for other eligible items.
- Shared event-flow item-name substitutions, independent of message IDs (new).
- Nook's displayed furniture: `SP_rcm_00_common:101`.
- HHP displayed items: `SYS_Wherearen_OfficeShop:001`.
- Hotel displayed items: `SP_kpg:151`.
- Able Sisters displayed clothing: `SP_hgh:051` **and `052`**.
- Nook Shopping, HHP and Hotel catalog row icons.
- Nook Miles redemption-list icons: recipe knowledge and ordinary-item catalog history.
- Nook Shopping and HHP furniture variation-name labels.

Dialogue stars use the native item-name substitution and its coloring/articles.
Subsequent lines may reuse the substituted name until the game replaces it.
The shared event-flow substitution hooks also run outside those shop messages;
coverage of individual conversations still needs in-game testing.
This mod adds no multi-select purchasing or pocket interactions.

## Catalog icon badges

Filled row-icon stars were confirmed working by the user in 0.2.3. The lookup
stays inside each row's icon container so recycled rows cannot affect one
another. This release retains that behavior and the original three layout files and adds the Nook Miles row.

`CATALOG_BADGE` reports ownership independently: `status=0` means unavailable,
`1` cataloged, `2` no family cataloged, and `3` only the current furniture variant
missing. Expected results are `panes=9 visible=1` for status 2 and
`panes=9 visible=0` for status 1 or 3. On a missing/partial badge layout, the
bounded `CATALOG_TREE` diagnostic now covers only the row's icon subtree.

Catalog rows show an outlined filled star only when the main item has never
been cataloged. A known furniture item with a missing body variant has no filled
row star. Clothing colors retain their separate item IDs.

The native dark fill and pale outline from Layout Test 3 are retained. All nine
layers update together as the game assigns items to pooled rows. Ownership is
queried on each row update, without a cached result. Buying refreshes the badge
when the game next rebuilds that row; this release does not force extra refreshes.

The four row layouts are now included in the main mod and start with badges
hidden. Disable the separate **Catalog Badge Layout Test** mod. Missing resources
or unavailable ownership data do not intentionally produce a star. The local
16:10 source replaces none of these four row files. Other mods that replace the
same row files need a combined layout.

The visual design is user-tested in the three original catalogs.

## Variation-name stars

In Nook Shopping and HHP, the bottom-left furniture color label now reads
`☆ Silver` when that color is missing from the current player's catalog, and
`Silver` when it is known. If no color of the item has been cataloged, both the
filled item-icon star and the hollow color-label star appear. Clothing keeps
its existing filled-star-per-color behavior.

The hooks run during the native variation-label update, covering item changes
and cycling colors. HHP explicitly selects the displayed body index from its
controller, matching the game's color-name lookup. Both direct and grouped text
bindings are supported. Native formatting/capitalization is preserved; stars are
added to a temporary text copy and the original text and save history are never
modified. Empty, tagged, oversized, or unsupported labels are left unchanged.

The Hotel catalog has no equivalent color-name/variation component in its
original layout, so this addition applies to Nook Shopping and HHP. It adds no
layout replacements and requires no additional 16:10 compatibility patch.
The variation-name stars have passed offline ARM checks and user testing in game.

## Recipe cards and ground items

Hover over an individual DIY or cooking recipe card in pockets or home storage.
Its balloon gets **★** when that player has not learned the recipe, and no star
when it is learned. Dropped recipe cards use the same check. This is independent
of whether the resulting item is in your catalog; it never learns a recipe or
changes your save data.

Other eligible ground items, including furniture shaken out of trees, use the
catalog markers above: **★** for a new item and **☆** for a missing furniture
body/color when the main item is already known. Ordinary items in pockets and
storage remain unmarked. Recipe books and shop recipe cards remain excluded.

The check runs when the game rebuilds a balloon's name. After learning a recipe,
move away and back to a duplicate card to refresh it. Ground balloons retain the
game's own refresh timing; the mod does not force a refresh while standing still.
Home-storage support targets the normal browsing view, not decorating selectors.

Startup should include `POCKET_RECIPE_STARS_READY`, `STORAGE_RECIPE_STARS_READY`
and `GROUND_STARS_READY`. `POCKET_RECIPE_STAR` and `STORAGE_RECIPE_STAR` report
recipe number, player, status and `marked`. Recipe status `0` means unavailable,
`1` learned and `2` unlearned. `GROUND_STAR` reports item, player, whether it is a
recipe, status, marker and `marked`; ordinary items use the catalog status values.
Each surface logs at most 64 relevant updates per launch. Unsupported data leaves
the name unchanged. No new layout files are required; the ground path adds one
executable hook.

DIY and cooking cards in pockets have been confirmed working by the user.
Ground recipe and item stars are confirmed. Coins and bags of Bells are excluded
by the Money item category, covering all denominations.
Try a learned and unlearned
card of each type, plus known and new dropped furniture. Include the startup and
relevant surface logs if a result is unexpected.

## Nook Miles redemption

0.7.1 fixed row-name binding selection: Miles can store its item name in the
third binding, while 0.7.0 always looked at the first. The lookup now follows
the native selection. The user confirmed the fix in game.

The Nook Stop's Nook Miles list shows the same small dark star with a pale outline
at the bottom-right of the item icon as the other catalogs. Names stay unchanged.
Individual recipe cards get **★** if unlearned. Ordinary items get **★** if the
main item has never been cataloged. As in the other catalog icon rows, a missing
furniture variant alone does not show a filled star; Miles has no variation label.

Bells, Bell vouchers, Nook Miles Tickets, all cliff/river/path construction permits,
hairstyle unlocks, pocket upgrades, other ability books and recipe bundles are
left unmarked. Voucher, ticket and permit exclusions also apply on other catalog
surfaces; those rewards are not collectible catalog entries.

This adds `LBtnListShopSonkatsuBtn.Nin_NX_NVN.zs`. The local 16:10 mod changes the
containing `ShopSonkatsu` layout, not this row file, so no combined patch is needed
for those local versions. All three previous badge layout files remain unchanged.

Startup `CATALOG_BADGES_READY` now includes Miles. Its capped `CATALOG_BADGE shop=Miles` diagnostics report item, player, catalog status, pane count, visibility,
recipe_id and recipe_status. Expected pane count is nine. Recipe status is 0
unavailable, 1 learned, 2 unlearned. Badges reset after each native row update,
including empty and excluded rows. Replace the entire mod folder, including RomFS.

## Dialogue item substitutions

0.8.1 fixes false stars on familiar crafted items. UI-result dialogue can build
its name from three different item sources; 0.8.0 always checked one scratch
buffer. The hook now follows the resolved native tag to the actual item used
for that name, including crafted recipe output. Unknown tag paths stay unmarked.

The shared event-flow item tag setters now prefix uncataloged items with ★ and
missing furniture body/color variants with ☆, without checking a message ID.
Individual recipe cards use learned-recipe history. Bells and the other currency,
travel and construction exclusions also apply here. The existing shop message
hooks remain as a fallback for their confirmed native dialogue contexts.

The hooks edit the temporary native name before the game copies it into its
chosen substitution slot. Singular/plural forms, noun metadata, item variants,
and the event's player are preserved; no shop selection is assumed. Text retains
its native casing because a dialogue noun commonly follows an article.
No name pointers or ownership results are cached. A substitution reflects history
when prepared, so already-open text does not automatically refresh after a gift.

This is an initial expansion, not a guarantee that every item mention uses these
paths. Test a villager gift or sale involving a missing item and a known item.
`DIALOGUE_ITEM_STARS_READY` confirms installation; the first 96
`DIALOGUE_ITEM_STAR` entries report the source path, item, player, ownership,
recipe status and whether the name was marked. Static message text stays native.

## Counted recipe names

Fence recipes use the game's counted-name format, with separate singular and
plural text buffers. Earlier versions accepted only the plain format, so an
unlearned Spooky fence recipe could log `status=2 marked=0`. Both verified counted
forms are now supported, while preserving noun metadata and native capitalization.
This applies to the shared shop, pockets, storage and ground name paths.
Pocket/storage recipe logs also include `type` to diagnose unexpected formats.

## Known limits and compatibility

The tested target is ACNH **3.0.3 on Eden**. Other game versions, emulators and
Switch hardware have not been validated. Unsupported instruction fingerprints
disable the affected hooks and produce a diagnostic instead of installing them.

- Special Goods can offer a missing furniture variant without a variation-name
  label. If another variant is cataloged, that offer has no star indicator.
- General dialogue support covers the shared event-flow item tag setters.
  Names written literally into messages, other specialized native formatters,
  and Able Sisters mannequin choice labels are not guaranteed to receive stars.
- The Hotel catalog has no variation-name label to mark.
- Layout compatibility is specific to the four packaged files:
  `LCatalogBtn.Nin_NX_NVN.zs`, `LNatteeCatalogBtn.Nin_NX_NVN.zs` and
  `LHotelCatalogBtn.Nin_NX_NVN.zs` and `LBtnListShopSonkatsuBtn.Nin_NX_NVN.zs`. Check the archive manifest for exact paths.
  The local 16:10 mod has no overlapping files; another mod replacing any of
  these files requires a merged layout.

## Installation and migration

1. Disable/remove the old **Optimized UI** mod in Eden. Keep its ZIP for rollback.
2. Extract `Catalog Aware Shops 0.8.1.zip` into the game's mod directory so the
   `Catalog Aware Shops` folder contains both `exefs` and `romfs`. Replace the previous version and
   disable the separate Catalog Badge Layout Test mod.
3. Enable Catalog Aware Shops. Enable **QoL Pockets Interactions** separately if
   you want Grab 5/10, direct storage withdrawal and Y-to-storage too.
4. Restart the game.

Either new mod can be used alone. Catalog Aware Shops uses `exefs/subsdk8`;
QoL Pockets Interactions uses `exefs/subsdk9`. Both ship the same byte-identical
`main.npdm` so that either is self-contained. Do not rename their executable
slots, or enable the old combined mod alongside them. Catalog Aware Shops needs
no messages.ini. Its RomFS contains only the four catalog row layouts; no
replacement fonts or messages are included.

Expected startup log: `[CatalogAwareShops] BOOT_OK 0.8.1; gameplay_hooks=16 dialogue_trace=0`.
`SHOP_CATALOG_STARS_READY` includes `able:051/052`. The existing dialogue
`NOOK_CATALOG_STAR` diagnostic now appears under `[CatalogAwareShops]` and has
`shop=nook/hhp/hotel/able`; status 0 unavailable, 1 cataloged, 2 missing (★),
3 missing furniture variant (☆). Shared-list and fitting-room checks have their
own diagnostics. `CATALOG_BADGES_READY` confirms the four row hooks installed;
`CATALOG_BADGE` reports shop, item, player, status, pane count (expected 9) and
visibility. `CATALOG_VARIATIONS_READY` confirms the new label hooks.
`CATALOG_VARIATION` reports shop, item, displayed body, player, status and
`marked` (1 means ☆ was added). Include these lines if a label is unexpected.

## Troubleshooting logs

Normal builds retain version/startup, hook failure and capped item ownership,
text binding and pane lookup diagnostics. Broad `NOOK_MESSAGE_TRACE` and
`NOOK_MESSAGE_DISPATCH` logs are disabled by default, along with the extra
trace-only hook. Tree dumps occur only for a missing or partial badge layout.

For a report, restart the game, reproduce the issue promptly, and include the
`[CatalogAwareShops]` startup lines and relevant item diagnostics, the shop/item,
game and mod versions, and other UI mods enabled. Restarting resets the log caps.

A developer can build broad tracing with `sh tools/build.sh --verbose-dialogue`.
Its startup line reports `dialogue_trace=1`; the normal build reports `0`.
This is a build option, not a runtime setting. Rebuild with `sh tools/build.sh`
to return to release defaults. Both commands clean objects before building so
switching modes cannot reuse the previous logging configuration.

## Build and validation

`sh tools/build.sh` uses the pinned Docker toolchain with networking disabled.
`python3 tools/package.py` creates versioned binary/source ZIPs and refuses to
replace an existing release. Distribute `Catalog Aware Shops 0.8.1 source.zip`
alongside `Catalog Aware Shops 0.8.1.zip`; it includes the matching source,
build tools and license files. Both standalone source projects are independent;
this project does not build or include the separate pockets interaction features.

Offline regression tests and compiled startup compatibility are documented in
`VALIDATION.md`. They require the local original main and extracted game data;
those assets are not distributed. Existing badges and variation labels have been
confirmed working by the user. Offline coexistence checks exercise both compiled
mods; they do not substitute for an Eden smoke test of the final release.

Before publishing, test 0.8.1 with both mods enabled: scroll/filter catalogs,
switch between known and missing colors, purchase and reopen the relevant UI,
and exercise the pockets actions. Confirm Able display messages `051`/`052` and
that startup reports no disabled features. That final live check remains pending.

## Reading the implementation

Start with `source/program/bootstrap.cpp`: it checks the supported game build,
initializes the hook framework, and enables balloon, dialogue, catalog badge,
and variation-label hooks.

| File under `source/program` | Responsibility |
|---|---|
| `game/catalog_ownership.cpp` | Validates item metadata, resolves the player's catalog, then checks item and furniture-variant history. Start at `QueryCatalog`. |
| `game/recipe_knowledge.cpp` | Validates a recipe card and live history before calling the game's learned-recipe getter. |
| `game/catalog_ownership.hpp` | Defines the 8-byte native item and the four ownership results. |
| `features/catalog_stars.cpp` | Finds the selected item in shop/fitting-room, pockets, storage, and ground names, checks catalog or recipe history, then prefixes its name. |
| `features/dialogue_item_stars.cpp` | Marks actual item substitutions in shared event-flow setters before their native copy. |
| `features/nook_catalog_stars.cpp` | Matches shop dialogue messages, finds the live formatter, and registers a copied starred item name. The filename and diagnostic names retain their historical Nook naming. |
| `features/catalog_badges.cpp` | Updates icon badges in Nook Shopping, HHP, Hotel and Nook Miles, including Miles recipe knowledge. |
| `features/catalog_variation_labels.cpp` | Marks missing furniture colors in the native variation-name label. |
| `features/catalog_star_text.hpp` | Inserts a marker while preserving noun metadata, capacity and capitalization. It contains no game addresses. |

Three kinds of numbers have different purposes:
- `address::...` values locate code/vtables/globals relative to the main module;
  `MainAddress` resolves them for the current randomized load address.
- `layout::...` values locate observed fields inside live game objects. They are
  not file offsets and do not describe the entire native class.
- CRC32 kind/field identifiers and ARM instruction fingerprints are evidence
  tables. Keeping their exact hexadecimal values is intentional. The comments
  explain their role; changing them requires verifying the new game binary.

`HOOK_DEFINE_INLINE` receives saved CPU registers at a verified instruction;
register-contract comments explain the relevant x-registers. The callback returns
to the native code through Exlaunch. Follow the named helpers first; read raw
fingerprint arrays only when checking support for another game version.

Missing metadata/history returns `Unavailable`, never `Uncataloged`. Borrowed
native pointers are used only during the callback; the dialogue registration API
copies the temporary name into owned storage for delayed display. Those rules
are why some defensive checks must remain even when normal gameplay passes them.

Formatting is defined in `.clang-format`, including the hook macros. Refactor
project code under `source/program`; `source/lib` and `source/rtld` are upstream
framework code and have not been reformatted in this pass.

## Earlier visual experiments

`CATALOG-BADGE-LAYOUT-TEST.md` documents the separate, always-visible Test 3.
Its reproducible builder remains available for appearance experiments. Version
0.8.1 packages the same design with initially hidden panes and runtime ownership
checks. Do not enable the visual-test mod alongside this release.
