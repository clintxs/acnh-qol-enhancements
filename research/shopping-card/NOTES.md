# Shopping Card — ACNH 3.0.3

Prototype 0.3.9, October 8, 2026. Original mapped main SHA-256:
`b1ae8e2a0d82f9f38f711c132d1751b221baf9d6411b2be06ebc4bb502ac912d`.
Build ID: `FF1D1C05670DB6021C85B624A710B963`.
All implementation uses readable Exlaunch C++ hooks. No new game item or data
schema is introduced. The optional module is always active when installed.

## Native evidence

The native event hash registries identify factories:

| Event | Hash | Factory | Execution |
|---|---|---|---|
| ShopHasBellCount | ECA8ABED | 01074220 | 00E7B530 |
| ShopBuyDone | 39A66E1F | 0100BA8C | 00B15CE0 |
| ShopSellDone | 7CB80C0B | 0101E998 | 00B1BFD0 |
| MoneyCapacity | 004DA1E7 | 01045164 | 00F86E68 |

Read the local decompiled `SNPC_rcm_00_Common` and
`SNPC_rcm_00_Common_Sell` flows alongside those functions. HasBellCount calls
the combined wallet/Bell-bag getter at 0257ABF0 from 00E7BF84. ShopBuyDone
spends through 0257B340 at 00B168B4. Native fitting checkout uses that same
spend helper at 0311D400; the native fitting-session balance is read at 031192D8.
The suite's existing C++ bag preflight also needs the shared AvailableBells accessor.

Savings APIs accept a pointer to the current one-byte player context:

| Operation | Main offset |
|---|---|
| Read savings | 0257F8B0 |
| Add savings, cap 999,999,999 | 0257F9F0 |
| Subtract savings, clamp at zero | 0257FB30 |

These resolve the resident through 024830C0/02483354, use the native tracked
accessor at resident+194B0+8620, and operate on encrypted SaveMoney through
0275DC20/0275DD6C. Production code calls the native APIs and never writes these
reverse-engineered save/accessor offsets directly. The cap and clamp behavior
are deliberately native; this prototype does not extend the savings limit.

At 00B1D4D4, ShopSellDone asks 0257B6F0 for wallet/pocket payment capacity.
Returning zero forces every positive sale through its existing savings deposit
at 00B1D4E8. A zero-valued sale follows the original zero-wallet-add branch.
The cShop MoneyCapacity query now selects existing flow case 0 for every
resident: SP_rcm_00_common:051, “Thank you! Please come again!” (with the twins’
echo). Verified directly in the local original 3.0.3 TalkSNpc_USen archive.
Case 0 executes the same ShopSellDone before the message; routing remains
controlled independently by the capacity hook above. No MSBT or event-flow
resource is replaced. Other modes/rooms and drop-off-box payouts are unchanged.

In 0.3.0–0.3.2, the query selected case 2 for residents with placed tents/houses.
Although that text mentions wiring to the account, it also incorrectly says
the wallet and pockets are full. Version 0.3.3 uses the neutral normal thank-you
instead and removes the now-unnecessary native house-level dependency.

ShopHasBellCount/ShopBuyDone resolve current field metadata through the
manager at GOT 04AB2720 and field-type table at GOT 04AB2640. The active field
is +330 and type key +338. Its getter at vtable+18 returns a pointer to a name
string; MurmurHash3(seed 0) at 005BD550 identifies indoor type 758F7EAF.
Indoor metadata at GOT 04AC3A80 has the room-name hash at +38:

| Room | Hash |
|---|---|
| IdrMarket01 | BE3CAD72 |
| IdrMarket02 | 8A17F484 |
| IdrTailor | 91E12AA0 |

Physical-shop routing uses those rooms and exact reviewed BL return addresses.
The catalog callers below are independent of room, including phone use outdoors.
No remembered speaker, message bank, local-player singleton or persistent
shop-session cache is used. The exact callsite lists are in shopping_card.cpp.
Each site and the native dependencies are fingerprinted before installation.
Native room state still needs an Eden test; the local harness models its inputs.

Purchase-space helper 02579FE0 is called by the item-space query at 00F9255C
and fitting confirmation at 031143FC. Its arguments are player, Bell payment,
and tailor-ticket usage. Pass zero Bell payment in card shops while preserving
tickets; otherwise native capacity could count Bell-bag slots that a savings
payment will never free.

## Savings currency display and pocket-close regression

The user confirmed Nook’s Cranny transactions in 0.3.1, then reported a
ka-ching and top-right refresh after closing pockets without a transaction.
Generic money UI 02B13F10 compares its cached cash with 0257ABF0. Its initial
value (02B136E8, controller+21C) is supplied as cash, so substituting savings on
refresh queued a spurious native counter transition (+290/+294/+298/+29C).
0.3.2 removes 02B13F18/28 and 0301AF0C/0301B67C/0301B6C0 from the savings
callers. Native cash initialization/refresh/animation now use the same source.

The regular Bell HUD scene uses L_Bell_00 -> LBellCounterRT. Its BellPage
update starts at 02B13BF0, controller resident+224 and Page+210. Page+3F8 is a
holder for a layout object; layout+38 owns the parameter table. Native
02B12C24 resolves the /L_Bell_00 typed counter parameter. This matches the
original update’s lookup at 02B13C44..02B13C6C. The SafeString vtable is obtained
from GOT 04AB2020, just as native code does.

An auxiliary controller updates at 0301B394, with resident+230 and a direct
counter at +210. Versions 0.3.2–0.3.3 hooked only that auxiliary path. The user
reported that the regular Nook’s Cranny HUD continued showing pocket Bells;
local tests had modeled that auxiliary controller rather than the actual HUD.
0.3.4 hooks both updates and tests the original BellPage update from entry to
return with its real Page/holder/layout path. The alternate resident offset is
poisoned in tests so confusing the two controllers cannot silently pass.

On both paths, counter+250 is the numeric text binding; binding+48 is its typed
adapter (payload+8, methods+18, setter+5E0). Direct SetText 03C1B588 and grouped
SetText 03C47B90 are resolved before following a real pane’s parent. Catalog
can already hook these setters, so fingerprints check untouched bodies after
the 20-byte hook region.

The Shopping Card module supplies LBellCounterRT with an additional hidden
N_ShoppingCard_00 subtree. Its green rounded window, geometric card icon,
English Bells badge and independent T_SavingsNum_00 retain all original
panes/materials/textures/animations. Each HUD update writes the current
resident’s native savings using a 24-byte UTF-16 buffer for the
nine-digit cap with commas. Original cash panes are restored outside supported
rooms. No player/UI pointer is cached. Pocket/price counters retain cash.
0.3.5 adds the white oval/icon rims and native white Bells badge with green text.
The regular HUD keeps a value-only savings transition (no UI/save pointers).
Native 03353B14 steps that independent +260-shaped value object; a no-op
callback interface leaves text/feedback under the module's control. On each
actual step, 03025400 receives the native previous/current/direction/finished
event and plays the live counter's Up/Down/Finish animation and sound feedback.
Its hidden cash text is immediately restored with 03025A4C; no native cash
amount, target or controller cache is changed. Savings text uses the independent
animated value. A new target retargets from the currently shown value. Initial
entry, room/resident switches and settled balances are silent. Only the regular
HUD drives feedback; the auxiliary display stays immediate to avoid duplicate
sounds. Leaving a supported room invalidates the numeric state.
Missing layout/adapter leaves ordinary Bells UI intact. Bounded diagnostics
report SAVINGS_DISPLAY_READY or SAVINGS_DISPLAY_MISSING with source=HUD or
auxiliary, making an unbound path observable. Native HUD cash refresh remains
independent: savings changes do not queue its money animation, while genuine
cash changes still do. The user confirmed the corrected 0.3.4 savings HUD and the 0.3.5 balance
animation. Their screenshots compare that layout with native Nook Miles and
Bells counters. 0.3.6 doubles the oval rim from four to eight layout units,
centers the card over the oval, and places its separate white backing BEFORE
the oval in draw order. The dark-edged green card is drawn AFTER the oval,
so the white lower edge is occluded while the card remains visible in front.
Both card layers use the same transform; the icon is almost upright after the
native parent's rotation. The native right-origin Bells badge is placed eight
units inside the oval's right edge. No transaction or animation behavior is
changed. In 0.3.8 the stepped rectangular card silhouettes are replaced with
three centered clones of the native W_BellBase_00 rounded window. White backing
94×66/frame 15, dark edge 82×54/frame 9 and green face 76×48/frame 6 preserve
its texture/filter/UV/shader setup. Their nested sizes give a six-unit white
rim and three-unit dark rim with concentric corners. The backing remains behind
the oval; four small rectangular decorations remain on the card face. The whole
Bells badge moves 24 layout units left (about 16 screen pixels at 1280 width),
keeping the caption centered in its white badge. The user’s 0.3.8 screenshots
show a collapsed card, so preserving the mask setup was insufficient: these
window materials/frame settings cannot be treated as arbitrary rounded rectangles.

0.3.9 removes those three windows. Each layer is now a filled silhouette of
ordinary untextured PIC quads: central band plus 0.75-unit-or-finer corner bands,
with one-unit vertex-alpha ramps along the outline. The same layer dimensions,
radii, colors, draw order and transforms remain. The three layers total 255
small quads, plus four decoration quads, sharing one solid material. This uses
the same primitive path as the previously working vector icon, with much finer
curves and alpha feathering. Check native frame pacing as well as edge quality.
The exported geometry is rasterized by tools/preview_shopping_card.py for local
inspection. This preview is not a native window/font/animation renderer.

The default resource retains its original 1920×1080 canvas, root and anchors.
There is no 16:10 height offset to undo for 16:9. A future verified placement
adjustment for a 16:10 mod should be an optional replacement layout, merged into
the Shopping Card module after suite updates. Do not assume an additional +60
root shift is needed: a containing HUD/aspect mod may already adjust placement.
Both aspect ratios still require live checks before publication.

## Deferred design

The requested initial prototype omits the unlock, Bells-menu entry, dynamic
message injection and per-player on/off preference. No SD mount or permission
was added to the suite's NPDM.

For the eventual toggle, a sidecar preference is a candidate, keyed by stable
island identity plus resident/account identity rather than the transient 0–7
player slot. It should default safely when absent and use versioned records and
atomic replacement so a failed write cannot affect the save. Verify actual
Eden/Switch mount and FS access before choosing a location; do not assume the
existing RomFS configuration mount is writable. Removing the module would leave
at most an inert sidecar, with no custom save field or unrecognized item.

The 100,000-Bell combined buy/sell unlock should use the game's existing
Nook's Cranny progress counter after its current-version semantics and resident
versus island ownership have been verified. Do not independently count routed
transactions or guess a flag offset. An ABD interaction hook and a native text
injection/menu-label path can then supply the unlock/toggle without MSBT
replacements. Those are separate research tasks, not claimed functionality.

## Verification

`tests/shopping_card_arm_test.py` executes compiled callbacks and original
savings/encryption/sale-branch/hash instructions at two ASLR bases, with modeled
resident lookup, field metadata, original wallet services and RNG. It checks all
eight resident slots, exact caller and room routing, zero/cap behavior, native
sale deposits, ticket-preserving space accounting, the shared fitting accessor,
module absence and each fingerprint failure. It does not execute Eden or write
actual saves. See VALIDATION.md for the required live playtest matrix.

## Nook Shopping/catalog — 0.3.7

Regular orders for yourself use the native Catalog sequence, constructed at
02B65F30 by factory 032A7508. Its resident is +1F8, Page +210 and vtable
049B28B8 (assigned at 02B66020). The catalog affordability BL at 02B7763C
calls Wallet 0257ABF0; its exact return 02B77640 now selects savings regardless
of room. The item-price routine 019ED730 and RegulationType4's native 20%
Bell Boom arithmetic/insufficient-funds choice remain intact (02B77700..7A8).
Accepted self-order callback 02B7F384 calls Debit 0257B340 at 02B7F588,
return 02B7F58C. That caller now withdraws savings. The original mail delivery,
order limits, purchase records/milestones and cancellation continue unchanged.
No generic price/display wallet read is redirected.

Catalog and CatalogBG contain no LBell part; the visible currency uses the
shared BellPage HUD. 0.3.7 searched the root list from BellPage+98 for the
Catalog sequence, assuming both shared a UI context. Its fixture passed, but
the user reported pocket Bells outside Nook’s Cranny. That assumption is no
longer required in 0.3.8; the exact live context arrangement was not measured.

Catalog vtable 049B28B8 supplies OnOpen at +168 (02B70FFC), close cleanup at
+1C8 (02B711B8), and destructor at +10 (02B66CF0). The deleting destructor
02B67064 calls the same destructor at 02B67074. Opening activates children
+228 and +218 through 03769260, then sets the +210 Page visibility through
03C5C660. Close has a native early-return path when +1F0/+1F4 are both -1 and
+1B0 is below 2. Trampoline hooks set the resident bit after native open and
clear it before close/destruction, including that early return. Only an eight-bit
resident mask survives calls; no UI/controller/save pointer is retained. This
is transient display state, with no on-disk preference or save-field addition.
A closed/destroyed catalog restores cash outside the physical card shops.
Savings animation keeps its value-only transition and distinct catalog context
key; native cash initialization/refresh remain unchanged.

The revised fixture uses deliberately different Catalog/BellPage contexts and
executes native open/close instructions plus BellPage updates at two ASLR bases.
It covers all residents outside shops/in Resident Services, purchase feedback,
silent reopen, resident isolation, ordinary teardown and the deleting-dtor BL.
Engine child/page calls and destructor internals are modeled. This removes the
old fixture’s shared-context assumption without claiming an Eden playtest.

Catalog gifts use separate CatalogGift/recipient/network sending paths (including
02B7F988, 02B7FB40, 02B7FE2C and 02B78030) and are not covered by these two
exact payment callers. Their native payments remain unchanged in this prototype;
do not infer gift coverage from the shared initial catalog selection or HUD.
A future extension must trace the actual accepted send and its affordability
checks together before redirecting shared mail/event money routines. The ABD,
Poki conversion, ordinary letter fees and other shared transactions must retain
their original sources. Nook Stop and phone orders still require an Eden playtest.
