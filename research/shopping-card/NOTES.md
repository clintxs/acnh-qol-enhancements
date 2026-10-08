# Shopping Card — ACNH 3.0.3

Prototype 0.3.3, October 8, 2026. Original mapped main SHA-256:
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

Only those rooms and exact reviewed BL return addresses select savings.
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

The Bell scene uses L_Bell_00 -> LBellCounterRT. Its update starts at
0301B394, controller player+230 and counter+210. Counter+250 is the numeric
text binding; binding+48 is its typed adapter (payload+8, methods+18,
setter+5E0). Direct SetText 03C1B588 and grouped SetText 03C47B90 are resolved
before following a real pane’s parent. Catalog can already hook these setters,
so fingerprints check untouched bodies after the 20-byte hook region.

The Shopping Card module supplies LBellCounterRT with an additional hidden
N_ShoppingCard_00 subtree. Its green rounded window, geometric card icon,
English Savings caption and independent T_SavingsNum_00 retain all original
panes/materials/textures/animations. The top-right update selects that subtree
and writes the current resident’s native savings amount synchronously, using a
24-byte UTF-16 buffer for the nine-digit cap with commas. Original cash panes
are restored outside the supported rooms. No player or UI pointer is cached.
Generic pockets/price counters retain the cash presentation. Savings updates
are immediate in this prototype; no dedicated count-up animation is added.
Missing layout/adapter leaves ordinary Bells UI intact. UI rendering and audio
still require live verification; local tests cover the native refresh branches
and modeled bindings plus structural resource integrity.

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
