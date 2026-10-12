# Shopping Card — ACNH 3.0.3

Prototype 0.3.16, October 8, 2026. Original mapped main SHA-256:
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

The user then reported that 0.3.9 showed neither savings nor pocket Bells,
including outside shops. The entire shared counter component is implicated.
The 281-pane/320-section layout expansion is a suspect, but no native hard limit
or allocation failure was observed directly. The preview and shape tests failed
to cover resource loading. Do not treat their success as proof of native rendering.

0.3.10 restores the previously displayed 35-pane/74-section/7,996-byte layout.
After normalizing the retained Bells-badge shift (X120 -> 96), the exported
BFLYT matches the recorded SHA-256 of the 0.3.7 component byte for byte.
The dense geometry helper is removed from production; its implementation remains
in Git and the preserved 0.3.9 source archive. The earlier stepped corners return.
A conservative build budget prevents accidental expansion of this recovery
component; it does not describe a verified engine limit. Native loading/rendering
must be established before another smooth-icon approach is shipped. Payment,
catalog lifecycle and savings animation hooks are unchanged in this recovery.

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


### Compact rounded card geometry (0.3.11)

The user confirmed 0.3.10 restores the counter. Its three overlapping rectangles
per silhouette explain the large steps at the card corners. A replacement can
use engine-supported geometry without expanding the layout or editing BNTX.

In the supplied 3.0.3 main, 00268550 recognizes section tag `spi1` and stores
its resource pointer at BuildResourceSet+18. 002649C0 resolves a shape index via
u32 offsets at section+0C. A round-rectangle entry is `<HHII>`: type 1,
reserved 0, integer radius, integer corner subdivisions. The section includes
a count/reserved pair at +08 and offsets relative to its start at +0C.
No vanilla layout in the supplied dump uses this feature; it is supported by
the compiled ui2d engine. These field interpretations are verified for this
build only, not asserted as a general BFLYT specification.

The Picture resource constructor at 00282D30 reads UV count at resource+66
and flag byte +67. Bit 0 selects a shape index u32 following the UV array
(+68 when count is zero). 00282EE8 looks up the shape, normalizes radius by
picture width/height and constructs RoundRectShape at 0028DB20. That object
has normalized corner diameters at +40/+44 and subdivisions at +48. Native
virtual buffer initialization at 0028E6A4 generates vertices and indices.

The three silhouette PICs are 108 bytes each, zero UVs/flag 1/index 0..2.
Dimensions/radii are 94x66/15 (white), 82x54/9 (dark), and 76x48/6 (green),
with 16 corner subdivisions. White backing stays behind the oval; front card
and stripe/chip/text details retain the same centered transform. New component:
29 panes, 69 sections, 7,444 bytes. The historical shopping_shape_arm_test.py checked actual
native parser/PIC-selection/mesh execution at two bases and bounded buffers.
Its allocations and sin/cos imports are modeled, material/GPU/full-layout
loading is not. The local raster preview cannot establish GPU antialiasing.
The subsequent live screenshots did not render this shape path properly.


### Shape material colors (0.3.12)

The user screenshot of 0.3.11 shows a white silhouette and detail bands rather
than a colored card. The 0.3.11 tests verified shape decoding and triangles but
stopped before material setup. Native Picture's shape drawing branch at 00283E0C
passes color mode 2 to Material::SetupGpuConstantBuffer at 00278410, while the
ordinary quad path uploads the pane's four corner colors. Shape mode skips those
vertex colors entirely. The three silhouettes shared ShoppingCardSolid, whose
foreground was white. Native material setup therefore produced white for all
three despite dark/green pane colors. The preview incorrectly used pane colors.

0.3.12 keeps the same shape records and geometry, adds three 44-byte solid-color
materials (white/dark/green), and makes shape pane vertex colors white. Ordinary
quad detail colors keep their working path. Original materials stay byte-identical;
29 panes/69 sections/7,588 bytes. Native Material ctor at 002745C0 decodes the
foreground RGBA from resource+28 (hex), via the color header at +20. Material's
runtime foreground is at +0C. 00278BDC writes normalized foreground constants
for the zero-texture path. Its fragment constant destination is the mapped GPU
pool+38 pointer plus Material+34.

The expanded shopping_shape_arm_test.py executes the real material constructor,
Picture drawing setup (00283B50) from entry to return and native material/color
routines at two bases. Shader initializers/selection, transforms, constant-buffer
allocation, matrix binding and GPU upload are stubbed in addition to the earlier
allocation/trig boundaries. Native half-width/half-height shader constants are
also checked against the exported shape dimensions. Pink pane colors intentionally disagree
with materials: the shape constants remain the correct white/dark/green. A white
material override reproduces the old failure. This closes the color coverage gap;
GPU drawing, full-layout loading and antialiasing still require an Eden playtest.


### Ordinary textured card (0.3.13)

The user screenshot of 0.3.12 has corrected colors but still shows separated,
distorted geometry. CPU mesh/material tests did not establish the full shape
shader/transform path, and its exact rendering fault remains unproven. Stop
using that path. The current builder appends two standard textured PICs cloned
from P_Icon_00 (136 bytes, one full-image UV set, shape flag zero). Materials
preserve native filtering, wrapping and shader options, changing only texture
indices. No runtime hooks/payment behavior change in this version.

The foreground card is a single 256x192 RGBA8_UNORM texture on a 128x96 pane:
82x54 dark rounded silhouette, 76x48 green face, stripe/chip/detail marks baked
into one image. A separate 94x66 white silhouette draws before the oval. Both
share the existing centered/tilted transform. Colors are stored in the same
linear domain as prior pane colors; preview converts to sRGB illustratively.
Four-by-four coverage samples per texture pixel supply smooth alpha corners.
The picture textures use Tegra block-linear storage with one-GOB block height.

The hash-checked embedded BNTX originally contains seven textures. Rebuild its
name table, ResDic, BRTI descriptors and two-section relocation table with two
additional textures; preserve the original seven image payloads and format,
dimensions, channels and tiling fields exactly. Dictionary key references use
low bits first starting at the end of each string. The builder reproduces all
original dictionary nodes, then extends them for the new names. Pointer offsets
and bounds are independently checked against the serialized relocation table.

shopping_texture_arm_test.py executes the game's actual inline dictionary
traversal at 00263368/00263410 (from ArcResourceAccessor) at zero and 7100000000
bases. All original/new names resolve to their expected indices and absent
keys fail. SDK relocation and imported strcmp are modeled; GPU is not executed.
The inverse GOB decoder walks physical bytes independently of the encoder and
checks the full bitmap, semantic detail pixels and partial corner alpha.
The preview resolves actual exported texture/material bindings and image bytes,
not just the source drawing formula. Layout is 24 panes/63 sections/7,100 bytes;
original cash presentation, 16:9 canvas/root, badge shift and animation remain.
Eden still must verify GPU loading, rendered icon, cash visibility and feedback.


### Counter allocation allowance (0.3.14)

The user reports the entire balance counter missing in 0.3.13. A previously
unchecked layer is the game's resource-size table. The supplied Yaz0-wrapped
RSTC has CRC32 867463D4 for normalized path
`Layout/LBellCounterRT.Nin_NX_NVN`: size 00013080 = 77,952 and flag zero.
Original compressed asset expands to 69,760 bytes (8 KiB of budget overhead).
0.3.10/0.3.12 expand to 73,856; 0.3.9 to 98,432; 0.3.13 to 463,528. Thus both
missing-counter experiments exceeded the original allowance. This establishes
an allocation mismatch, though the user's process was not traced directly.

Native 03E3C8DC parses RSTB/RSTC, retaining RSTC count/records at manager+68/+70
and named records at +78/+80. 03E3CAD0 takes manager, result and SafeString path:
CRC32 over path+8, then binary search of the sorted records. On success it
writes uint32 size at result+0, metadata byte at +4 and returns true. False
leaves output unspecified. Resource request setup at 03E36E30/03E36560 queries
this result before selecting/creating its resource heap; compression suffixes
are normalized by native 03E362C0. No broad filename matching is appropriate.

CounterResourceBudget calls the original query, then matches the exact path
and raises size to at least 1 MiB. Preserve larger overrides/metadata, and supply
size+zero flag for a missing counter entry. Unrelated output/return values are
untouched. No global RSTC asset is shipped, avoiding conflicts with other mods.
The package builder refuses archives whose expanded size plus 8 KiB exceeds
this floor. The margin also accommodates added layout objects.

Core's optional prepareResources callback runs for installed package markers
before behavior configuration gating. Shopping Card independently fingerprints
and installs its allocation hook once; disabled/invalid/old configuration still
needs it because RomFS layout overrides remain installed. Removing the package
means no allocation hook or behavior hooks after restart. Financial/UI behavior
is still wholly gated by valid enabled compatible configuration. Resource
fingerprint failure prevents both support and behavior installation; behavior
fingerprint failures leave only the safe, narrowly scoped allocation support.

shopping_resource_arm_test.py executes native CRC32/RSTC lookup and the final
compiled callback with the supplied table at two bases. It reproduces 77,952
vs 463,528 and verifies the floor, missing entries, larger mod overrides/flags,
exact-path isolation, disabled behavior, removal and fingerprint conflicts.
The texture test now runs actual SDK RelocationTable::Relocate at SDK+81A90
(build A226048605D3597F90CB1067D141CD36) and compares all relocated bytes to
its model. NSO sections are independently decompressed/hash-verified. Native
texture-name traversal also passes. GPU initialization and live visibility
remain unverified until the next Eden playtest.


### Card detail polish (0.3.15)

The user confirmed 0.3.14 works. Move both decorative card-detail lines up six
layout units (approximately four screen pixels at 1280 width): centers -12/-19
become -6/-13. This centers the pair vertically within the area below the stripe.
Change the border's linear RGBA from (10,12,8,255) to (5,18,16,255), a dark
blue-green that complements the green face. These are foreground texture pixel
changes only; white backing, pane transforms, dimensions, texture formats,
resource allocation support and transaction/display hooks remain unchanged.


### Card detail polish (0.3.16)

The previous border still appeared black in-game. Increase its linear RGBA to
(12,50,42,255), making it visibly lighter and more blue-green while retaining
contrast with the card face. Raise both detail lines another 1.5 layout units
(one screen pixel at 1280 width), from -6/-13 to -4.5/-11.5. Only foreground
texture pixels change. Keep the working layout, backing and resource support.


### Optional gold palette (0.3.16 assets, October 9, 2026)

The user confirmed 0.3.16's appearance and requested a vanilla-colored gold
variant. Keep green as the main suite default, with byte-identical generation.
Pass a named palette through the existing layout/texture builders. The gold
counter/caption and cream rim/badge copy the original T_Bell_00 and
W_BaseL_00LT linear material colors (130,97,16,255) and (255,241,195,255).
The card uses the same gold face, a darker gold border (70,48,6,255), cream
details and the existing chip color. Shape, line positions and texture
descriptors stay unchanged. Native cash panes/materials/textures are preserved.

package_shopping_card_colors.py creates one optional archive with Gold and
Green choices. Each choice merges a single resource into the existing Shopping
Card folder, avoiding competing mods that override the same resource. Green
is checked byte-for-byte against the released 0.3.16 suite. No Core or marker
version change is needed. Existing layout/texture checks accept --palette gold;
the preview now reads serialized window colors instead of a hardcoded green.


### Gold revision 2: bell-bag face

The user clarified the card should match the light bell bag, rather than the
HUD's darker gold fill. The supplied Nook Shopping screenshot's bell-bag body
samples around sRGB (249,191,116); convert to linear (242,133,45,255) for the
card face. Retain the existing counter palette, border, stripe, details and
chip. This changes only foreground color pixels; serialized geometry, alpha
coverage and original resources remain identical to revision 1. The optional
packager accepts --revision 2 to preserve old archives without changing Core.


### Gold revision 3: user-selected #F9C687

Use the user's color-picker selection sRGB #F9C687 (249,198,135), converting
to linear RGBA (242,144,62,255) before writing the RGBA8_UNORM card face.
This replaces the screenshot estimate from revision 2. Other palette fields,
geometry, texture settings and the green default are unchanged. Package with
--revision 3 and retain all previous archives.


### Color options revision 4: Hotel Blue and Poki Red

The user requested two screenshot-inspired alternatives. Sample Hotel Blue's
counter (#4575F5), ticket face (#7094F9), and red accent (#EB4E4C). Convert to
linear RGBA for materials/textures and add navy edge #354F83. Separate the
stripe from the cream details with an optional Palette.stripe value, falling
back to the detail color for green/gold/Poki. Use the Poki bag's sampled coral
#F8A486 as its card face, with deeper warm-red #D96459 counter and #833F35
border; retain cream details and a gold chip. All are appearance variants of
the Bells savings HUD, without new currency behavior.

Builders keep the same transforms, texture descriptors, smooth alpha coverage
and allocation guard. Preview CLI choices now come from the palette registry;
optional archive names expand hyphens to spaces for Hotel Blue and Poki Red.
Package --revision 4 alongside previous downloads. Core remains 0.3.16.


## 0.3.23 combined funds and native cash priority

Affordability/HUD now combine Savings + pocket Bell-bag items + wallet Bells.
The displayed target clamps to 999,999,999 (the original 24-byte UTF-16
capacity), but transaction arithmetic uses the full unsigned total. The
maximum supported total remains below INT32_MAX.

Wallet `0257ABF0` already includes Bell bags. `0257AD90` returns wallet space
(99,999 minus wallet Bells), letting the card separate sources without
reading raw inventory or encrypted save fields. Native debit `0257B340` /
`026FDDC0` spends wallet first, selects smallest Bell bags, and returns
denomination change to the wallet. After withdrawing the savings portion,
debit old wallet + required bag portion, then refund old wallet minus the
intended wallet portion through native tracked addition `0257AF90` /
`0257B0B0` / `026FD5C0`. Wallet-only fallback uses the ordinary debit alone.
No temporary fake wallet balance or direct save/item write is used. Native
addition can repack cash into ordinary 99,000-Bell bags when change plus the
wallet refund exceeds 99,999; source balances then reflect native change.
The explicit 500k example leaves the 99k wallet intact. Purchase records and
item delivery remain in their original callers. Inventory space remains a
conservative preflight; do not promise slots that change may refill.

`03025400` is the native number-feedback callback. While that counter's
shopping-card subtree is visible, its cash callback only formats the hidden
cash text; the combined transition calls the original feedback directly.
This avoids duplicate cash/card sounds and feedback above the display cap.
Other currency counters and ordinary cash outside card scope retain feedback.

Host tests cover 5,040 payment boundary combinations. Compiled ARM tests cover
all residents, caller/room isolation, combined affordability, insufficient
funds, change, display saturation and native feedback. The native cash fixture
executes real cash/savings wrappers, debit, credit/overflow and encryption;
resident/parameter lookup and inventory iteration/insertion are modeled.
Live mixed-funds checkout in shops, fitting and both catalog entry points is
still required. Clean uninstall remains ordinary native money/item state.


## 0.4.23 Tom Nook storage expansion

The original `SNPC_rco_10_Common_ExpandHouseStorage` flow uses shared
`HasBellCount` and `BellCountDown` commands for all six fees: 500,000, 700,000,
900,000, 1,200,000, 1,500,000 and 1,800,000 Bells. Scope by the current command's
ResFlowchart name, not the executor's root, which can remain
`SNPC_rco_00_Common` while executing this subflow. No eventflow/MSBT edit.

Shared query dispatcher `0060FB10` reads QueryArg+10 -> ResFlowchart ->
length-prefixed ResString (`0060FBFC`). During this exact flow's HasBellCount,
allow its native Wallet return `00F7CD1C` to read combined funds. Restore scope
immediately after the query. The query retains the native enough/not-enough
branches and therefore cannot approve a partial payment.

Action dispatcher `0060F4F0` reads ActionArg+38 -> ResFlowchart -> ResString
(`0060F5EC`). BellCountDown's +30 method (`009DC7A4`) returns true, selecting
synchronous +58 execution (`0060F610..0060F6AC`). Scope the event identity only
across that dispatch. BellCountDown execution `00C97260` reads event at +8,
amount at +D8, and subtract flag at +100. It tail-branches to the shared cash
debit, so the old shop caller allowlist alone cannot identify storage fees.
Route nonnegative subtraction through the same savings-first payment helper
as other card purchases; leave addition and other flows native. No temporary
money, direct inventory writes, construction flags or loan rules change.

UIMoneyAppear/Disappear in this exact flow control per-resident card visibility.
Other commands retain visibility without reopening a dismissed counter. A
command in another flow, or event teardown `0060F340`, clears that event's
borrowed identity; it is never dereferenced later. The existing combined
counter/animation and cap apply. Four additional hooks are fingerprint guarded.

The new ARM fixture runs original native action/query dispatch, HasBellCount,
and synchronous action discrimination with synthetic residents and command
parameters. Native savings encryption runs; engine parameter binding and cash
services are modeled. Coverage includes all prices/residents, exact/one-short,
mixed funds, cancellation/display dismissal, teardown and unrelated fees at
two ASLR bases. Live storage purchase and HUD timing still need confirmation.


## 0.4.24 Correct serialized storage command names

Live 0.4.23 feedback: Tom Nook still displayed pocket Bells and rejected a
payment that savings could cover. Both are reproduced on the old compiled ELF
using the original BFEVFL command names: 2,000,000 savings + 77,777 wallet,
1,800,000 fee, card hidden and native insufficient result 1.

The decompiler slices `[15:]` from actions and `[14:]` from queries in
`decompiler/bfevfl.py`. Those shortened names are presentation only. The native
dispatchers receive `EventFlowActionUIMoneyAppear`,
`EventFlowActionUIMoneyDisappear`, `EventFlowActionBellCountDown` and
`EventFlowQueryHasBellCount`. The actual BFEVFL actor arrays and main's native
hash registries at 0487D8B0 / 0487D450 / 04880370 confirm them. Scope comparisons
now use the complete serialized names. Dispatch offsets, flow matching, money
helpers, display lifecycle and all other fee handling remain as before.

The earlier fixture used the same shortened names as the faulty comparisons,
so its native dispatcher coverage did not catch this boundary mismatch. The
fixture now reads actor action/query tables directly from the untouched local
storage BFEVFL and passes those strings into the original native dispatchers.
It additionally checks native name hashes against the original command
registries. The corrected fixture fails on 0.4.23 before the fix. This supersedes
the earlier synthetic-command test evidence; another live Eden check is needed.


## 0.4.25 Resolve the actual owning flowchart

Live logs confirm 0.4.24 Core loaded, Shopping Card enabled, and both symptoms
persisted. The second boundary mistake was treating ActionArg+38 / QueryArg+10
as ResFlowchart. They are ResEvent pointers; their first field names EventNN.
The native dispatcher builds diagnostics from an event name plus executor
root name, not from the current subflow. Earlier fixtures manufactured a
flow-name record in that event field, masking this mistake.

The published EventFlow implementation clarified the argument/accessor types:
https://github.com/open-ead/EventFlow/blob/master/include/evfl/Action.h
https://github.com/open-ead/EventFlow/blob/master/include/evfl/Query.h
https://github.com/open-ead/EventFlow/blob/master/include/evfl/Param.h
These were checked against ACNH's actual native instructions before use.
ParamAccessor constructor 0058A9E8 stores context at +8, node index at +10 and
node counter at +14. Native 0058AA18..0058AA38 follows context+38 node buffer,
32-byte indexed node -> first pointer FlowchartObj -> first pointer
ResFlowchart, then uses its events+30 and node event index+14. The flow name
is at ResFlowchart+20, confirmed independently in the original BFEVFL.

StorageFlow now follows that chain. Action accessor begins at argument+0;
query accessor begins at argument+28. Validate node bounds/counter, EVFL magic,
event bounds and equality between the current node's event and Arg.res before
matching the flow name. No borrowed context/resource pointer is retained.
The original command-name and synchronous payment fixes remain necessary.

The fixture now relocates the complete original storage BFEVFL using its RELT.
Native construction windows 0058D65C..0058D688 (query) and
0058DC50..0058DC90 (action), including the real ParamAccessor constructor,
produce the arguments. Native GetParamCount independently resolves the actual
parameter metadata through that node. These native-produced args reproduce
both 0.4.24 failures; synthetic flow names in Arg.res are gone. A nonzero node
and unrelated executor root explicitly exercise subflow ownership. Native
constructor/accessor windows join the fingerprint set.

The integrated HUD fixture also connects storage dispatch to real BellPage,
combined text, savings debit, Down/Finish feedback and ordinary cash restoration.
Missing/out-of-range/stale/mismatched contexts are rejected. In-game validation
remains required; previous synthetic-context success claims are superseded.


## 0.4.26 Storage counter dismissal (2026-10-12)

Live 0.4.25 confirmation: storage payments/card now work. The user saw a brief
ordinary Bells counter after the card disappeared. `UIMoneyDisappear` cleared
storage presentation immediately, so the next BellPage update restored cash
panes while the native layout was still running its Out animation.

Storage sessions now distinguish active display from closing presentation.
Dismissal, leaving the storage flow, or event teardown drops the borrowed event
identity and active flag immediately, retaining only a per-resident closing bit.
The HUD keeps the card until native `02B12EAC` returns true. Its original body
follows BellPage+210 -> Page+3F8 -> layout+1B0, checks the animation object's +60
completion method, and requires BellPage state (+1B0) == 3. The predicate and
relevant field accesses are fingerprinted. No new hook, frame timeout, save
state or retained UI pointer is added. Affordability and debit scope are unchanged.
An explicit UIMoneyAppear in another flow clears closing presentation; a new
storage appearance starts a fresh session. Auxiliary counters ignore closing.

The storage/HUD fixture executes this original hidden predicate, modeling only
the layout engine's animation-completion answer. It checks multiple closing
frames, state 3 with unfinished animation, finished animation with nonhidden
state, restoration behind a hidden root, ordinary-counter takeover, event
teardown during Out and reopen during Out, at both ASLR bases. Native engine
animation playback is not emulated; live confirmation of dismissal remains
necessary.


## 0.4.27 Both counter updaters must preserve storage presentation

Live 0.4.26 feedback: pocket Bells still briefly appear. Logs confirm Core
0.4.26 and Shopping Card 0.4.21 loaded. The user clarified that the Shopping
Card must remain available in storage dialogue, with no ordinary Bells counter.
No UI events are removed: storage's UIMoneyAppear/Disappear still execute.

Reproduced the actual code gap on the 0.4.26 ELF with interleaved updates:
storage appear -> HUD -> auxiliary -> disappear -> HUD (card still shown) ->
auxiliary (cash restored before either Out finished). The 0.4.26 fixture only
called the HUD during dismissal, concealing this independent updater path.
Only that path honored closing state; it also cleared the global state as soon
as its own window hid, regardless of the other controller's lifecycle.

Closing state is now a bit mask of controllers that presented the storage
balance. Both update paths honor it, and each clears its own bit through its
original hidden predicate. The HUD uses 02B12EAC, the auxiliary uses 0301A690
(direct owner+210 layout -> layout+1B0 animation +60; owner+1B0 == 3). The card
remains selected on both paths while any presented controller is still closing.
If dialogue finishes before any first display update, both bits are retained.
No timers, hook additions, saved state, or UI/event pointer retention are added.
Native commands, payment routing and ordinary-counter takeover remain intact.

The revised fixture invokes both update paths before and after dismissal,
deliberately sharing their pane binding. It runs both original hidden predicates
with independent completion answers and verifies each completion order, multiple
Out frames, teardown, reopening and explicit ordinary-counter takeover. The
standalone reproduction bypasses imported test loops so new fingerprints do not
mask the old binary's presentation failure. In-game confirmation remains needed.


## 0.4.28 Use the actual UIMoneyDisappear completion condition

User identified the successful-payment Sub_Event12 boundary: the success
message is followed by UIMoneyDisappear and two reservation flags. No subsequent
UIMoneyAppear occurs in that block. Native action registry 0487F860 points to
constructor 01043414, whose vtable provides enter 00D18D50 and update 00D18E50.
This command is asynchronous (+30 returns false), requests dismissal, then
waits for the money controller at screen+288 -> +210 to finish.

The earlier meaning assigned to 02B12EAC / 0301A690 was WRONG. Both require
controller state 3; native state-registration strings identify this as
State::cPlay (04000077), not hidden. State 1 is State::cIdle (03FE3B76).
Auxiliary State::cDisappear is 5; BellPage State::cDisappear is 4. Their native
Out updates 0301B340 / 02B13B94 queue Idle (1) after the layout completion query
(+50). The previous tests supplied a guessed hidden=state3/virtual+60 interface,
so their passing result did not validate actual native disappearance semantics.
The 0.4.26/27 hidden-predicate interpretation and related success claims above
are superseded by this direct command/state-registration evidence.

Actual UIMoneyDisappear completion 00D18F18..00D18F38 requires both transition
slots (+1F0 and +1F4) to equal -1 and current state (+1B0) <= 1 (Init/Idle).
Storage dismissal now uses exactly these fields/conditions independently for
both presented controller paths. It keeps the existing card and native UI
commands; no static timers, eventflow replacement, new hooks, saved state or
items are involved. Fingerprints validate the native completion window and
both native Out -> queued Idle handlers instead of the misidentified functions.

Reproduced the 0.4.27 ELF restoring cash in Play while ORIGINAL UIMoneyDisappear
completion returned unfinished. The corrected fixture runs the original
00D18F14..00D18F3C completion window directly, with actual controller fields,
and checks presentation against its answer across six states and both pending
slots. It covers Play -> queued Out -> Disappear -> queued Idle -> stable Idle,
interleaved updates/either finish order, reopening, teardown and thirty commands
before the first display update. Full layout playback remains outside emulation;
live confirmation is still required. Payment/source priority is unchanged.


## 0.4.30 Silent entry with native cash and stale combined balances

User reports an intermittent ka-ching on entering Nook's Cranny, absent on
immediate repeat visits. Reproduced two paths in the compiled 0.4.29 ELF:

- CashNumberFeedback only suppressed native feedback when the card pane was
  already visible. A native cash callback before the first presentation update
  reached the real native DownFinish callback even though the card should own
  this counter. It now also suppresses feedback when CurrentShop is supported
  and this bound counter has both custom card and savings-number resources.
  Formatting still executes; missing resources retain native cash feedback.
- SavingsAnimation only reset on a HUD update outside card scope, a different
  shop key or resident. With no outdoors HUD callback, changed funds on returning
  to the same shop animated against the previous visit. An auxiliary update
  before HUD could already expose the card. The old ELF produced Up feedback
  in this sequence. HUD Init/Idle/Appear now seeds silently, as does a freshly
  hidden/reconstructed card pane, without retaining UI pointers or using time.

BellPage registration at 02B13434 supplies state 2; 02B13454 names it via
03F87A5A (State::cAppear). Existing Idle/Play/Disappear meanings remain intact.
Only opening seeds silently; visible purchases/sales retain native stepping,
Up/Down/Finish and sounds. Storage dismissal still waits for Idle with no
pending transitions; the user confirmed that 0.4.28 fix works in-game.

Expanded compiled regressions cover all supported shop hashes, two bases,
auxiliary-first opening, both balance sources changing without outdoor HUD
updates, interrupted animation, fresh Play panes and native resource fallback.
The previous ELF fails the new entry-feedback assertion; a separate definition-
only fixture reproduced its stale combined Up animation. These establish the
code paths, not the exact live trigger. Live 0.4.30 sound confirmation is pending.
