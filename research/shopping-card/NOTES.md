# Shopping Card — ACNH 3.0.3

Prototype 0.3.1, October 8, 2026. Original mapped main SHA-256:
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
The cShop MoneyCapacity query selects existing flow case 2, whose message says
payment will be wired to the account. That flow has a per-resident housing
condition: PlayerHouseLevel must differ from Homeless (enum 0). Query factory
0105C500 (hash 74BA90AD) and evaluator 00F8B474 read the same house-level data
as 0256C940. Only residents with a placed tent/house use the wire message.
Other residents retain ordinary sale text and still deposit through ShopSellDone.
This avoids introducing a housing prerequisite for selling with the card. Drop-off-box payouts use
separate routines and are not changed.

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
