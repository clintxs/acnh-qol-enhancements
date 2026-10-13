# Catalog: readable eligibility, history and markers

This module continues `refactor/semantic-feature-api` from accepted Crafting
commit `b3bd3acafb49aeb0ba7a4d1461fbebd64f79f469`. It stops at Catalog.
Implementation commit: `087e3ed5f9af4b735dd443ed27b04c5b3e8fc61d`.

## Read in this order

1. [Feature implementation](../source/program/catalog/features/catalog_features.cpp):
   `IsEligibleKind` and `IsExcludedReward` define supported surfaces/items;
   `QueryCatalog` shows donation versus resident catalog versus furniture color
   history; `DecideItemMarker` shows recipe priority and scoped creature-only
   names. `CatalogMarker`, `ShowRowBadge` and `CopyVariationLabel` choose the
   actual visible result. `IdentifyShopMessage` lists the exact shop messages.
2. [Feature declarations](../source/program/catalog/features/catalog_features.hpp):
   surface/use scopes and copied `MarkerDecision`. Ordinary implementations live
   in `.cpp`; the existing capitalization-parameterized text helper remains a
   template in `catalog_star_text.hpp`.
3. [Native history operations](../source/program/catalog/game/catalog_ownership.hpp)
   and [implementation](../source/program/catalog/game/catalog_ownership.cpp):
   live ItemParam identity/field validation, native kind classification, ready
   resident catalog views, original item/body history and island donation readers.
4. [Recipe knowledge](../source/program/catalog/game/recipe_knowledge.cpp):
   exact card encoding, validated DIY/cooking category, event-owned villager
   recipe and original learned-recipe getters. The output item's catalog record
   never substitutes for knowledge of its recipe.
5. [Row adapter](../source/program/catalog/game/catalog_badges_native.cpp),
   [balloon adapter](../source/program/catalog/game/catalog_stars_native.cpp),
   [dialogue substitutions](../source/program/catalog/game/dialogue_item_stars_native.cpp),
   [shop dialogue](../source/program/catalog/game/nook_catalog_stars_native.cpp),
   [color labels](../source/program/catalog/game/catalog_variation_labels_native.cpp).
   These retain native phases, ABI registers, borrowed UI objects and rendering
   operations. Core's existing module assembly installs the same hook identities.

## A real before/after

Previously `game/catalog_ownership.cpp::QueryCatalog` combined ItemParam parsing,
resident save resolution and the mod's art/variant decisions; marker choices
were repeated across hook callbacks in `features/`. Now the feature reads:

```cpp
if (game::IsCreatureKind(metadata) || (artStars && game::IsGenuineArtKind(metadata))) {
    return game::ReadMuseumDonation(item);
}
game::CatalogHistory history;
if (!game::OpenCatalogHistory(player, item, history)) {
    return CatalogStatus::Unavailable;
}
if (!game::HasCatalogedItem(history, item)) {
    return artStars && game::IsForgedArtKind(metadata)
               ? CatalogStatus::FakeArtUncataloged : CatalogStatus::Uncataloged;
}
```

The next branch checks furniture body history only after the item itself is
known. The native adapter still validates both live tables, body count, displayed
body and native history index before using the game's variant getter. No
replacement save bitset, ownership cache or independent progress writer exists.
The old mixed feature implementations were moved to `game/*_native.cpp` and
superseded policy was removed from them.

## Description-to-code map

| Player-facing behavior | Actual feature decisions | Native work retained |
| --- | --- | --- |
| Ordinary shop/ground/dialogue catalog names | `QueryCatalog`, `IsEligibleKind`, `IsExcludedReward`, `CatalogMarker` | Exact eight-byte source item, validated ItemParam identity, ready resident mapping/save view, original catalog lookup/read locks, name buffers/capitalization. |
| Fitting-room clothing colors | `Surface::FittingRoom` requires clothing; `QueryCatalog` checks the selected item's ID | Eight verified clothing kinds in both resource encodings; actual selected-row/controller/balloon correlation. Each clothing color retains its own native item ID. |
| Furniture family and body colors | `QueryCatalog` reads body history only after known family; `CatalogMarker` maps missing body to ☆ | Original count/index/variant queries, remake and history bounds, exact displayed body. Fabric/custom design bits do not invent extra colors. |
| Genuine art donation / forged art catalog state | `QueryCatalog` routes genuine art to museum when `art_donation_stars` is enabled; uncataloged fakes produce ☆ | Verified genuine/forged ItemKind identities, island museum view/lock and original donation lookup. Setting off restores ordinary catalog semantics. |
| Fish, bugs, sea creatures | `QueryCatalog` always uses museum; `DecideItemMarker` limits inventory and catch-only names to creatures or recipe cards | All 200 native species, full-item donation comparison, any resident's donor byte, main/alternate island and lock/readiness guards. Models, shells, fossils and event catches remain excluded from creature semantics. |
| Unlearned DIY/cooking recipe cards | `DecideItemMarker` chooses recipe knowledge before catalog ownership; unavailable knowledge produces no star | Card ID and low-16-bit recipe encoding, wrapped/special exclusion, disjoint validity tables, ready learned history and distinct original DIY/cooking getter ABIs. |
| Villager's current DIY/cooking recipe | `ItemUse::VillagerCraft` stays on the recipe path even when unknown | Verified script-item target 16, event-owned NPC type/save view and current recipe number. The player's own crafted-output UI-result names retain catalog semantics. |
| Nook/HHP/Hotel/Miles row badges | `ShowRowBadge` permits a complete badge only for `Uncataloged` family/donation status or a Miles unlearned recipe; missing-body and forged-art statuses stay hidden | Typed direct/group name binding, Miles slot 2 selection, bounded row-local parent/child traversal, all nine panes, original HHP/Hotel/Miles row renderer first. Every found pane gets the new visibility, including false on reused/unknown/excluded rows. |
| Missing furniture color labels | `CopyVariationLabel` chooses ☆ only for missing family/body; rejects tagged, empty, already marked, unterminated and oversized text | Exact Nook/HHP binding scope; HHP's actual displayed body; synchronous original pane/group setter once; native localization, notification and sizing. |
| Shop confirmation dialogue | `IdentifyShopMessage` lists Nook 101, HHP 001, Hotel 151 and Able 051/052; `Surface::Furniture` preserves Nook's narrower scope | Native action/event/selection/formatter, direct-before-reference dialogue resolution, constructor and owned slot-0 registration. Unrelated dialogue setters retain their actual tag slot. |
| Filled/hollow/absent names and safe fallback | `CatalogMarker`, `DecideItemMarker`, `ShowRowBadge`, `CopyVariationLabel` | Existing noun metadata, both singular/plural buffers, native capitalization including Dutch IJ, validated text capacity/storage, no duplicate prefix. Unsupported text remains native. |

## Ownership, ordering and failure

- `CatalogHistory` is a callback-local borrow obtained through
  `OpenCatalogHistory`. Call history operations only after success and in the
  same callback. It owns neither save memory nor a retained lock; original
  getters acquire/release their native read locks. No view survives an update.
- `Unavailable` remains distinct from unowned, unlearned and undonated.
  Malformed/missing metadata, identity, resident readiness, lookup, recipe
  category, museum provider/lock or variant data cannot earn a misleading star.
- Shared lists retain the existing broad ordinary-item rule: an identity-matched
  ItemParam row outside the explicit currency/recipe/reward exclusions can use
  native catalog history. Kind classification is not a new exhaustive allowlist;
  fitting and furniture scopes remain narrower. Unavailable history still earns
  no marker.
- Query ordering is preserved: eligibility first; donation before resident
  history when appropriate; one original item query; body query only for known
  furniture with a remake record. Item/history memory remains unchanged.
- Recipe cards use actual recipe identity. Villager dialogue reads the current
  recipe from its event-owned speaker; identical output items do not merge
  recipes. Unknown recipe data never falls back to output catalog ownership.
- Names are temporary callback borrows. Ground balloons use the per-balloon
  player byte; shop/fitting/inventory paths validate their caller and controller.
  Dialogue substitutions are marked after native construction and before the
  game's owned copy. Shop registration explicitly copies its local name.
- Color label scopes are synchronous and nest safely: inner updates restore the
  parent context. Only the matching payload and typed setter can use the copied
  label. No label result or UI pointer persists after the original update.
- Rows are rebuilt from current history each time. Missing or partial badge
  layouts hide every discovered layer, never show a bare outline, and never
  search a sibling row. No negative ownership result is cached.
- This module starts no deferred actions, purchases or donations. Its existing
  native copy boundaries handle later dialogue display; cancellation/reset is
  expressed by native rebuilds, scope exit and explicit badge visibility writes.
- Defaults, settings syntax/version, translations, assets and user controls are
  unchanged. `art_donation_stars` remains enabled by default. No release or
  installed emulator package was replaced.

## Verification and preserved outputs

- Fresh normal and verbose-dialogue builds passed with the repository's pinned
  devkitA64 container. The verbose build is compile-only evidence; the final
  root and saved candidate outputs are normal (`CAS_*` diagnostic flags zero).
- `QOL_BASELINE_ELF=dist/native-first-catalog/baseline/project.elf python3 tools/check.py`
  passed: **24 host suites and 52 ARM/resource scripts**, exit 0. Host policy
  tests link the actual feature `.cpp` against controlled native observations;
  they verify eligibility, history order/frequency, recipe priority/no fallback,
  marker choices, label rejection and repeated row visibility decisions.
- Catalog baseline replay passed **14 inherited scenario suites and 307,891
  tracked native calls** on both ELFs. It compares the same unchanged scenario
  assertions and call order/values for native item/body/donation/recipe queries,
  resolver calls and fixture read-lock callbacks. Temporary stack addresses are
  excluded from traces; complete eight-byte item and recipe values are retained.
  Each child process reads its own ELF symbols/relocations, without replacing
  root outputs. Final fitting coverage includes 26,077 cases and 5,207 clothing IDs.
- Production activation passed all **128 module subsets at two load addresses**.
  Catalog-only and Catalog with the other refactored modules match the accepted
  build's installed native hook sites. Shared baseline checks also passed:
  Dialogue 1,136; Crafting 93,804; Controls 2,156; Pockets 978 callback scenarios.
- Packaging/resource/config/manifest checks passed with `--revision 999999`
  (no release archive written). A temporary source archive included the new
  policy/native files and excluded the superseded mixed source, then was removed.
- All **3,227** pre-existing ignored `dist` files retained their hashes. The
  `main` and `refactor/internal-feature-interfaces` refs remain unchanged.

Logs are `full.log`, `baseline-comparison.log`, `focused.log`, `final-build.log`,
`verbose-build.log`, `package.log` and the historical baseline-failure log under
`dist/native-first-catalog/`. `candidate/SHA256.json` records final outputs/module
hashes; `candidate/identity.json` and `verification.json` record source/baseline
identity and checks. No live validation was performed.

The accepted build and Catalog module were copied before edits to
`dist/native-first-catalog/baseline/`; `identity.json` records their SHA-256 hashes,
branch and refs. `preservation.json` records all 3,227 pre-existing `dist` files.
Logs, final normal outputs and verification metadata stay in this ignored folder.

The older standalone `catalog_stars_arm_test.py` main still expects a catalog
star for creature item 2 without ready museum data. That assertion also fails on
the accepted baseline; it is unchanged. Maintained museum, art, ground/dialogue
and fitting scenarios supply the appropriate expectations and guards. The old
fixture remains reused by those tests; its stale standalone main is not counted
as a passing suite.

Offline tests use real local native ARM, ItemParam/ItemRemake and packaged layout
hierarchies with synthetic UI/save views and selected stubbed engine services.
Production activation tests establish hook installation/coexistence, not live
rendering. Eden validation still needs to confirm the actually loaded badge
resources, reused rows, localized/counted names, color labels, current-island
museum state and dialogue display after owned copying. No live game was run.
