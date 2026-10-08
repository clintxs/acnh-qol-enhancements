# Able Sisters Bag — prototype 0.3.7

Bulk-purchase prototype for ACNH 3.0.3 on Eden. Y toggles exact clothing variants
in a shopping bag; A retains the native try-on behavior. The displayed price
includes the tried-on outfit and bag extras, counting an item present in both
only once. The bag badge uses a custom thick shopping-bag outline in the worn checkmark's
pale-mint color, with padding inside the circle. It no longer uses the eShop glyph. No extra status text is added.

## Install and first test

Make the save backup you planned before testing purchases. Replace the existing
**Able Sisters Bag Prototype** folder with the one from
`Able Sisters Bag prototype 0.3.7.zip`, enable it, and fully restart the game.
Keep both `exefs` and `romfs`. Catalog Aware Shops and QoL Pockets Interactions
can remain enabled. Do not enable the historical combined Optimized UI mod.

The previous ten-item test succeeded on Steam Deck: the user confirmed the
correct Bell deduction (with Bell Boom) and all ten variants delivered to storage.
For this build, repeat the scenario that previously failed:

1. Add ten inexpensive items to the bag, then try on two other items.
2. Confirm the total, buy, and choose to send everything home.
3. Verify that all twelve distinct purchases arrived and the Bell deduction
   equals the displayed total. An item both tried on and bagged counts once.
4. Next try a purchase of 17–20 items, then a small transaction with storage
   overflow into pockets. Test the wear option separately.

The useful log sequence is `CONFIRM`, `PREFLIGHT`, `COMMIT_BEGIN`, two
`COMMIT_LIST` entries, and `COMMIT_RETURNED`. Purchases above sixteen entries
also log `RECORD_PREVIEW`. Invalid internal state logs `PREFLIGHT_INVALID` and
cancels confirmation without incorrectly claiming storage is full.

## Current limits

- The ten-item checkout cap is removed. The bag supports **256 distinct
  selections**, plus the native tried-on outfit. Exact overlap is counted once.
  This internal bag bound is not treated as a storage-capacity error.
- Storage is filled first; overflow uses pockets. Preflight conservatively
  reserves room for either checkout choice without assuming payment frees slots.
- Tailor tickets apply to the native tried-on outfit. Bag-only extras cost Bells;
  ticket assignment to additional bag items is not implemented.
- Native funds/capacity dialogs are used during checkout. If pressing Y would
  exceed available storage/pocket space, the addition is refused and the native
  capacity dialog is requested immediately. No extra status text is added.
- The native purchase-report format has sixteen detail slots. Its diagnostic
  record keeps the full payment total and a preview of the first sixteen items.
  Delivery, catalog registration and per-item milestone loops process the full
  purchase. The mod does not expand the fixed report payload.
- The bag resets when leaving the fitting room. It is not saved.
- Real save writes, clothing changes, rendering, and catalog results need live
  verification. Tests model the save APIs. The implementation uses the native
  transaction and inherits its delivery-failure behavior; it does not add a
  rollback system. A last-moment preflight rejection prevents payment but can
  still let the native caller close the fitting room.

## Implementation and validation

The mod uses `subsdk7`, a private fitting-room input vtable slot, and eleven code
hooks. It validates the 3.0.3 instruction fingerprints before installation.
The input pointer is written to validated writable data and checked by readback.

Before payment, the mod freezes native send/wear lists plus exact 8-byte bag
items. It supplies mod-owned item-pointer arrays to the native checkout lists and leaves native
payment, ticket consumption, storage/pocket delivery, clothing swap, result
dialog routing, purchase records, and milestones in place. Gross-price metrics
also include bag extras. A session guard prevents repeated bag payment.

`VALIDATION.md` records what is tested and what remains unverified.
`RESEARCH.md` records the reverse-engineered addresses and contracts.

## Development

`sh tools/build.sh` uses the pinned Docker toolchain with networking disabled.
`python3 tools/package.py` creates binary and source ZIPs and refuses to replace
existing releases. Python 3.14 supplies zstd. Original local game resources are
required and are not included in the source archive.

Run `tests/policy_test.cpp` with a C++20 compiler. Run the Python ARM tests from
the parent workspace with `PYTHONPATH='Grab 5/working/python'`. They need the
local mapped main, Unicorn, and compiled ELF files. On macOS Unicorn requires
CPU-cache/JIT access that the restricted execution sandbox may block.

## 0.3.1 icon revision

The bag is drawn with thirteen untextured native rectangle panes: a broad body
outline and a simple handle, with a five-unit stroke. Geometry is defined in
`tools/bag_geometry.py`. The existing circular background, placement, and group
visibility are preserved. `artwork/bag-outline-preview.png` is a geometry preview
on a simplified solid circle, not an in-game screenshot. Native rendering needs
live verification. Purchase behavior is unchanged from 0.3.0, which the user has
now tested successfully with nineteen items.

## 0.3.2 rounded icon and feedback

The bag body and handle are rounded, and the outline is reduced to 72% scale
for more space inside the circle. The badge plays the native checkmark's
EquipIn Hermite scale curve (0.5 → 1.4 → 0.9 → 1.0 across seven UI updates)
and alpha onset; removal follows its short hide timing. The curve is applied
only to the separate bag group. Recycling a row resets its appearance from item
identity, and leaving the fitting room clears animation state.

Successful Y adds/removals also trigger the native cursor click animation,
using the animation-only path researched in QoL Pockets. It requires a visible
primary-player cursor still attached to the selected fitting row. It does not
send a button click or trigger the A/try-on action. Rejected or held inputs do
not replay the tap. Visual rendering and timing need live verification.

## 0.3.3 animation redraw fix

Mark the bag pane's transform dirty whenever its scale changes. Without this,
the native renderer could retain the small initial transform until moving the
cursor invalidated its parent. The native matrix-propagation path is now
covered in the feedback test with parent invalidation disabled. The rounded
outline is 40% thicker (seven units before the existing 72% icon scale).
The icon centerline dimensions and placement are unchanged. Live verification
is pending; transaction and cursor-tap behavior are unchanged.

## 0.3.4 basket silhouette

Replaces the outlined bag with the requested two-semicircle design: a solid
lower half-circle bowl, and a smaller hollow upper half-circle handle. Native
horizontal strips approximate the curves, leaving the handle interior open.
The silhouette is 34 by 26 layout units, centered inside the existing badge.
Mint color, badge animation, hand tap and purchase behavior are unchanged.
The preview shows the intended shape on a simplified solid-circle background;
actual rendering remains to be checked in-game.

## 0.3.5 larger, softer basket

Enlarges the basket by 25% to 42.5 by 32.5 layout units and rounds the two
outer rim corners into the bowl. Badge size, mint color, animation, hand tap
and purchase behavior are unchanged.

## 0.3.6 larger basket

Increases the basket another 20% to 51 by 39 layout units, preserving its
rounded rim corners and the existing background circle. Runtime behavior
is unchanged.

## 0.3.7 larger handle

Restores the 0.3.5 body scale and rounded rim, and increases the handle outer
radius from nine to eleven units. The inner radius increases from five to
seven units, preserving the four-unit handle thickness. Background circle,
animation and purchase behavior are unchanged.
