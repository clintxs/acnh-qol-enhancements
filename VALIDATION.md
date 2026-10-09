# Unified QoL validation — 0.3.30 DIY preview rendering and return

Validated locally on October 9, 2026 against the supplied original ACNH 3.0.3
main (build FF1D1C05670DB6021C85B624A710B963). The user confirmed the player
dialogue fix and subsequently the corrected 0.2.3 NPC diagnostic build work in Eden. The user also confirmed Nook’s Cranny transactions in Shopping Card 0.3.1.
The user confirmed the corrected savings HUD in 0.3.4 and balance animation
in 0.3.5, the restored counter in 0.3.10 and the 0.3.14 allocation fix.
Localized name capture, configuration layering, aspect-ratio placement and Able
Sisters still need a live check. No running game or save files were accessed
by the local tests below.

## Build and loader

- Pinned devkitPro Docker build, networking disabled: passed with `-Wall -Werror`.
- Core deploys only `subsdk6` plus the existing minimally adjusted `main.npdm`.
  No package contains subsdk9; avoiding the slot does not prove future
  Jubbsticks Additions hook/NPDM compatibility.
- All 64 combinations of six feature packages at two ASLR bases passed.
  `exl_main` installs only the content-mount continuation hook and performs no
  configuration reads. Activation executes the real compiled registry/parser and
  installers; a second activation performs no additional reads or installations.
- Full suite installs 70 disjoint modeled hook ranges (Core discovery 1,
  DIY preview 4, Catalog 16, Pockets 16, Faster Dialogue 7, Fitting Room 11,
  Amiibo 1, Faster Crafting 2, Shopping Card 11 behavior + 1 allocation support), plus the private
  fitting input vtable entry. No embedded binary replacement arrays, IPS or
  dialogue code-cave helper remain in the collection.
- Per-feature absent, disabled, invalid, missing-version, oversized, below-minimum
  and newer-version configurations were checked. Newer valid packages warn and
  run. Below-minimum packages leave feature code untouched. Open/read/size errors
  close handles where appropriate and disable requested behavior. Installed Shopping Card assets retain their
  independently checked allocation support, including invalid/disabled metadata,
  because RomFS overrides still load; absent packages install no support. Occupied
  startup/hook locations fail their instruction checks; dialogue validation
  rejects altered hook sites or legacy instruction changes before any of its
  hooks are installed. Original code-cave padding remains untouched.
- Every installed module reads its regular INI and then its optional custom INI,
  including disabled regular configurations. Shared enabled/version overrides,
  empty overlays, absent/invalid markers, atomic rejection of invalid/duplicate
  metadata, missing/read/size/open failures and once-only reads are tested at
  two load addresses. A custom file can re-enable installed code but cannot
  activate an absent module. Dialogue tests cover inheritance, false overrides,
  list replacement/clearing, invalid/duplicate preference fallback, normalization
  after merging, and the same keys in either file. Module packages from 0.1.0
  through the current version remain accepted under minimum 0.1.0 for the
  original five modules; Shopping Card requires minimum 0.3.2 (currency resource required).
- Compiled Amiibo C++ callback feeds 9,000 into the unchanged native halfword
  timeout store at both load addresses. The original immediate is untouched.

The loader harness models FS services, code-hook allocation, writable aliases
and cache services. It does not execute Eden's loader or establish that the
content-mount continuation is safe in a live process. Exlaunch's existing inline
adapter is selected with full SIMD-register preservation for discovery. The
SIMD context declaration was corrected to match the assembly stack layout, with
compile-time offset assertions. Ordinary InlineCtx layout is unchanged.

## Shopping Card

- 0.3.7 covers regular Nook Shopping/catalog orders for yourself. Catalog gift
  sending uses a separate native payment path and is outside this prototype.
- At two ASLR bases, the catalog affordability caller reads each of eight
  residents' savings in every modeled room and outside shops. The original
  accepted-order BL executes the compiled debit hook and native encrypted
  withdrawal; pocket funds remain unchanged. Invalid residents and adjacent
  unlisted callers retain native routing. Empty savings returns zero even
  with cash available. Native price comparison and the 20% Bell Boom adjustment
  execute unchanged at below/equal/above thresholds through the full savings cap.
- The 0.3.7 same-context root-list fixture passed, but the user reported cash
  still displayed outside Nook’s Cranny. 0.3.8 removes that context assumption.
  At two ASLR bases, the revised HUD test executes native Catalog open/close
  routines and the original BellPage update with deliberately separate contexts.
  It checks all eight residents outside shops and in Resident Services,
  savings Down/Finish feedback after ordering, silent unchanged/reopened balances,
  cash restoration on close, direct/deleting destruction and resident isolation.
  The deleting destructor's original BL enters the compiled teardown hook.
  Native open/close engine calls and destructor internals are modeled at their
  boundaries. No live Nook Stop/phone gameplay is claimed.

- Compiled C++ hooks run at two ASLR bases for both Nook's Cranny rooms and
  Able Sisters. Eight separately modeled resident accounts retain independent
  balances. Every allowed balance/debit caller is covered; ABD, pocket and
  unrelated caller/room paths retain the original wallet service. Missing room
  state and invalid residents do not route to savings.
- Tests execute the original savings read, deposit, withdrawal and encrypted
  money routines, including native zero/cap behavior; only resident lookup,
  current-field type lookup and RNG service are modeled. No actual save is used.
  The original hash routine verifies all three room-name constants.
- The original ShopSellDone payment branch executes with the compiled capacity
  callback's result and deposits the full test proceeds into the same resident's
  savings. The cShop capacity query selects ordinary sale success text (case 0)
  for every resident. Actual positive proceeds still follow the native savings
  branch. Other capacity modes/shops retain the native query result. No housing
  prerequisite or pockets-full explanation is introduced. Native cap clamping remains.
- Purchase-space callback passes zero Bell payment and unchanged ticket count
  to the original space helper, preventing false slots from Bell bags that
  savings payment does not consume. Shared Fitting Room affordability uses
  savings only while Shopping Card is installed and in a supported shop.
- Every Shopping Card instruction fingerprint is individually corrupted; each
  conflict prevents financial/UI behavior hooks; independent allocation support
  stays active for installed assets. Its own entry conflict prevents all support
  and behavior.
- Existing native fitting checkout and feedback regressions are rerun with the
  shared account accessor. Actual shopping gameplay, native field state and
  both modules together require the live checks below.

- Original generic and top-right cash refresh branches execute with the
  compiled wallet hook, different savings/cash balances, and matching native
  cash caches. Neither queues the amount animation that caused the reported
  pocket-close ka-ching in 0.3.1. A positive control with a real cash change
  still queues the original animation; no sound routine is muted.
- The user reported that 0.3.3 still showed pocket Bells in Nook’s Cranny.
  The earlier test covered an auxiliary controller only. 0.3.4 also hooks the
  actual BellPage HUD update at 02B13BF0, resolving its /L_Bell_00 counter from
  the Page/layout parameter table and reading resident+224 rather than +230.
- New checks execute the original BellPage update from entry to return at two
  bases, with only engine lookup/text boundaries modeled. All rooms/residents,
  an intentionally wrong alternate resident field, savings purchase/deposit
  refreshes without cash animations, cash restoration, and genuine cash-change
  animation are covered. This closes the earlier auxiliary-only coverage gap.
- Compiled savings display uses each controller’s current resident,
  resolves both direct/group text adapters, shows full zero/cap balances,
  restores cash panes outside supported shops, and tolerates absent resources.
  Native text setting, typed parameter lookup and pane lookup are modeled;
  0.3.10 recovery was confirmed in Eden; the user confirmed the
  balance animation in 0.3.5.
- 0.3.5 tests execute native 03353B14 number stepping and 03025400 feedback
  through its Up/Down/Finish animation dispatch and positional sound setup,
  with engine formatting, animation playback, position query and thread index
  boundaries modeled. Purchases count down, deposits count up, and completion
  selects the correct Finish cue. Full-cap-to-zero, mid-flight reversal and
  one-Bell completion pass at two ASLR bases. Initial display, resident/room
  switches, settled updates and auxiliary refreshes produce no duplicate cue.
  Hidden cash text is restored after feedback; native cash current/target and
  cached amounts remain unchanged. Actual rendered animation and audible sound
  are not established by these engine-boundary tests.
- Resource checks preserve every existing pane/material and font/animation archive
  member byte for byte. Original texture settings/payloads are preserved inside
  an extended BNTX container. The added subtree starts hidden, carries an independent
  24-byte amount buffer, and uses native rounded windows and ordinary textured pictures.
  The eight-unit oval rim, centered card transform, dark foreground card
  edge, separate white backing behind the oval and right-hand Bells badge
  are checked. Both later icon experiments failed live despite local checks:
  0.3.8 flattened the card; 0.3.9 made both savings and normal cash counters
  disappear everywhere. Its geometry preview established shape coverage only,
  not native resource loading/rendering. That build increased the component
  from 35 panes/74 sections/7,996 bytes to 281 panes/320 sections/33,580 bytes.
  Layout expansion is a suspect; no native pane/allocation limit is established.
- 0.3.10 restores the compact layout that previously displayed. A recorded hash
  of the 0.3.7 exported BFLYT verifies the recovery byte for byte after normalizing
  its sole intentional difference: Bells-badge X120 -> 96. The existing native
  resources, materials, controls and animations remain unchanged. A conservative
  project budget enforces 35 panes/74 sections; it is not an engine-limit claim.
  The original 1920x1080 canvas/root and the catalog lifecycle hooks remain.
  Local ARM regressions verify cash presentation outside supported contexts,
  savings presentation within them, amount feedback and teardown. These model
  the layout/engine boundaries. The user subsequently confirmed the restored
  counter works. That recovery retained the earlier stepped icon corners.

- 0.3.11 uses native `spi1` shape resources and PIC flag bit 0, with three
  RoundRectShape records for white backing/dark edge/green face. The component
  shrinks to 29 panes/69 sections/7,444 bytes, below the live-confirmed recovery's
  budget. Original panes, materials, textures and animations are preserved.
- The 0.3.11 ARM test executed the original section reader at 00268550, PIC shape
  selection at 00282D9C, shape resolver/constructor and RoundRect mesh generation
  from the exported resource at two ASLR bases. Only allocation and imported
  sinf/cosf are modeled; loader relocations are supplied. Each silhouette yields
  84 vertices and 210 indices, with checked radius normalization, corner-circle
  positions, index bounds and allocation guards. It does not execute the full
  layout constructor, material/shader setup or GPU drawing; an Eden playtest
  is required to establish appearance and continued visibility. Raster preview
  supersampling is illustrative and does not prove native antialiasing.

- The user reported 0.3.11 no longer looked like a card. Its screenshot shows
  white shapes/detail stripes with no green face or dark edge. Mesh-only tests
  missed the native drawing path's color behavior: Picture's shape branch at
  00283E0C selects material mode 2 and skips the quad vertex-color constants.
  All three silhouettes shared the solid-white material, so their pane colors
  could not supply the intended dark/green colors.
- 0.3.12 adds a separate foreground material for each silhouette, with white
  pane colors. Layout count stays 29 panes/69 sections; size is 7,588 bytes.
  The 0.3.12 tests executed native material decoding (002745C0), Picture drawing setup
  (00283B50) from entry to return, material setup (00278410), and foreground constant writes (00278BDC)
  at two ASLR bases. Deliberately conflicting pane colors leave the correct
  normalized white/dark/green material constants; a shared-white material
  reproduces the 0.3.11 failure. Existing mesh, index/area/guard checks remain.
  Shader initialization/selection, transforms, constant-buffer allocation,
  matrix binding and GPU upload/draw are modeled;
  those CPU tests do not establish live appearance or antialiasing. The raster
  preview now reads material colors for shapes instead of their ignored pane
  colors. The user subsequently reported split, distorted card geometry.
  These shape-path tests did not establish GPU rendering and are superseded
  by the texture checks below.

- 0.3.13 abandons the unused native shape path. Two ordinary 136-byte PICs
  clone the native Bells icon's material/sampler and full-image UV coordinates.
  The entire foreground card is one RGBA8 image; its white backing is another,
  drawn before the oval. The layout has 24 panes/63 sections/7,100 bytes.
- Both textures are appended to the embedded BNTX. Original seven payloads and
  format/dimension/channel/tiling fields remain byte-identical. Checks cover
  required relocation pointers and their section bounds, both new descriptors,
  full independent block-linear decoding, stripe/chip/detail colors and partial
  alpha coverage at rounded corners. Native 3.0.3 dictionary lookup (00263368
  and 00263410) resolves all original and added texture names at two bases and
  rejects missing names; strcmp is modeled. The actual hash-checked 3.0.3 SDK
  relocation routine (00081A90 in SDK) now relocates both original and extended
  containers and agrees byte for byte with the relocation model (including the
  relocated flag). SDK NSO segments are decompressed and hash-verified locally.
- The preview resolves exported PIC/material/texture references and decodes
  packaged image bytes. Its oval is illustrative. GPU loading/sampling/drawing,
  native output colors and continued counter visibility require an Eden check.

- The user reported all currency display missing in 0.3.13. The native RSTC
  entry for `Layout/LBellCounterRT.Nin_NX_NVN` is 77,952 bytes; vanilla expands
  to 69,760, 0.3.10/0.3.12 to 73,856, 0.3.9 to 98,432, and 0.3.13 to 463,528.
  This is a concrete allocation mismatch missed by prior structural tests.
- 0.3.14 adds a trampoline at native resource-size lookup 03E3CAD0. It executes
  the original lookup, then raises only the exact normalized counter path to
  at least 1 MiB. Larger sizes and existing metadata are preserved; missing
  entries receive the floor with flag zero. No shared RSTC file is replaced.
  The builder checks expanded resource size plus native overhead fits the floor.
- The new resource ARM test executes actual CRC32 and RSTC binary search with
  the local original table and compiled callback at two bases. It reproduces
  the insufficient original budget, checks the new floor, exact-path isolation,
  larger overrides/metadata, missing table, disabled behavior and removal.
  Resource-entry fingerprint conflicts reject support and all behavior.
- Resource preparation is independent of financial/UI behavior: while the
  package marker remains installed, its enlarged assets can still load with
  disabled, invalid or incompatible configuration. Preparation is once-only;
  absent packages install no hook. Core subset/metadata/overlay tests cover
  this exception without enabling transactions or savings presentation.
  Actual live counter visibility and GPU rendering still need an Eden check.

- The user confirmed 0.3.14 works. 0.3.15 changes only foreground texture
  pixels: detail-line centers move from Y -12/-19 to -6/-13 and the border
  changes from linear RGBA (10,12,8,255) to (5,18,16,255), a dark blue-green.
  Original textures, backing, layout structure, allocation support and payment
  behavior stay unchanged. Existing packaged-pixel checks follow the new centers;
  the exported texture preview is inspected before packaging.

- 0.3.16 raises the line centers another 1.5 layout units to Y -4.5/-11.5
  (one screen pixel at 1280 width) and changes the border to linear RGBA
  (12,50,42,255). The previous shade still appeared black in-game. Only the
  foreground texture pixels change; the packaged preview is inspected locally.
  Final color and placement still need an Eden check.

## Conditional dialogue

- Host settings/policy checks cover defaults, friendly aliases and resource IDs,
  case-insensitive deduplication, UI/player independence, no stacked boost, 64-ID bounds,
  invalid values/keys/list entries, empty lists, duplicate keys and size limits.
- 6,454 compiled ARM cases at two ASLR bases execute the original native input,
  reveal and renderer gate code with the real compiled SIMD adapter/callbacks.
  No-input results match native held fast; B/R uses the extra tier. Protected
  modes, forced native display, disabled acceleration, scripted overrides,
  countdown handling and parser call counts remain unchanged.
- Input matrices include B/R, excluded L, unrelated buttons, prior latch,
  active/fallback/missing controllers and release. Controller bytes are unchanged.
  Main/renderer matrices use five live fast rates, scales and completed states.
- Native-produced owned speaker handles and modeled registry entries cover rcm/rct/dod/doc/kpp,
  kppB, another NPC, a villager, player narration, dialog/balloon/unknown,
  missing or invalid speaker, stale generations, retiring entities, unrelated
  entity kinds, NPC RTTI mismatch/uninitialized/missing/foreign methods, and
  overlong resource names. Sequences switch speakers and
  repeatedly hold/release; borrowed speaker/registry bytes remain unchanged.
  Kapp'n's friendly alias includes both known SpNpcParam profiles.
- Player cases execute the original 01AF89A0..01AF8A84 pending-to-active
  bank copy, then the compiled classifier through the actual SIMD adapter.
  They cover all five self banks, missing speaker entities, UI/player settings
  independently on/off, B/R non-stacking, ignored pending IDs, transitions to
  unknown/NPC banks, bounded/overlong names, and read-only metadata. Known self banks use the player
  category even when the presentation is a system dialog. All five bank names
  were found in each of the 14 localized TalkFtr/TalkSys resource sets.
- Speaker metadata lookup and parser completion are modeled; the owned handle is produced by original constructor instructions. Hook
  allocation/trampoline relocation is modeled; the actual SIMD adapter and C++
  callbacks execute. These tests do not validate physical controllers, audio,
  emotes, complete rendering or actual game context coverage in Eden.

## Existing behavior against the unified ELF

The inherited tests were adjusted for the new source/translation paths and
combined ELF. The Pockets fixture's synthetic adapters were moved beyond the
larger module's BSS; this was a test-memory placement change, not a gameplay fix.

- 192 version comparisons plus valid/invalid config boundaries passed.
- 392 stack-menu cases and 864 storage-policy cases passed.
- All fourteen caption locales, fallback handling, UTF-8, capacity and atomic
  parser rejection passed.
- 372,075 fitting-bag capacity/policy cases passed.
- 630 compiled Pockets ARM cases and 190 storage-hint cases passed at two bases:
  native Split, menu dispatch, translated captions at the new path, withdrawal,
  Y-to-storage lifecycle, native footer allocation and caption routing.
- Fitting Room native checkout tests passed at two bases: exact variants,
  outfit/bag overlap, wear/send choices, storage/pocket/mixed delivery, tickets,
  purchase records, metrics, repeat guards and preflight rejection. Engine/save
  APIs are simulated. Native scale/dirty-transform, alpha/removal and cursor-tap
  feedback checks also passed.
- 1,926 dialogue/ground catalog cases, 44 UI-result source cases, 148 variation
  label cases, 32 Miles name-binding cases and 54 counted-name cases passed.
  Real fence metadata/name types were exercised. Imported catalog fixtures also
  exercise their prerequisite native query, naming and badge checks.
- 26,077 fitting catalog-star cases passed, including all 5,207 clothing IDs,
  native selection frames for eight kinds, real USen names and two load bases.

These tests use original local game instructions/resources and modeled engine
boundaries. They do not confirm rendering, physical input, audio, real purchases,
real delivery/catalog saves, or complete emulator behavior.

## Packaging

- Exactly seven mod folders: Core, Catalog, Pockets, Faster Dialogue, Fitting Room,
  Amiibo and Shopping Card. Six versioned INIs; only Core has ExeFS files. The actual personal
  custom INI is absent from the suite; a root .example is inert. A separate
  optional personal-mod ZIP contains the requested preset and no executable or
  module marker, so replacing the suite folders cannot overwrite preferences. Optional modules are
  absent from Core's own RomFS, so removing/disabling one removes its marker.
- Six layout payloads match the current standalone binary ZIPs byte for byte,
  checked against independently captured SHA-256 values. A seventh layout
  belongs to Shopping Card and adds the initially hidden savings presentation.
- Catalog badge builders verify original resource hashes, archive changes and
  hidden initial badge panes. Fitting builders preserve other archive members.
- Binary ZIP contents and SHA-256 manifest are checked. Matching source excludes
  game resources, binaries, build outputs, Git data and caches. Packaging refuses
  to overwrite an existing release.

## Reproduce

From the workspace root, build with `sh qol-enhancements/tools/build.sh`.
Then run `python3 qol-enhancements/tools/check.py` and
`python3 qol-enhancements/tests/package_test.py`. The ARM emulator requires JIT
access outside the Codex filesystem sandbox on this Mac. Its Python packages are
currently at `Grab 5/working/python`; original local main/resources and the zstd
CLI are also required. None of those original game assets are redistributed.

## Required live checks

1. Launch Core alone. Confirm ENTRY, WAITING_FOR_CONTENT and BOOT_OK with zero
   enabled modules, and unchanged gameplay. Launch all six modules and confirm
   all package versions, ENABLED lines and BOOT_OK without installation errors.
2. Disable/re-enable each module in Eden, fully restarting each time. Verify
   code behavior and resource replacements follow the module toggle. Check a
   below-minimum INI and a newer INI produce the documented error/warning.
3. Check Catalog badges, scrolling, variants, Miles, recipe/ground stars and
   dialogue item sources. Check Pockets Grab 5/10, caption translations, native
   storage withdrawal, Y storage and footer hints with each module alone and
   together.
4. Test Faster Dialogue without a custom mod: base 2, held B/R 4, release and
   disconnect, multi-page dialogue, choices, A/B behavior, Animalese and emotes.
   Then enable the separate personal preset and restart: verify automatic 4
   for Timmy and Tommy independently, Orville, Wilbur, and Kapp'n (including
   alternate profile if encountered), plus blue UI/ABD dialogs. Independently
   enable faster_player for DIY workbench prompts/results, fish/bug/sea-creature
   catches and recipe learning; turn faster_ui off and check those self messages
   still boost. Reverse the toggles and check blue system dialogs separately.
   The corrected NPC diagnostic was confirmed working; verify
   unlisted villagers remain at base and no prior NPC boost carries into the
   next message. Check custom file edits, removal, module disabled, unknown IDs
   and invalid values across full restarts. Check regular-file preferences,
   custom inheritance and enabled overrides for other modules in 0.2.4. Kapp'n songs/travel should retain
   native scripting; UI recognition may need additional verified contexts.
   Check intended FPS-mod combinations separately; dynamic rates are retained
   but static hook fingerprints do not establish complete compatibility.
5. Use the existing save backup for Fitting Room purchases: bag plus outfit,
   overlap, 17–20 items, storage overflow, wear/send and Bell deduction/delivery.
   Confirm badge animation and catalog-star interaction in the fitting UI.
6. Check Amiibo late scans, timeout, B cancellation and retry. The standalone
   timer was confirmed at five minutes; unified runtime installation is new.

Switch hardware and other emulators/game versions are not validated targets.

7. Shopping Card: first confirm both ordinary pocket-Bells display outside shops
   and savings display in shops/catalog return after 0.3.9's disappearance.
   Check entering/leaving shops, opening pockets and opening/closing Nook Shopping.
   Use the existing save backup. Test a floor item and cabinet
   item in both Nook's Cranny versions, clothing from Mabel and the fitting room,
   then the fitting bag with both modules enabled. Compare savings at the ABD
   before/after; confirm pocket Bells and Bell bags do not change. Test cash
   rich/savings poor and cash empty/savings sufficient, cancellation, tailor
   tickets, full storage/pockets, wearing/sending, and several residents. Sell
   items/turnips at the counter and verify the deposit and ordinary thank-you message. Check
   drop-off box, ABD transfers, Harv and another vendor retain normal behavior.
   Close/reopen pockets repeatedly without a transaction; verify no false
   sound/amount animation. Check green savings display at zero and large
   balances, thicker white oval outline, centered dark-edged card and its white
   backing visible only above the oval; check the restored full-height card
   with the earlier stepped dark/white corners
   and the lower-right white Bells badge with green text shifted left about 16 pixels.
   Check at 1280x720 (16:9) and 1280x800 (16:10 with the existing aspect mod),
   including top/right margins, clipping, icon proportions and frame pacing.
   Keep any later 16:10 placement correction separate from the default package.
   Verify purchases count down with ka-ching, sales count up, rapid changes settle
   correctly, and first entry/resident changes stay silent. Check leaving both
   shops restores cash; pockets still show cash.
   At Nook Stop and in the phone app (including outdoors), first verify the
   savings display appears without visiting Nook’s Cranny. Order special goods
   and previously cataloged furniture/clothing for yourself. Compare savings,
   pocket Bells and delivered mail; check order limits, cancellation, insufficient
   savings despite rich pockets, empty pockets with sufficient savings and Bell
   Boom prices. Verify the card HUD and downward animation/sound, closing to cash,
   reopening silently and switching residents. Gift payment is unchanged and
   needs a separate implementation; check the native gift flow for regressions.
   Restart with Shopping Card disabled/removed and verify native payment.

## Optional Shopping Card colors — October 9, 2026

- The user confirmed the 0.3.16 green appearance. Add a gold option without
  changing the Core version or transaction hooks. The original Bells caption
  material supplies linear RGBA (130,97,16,255); its rim material supplies
  cream (255,241,195,255). These exact colors are checked against vanilla for
  the gold counter/caption and rim/badge. The card face uses the same gold,
  with a darker gold edge (70,48,6,255), cream stripe/details and gold chip.
- Existing layout checks pass for both palettes: original panes/materials,
  font/animation resources, 24 panes/63 sections, 7,100-byte BFLYT, native 16:9
  placement and scoped resource-budget guard. No geometry changes.
- Gold passes packaged pixel decoding, smooth corner coverage, original seven
  textures preserved and actual SDK relocation/native texture-name traversal
  at both existing bases. Its exported pixel preview was inspected. GPU color
  and final appearance still need an Eden check.
- The existing suite/package check passes against the untouched 0.3.16 ZIP,
  including byte-identical green layout. Optional Gold/Green choices replace
  only that layout inside the existing Shopping Card folder; their archive
  entries and SHA-256 manifest are verified after writing. Green restores the
  user's confirmed appearance. No executable or module configuration included.

### Color options revision 2 — lighter bell-bag card

- The user requested a lighter card matching the bell bag rather than the
  counter. Sample the light body in the supplied Nook Shopping screenshot:
  median sRGB (249,191,116), approximated in linear RGBA as (242,133,45,255).
  Apply this to the gold card face only.
- Existing gold structure/native-palette checks pass. Comparing against the
  revision 1 optional ZIP confirms only foreground card texture colors change:
  descriptors, alpha coverage, backing, counter/badge, layout, original
  textures and other resources are identical. All changed RGB channels are
  lighter. The exported preview was inspected; live appearance remains pending.
- Package as a separate r2 archive, preserving revision 1 and the 0.3.16 suite.
  The packager verifies the green restore option against the original release
  and verifies every ZIP entry after writing. Core remains 0.3.16.

### Color options revision 3 — requested #F9C687 face

- Use the user's selected sRGB #F9C687 (249,198,135), converted to linear
  RGBA (242,144,62,255), for the gold card face. Only this palette entry changes.
- Existing gold layout checks pass. Comparison with the r2 optional archive
  confirms identical layout/resources, texture descriptors and alpha coverage;
  only the foreground card's RGB pixels differ. The decoded face center
  converts back to exactly #F9C687. The exported preview was inspected.
- The r3 archive preserves previous downloads and includes the byte-identical
  green restore option. Core remains 0.3.16; live appearance remains pending.

### Color options revision 4 — Hotel Blue and Poki Red

- Add Hotel Blue and Poki Red alongside the existing green and gold options.
  Hotel screenshot medians: counter sRGB #4575F5 -> linear (15,45,233),
  ticket #7094F9 -> (41,76,242), red accent #EB4E4C -> (212,19,18).
  Use these for counter, card face and stripe, with navy #354F83 edge.
- Poki bag screenshot median #F8A486 -> linear (239,95,61) supplies its coral
  card face. Pair it with deeper warm red #D96459 counter and #833F35 edge;
  preserve cream details and the gold chip. Both themes retain Bells/savings
  behavior. No Poki or hotel currency hooks are added.
- Allow a separate optional stripe color, defaulting to the detail color for
  existing palettes. All four pass existing layout checks. Preview each new
  theme from the serialized materials/textures and inspect locally. Native
  texture tests cover each new palette's face/edge/chip/stripe/details and
  independent byte-order decoding, original textures and SDK relocation/name
  lookup. Actual in-game appearance remains pending.
- Revision 4 includes all four choices with readable Hotel Blue/Poki Red folder
  names. Green must match the main 0.3.16 ZIP byte-for-byte; green/gold also
  match revision 3. The package verifies all entries and its checksum manifest.
  Previous archives and the Core executable remain unchanged.


## 0.3.17 localized NPC names

- Two new C++ observation hooks execute during native name-label preparation,
  alongside the unchanged five speed/input hooks. No name lookup, message
  rendering, voice, emote or delegate behavior is replaced by the hooks.
- Tests execute the original name-holder construction, source+128 validity and
  dispatch, and actual UTF-16 data/capacity getters through the real SIMD hook
  adapter. Only the resource lookup inside the name delegate is modeled.
- 6,916 name cases passed. Real special NPC and villager names from all 14
  supplied locale archives feed
  the native holder and compiled capture code. Russian grammar tags with embedded
  zeros are removed. No extracted name tables are included in the release.
- Host checks cover valid/malformed UTF-8, UTF-16 surrogate pairs, missing
  terminators, malformed/oversized tags, bounded conversion, Unicode list entries
  and unchanged regular/custom configuration layering.
- Compiled name matching covers Blathers and accented/Japanese/Chinese/Russian
  names, names in either INI, owned copies after native stack reuse, unavailable
  labels, mismatched source and generation, retiring entities, UI/player scope
  and language-independent internal IDs. Existing aliases remain available.
- The prototype still requires a live Eden check: add Blathers to the existing
  personal list, restart and compare his text without/with B/R; also check a
  listed villager and an unlisted speaker. Alternate-language name capture is
  locally tested but has not been confirmed in a live game.


## 0.3.22 unnamed dialogue

- Rename the public preference to `faster_unnamed`, preserving `faster_player`
  as an alias. Host checks cover both spellings, cross-file overrides,
  malformed values and mixed-alias duplicates retaining the earlier layer.
- Reuse the existing seven C++ hooks. Name capture observes the same native
  null-data/zero-capacity/initial-NUL branches that suppress the name pane.
  Nonempty malformed names remain named even when UTF-8 conversion fails.
- Native ARM checks cover all eight reported message groups plus an unlisted
  group and variable labels in talk/balloon windows, both alias spellings,
  disabled settings, absent sources, independent blue UI, unobserved name
  preparation and message-bank/label/speaker-generation changes at two bases.
- The new longer bank cases exposed overlapping mock string/vtable storage;
  move the pending-bank test buffer away from the fake vtable. Production
  code does not use those test addresses.
- Existing Core activation/configuration and 6,890 dialogue reveal/input/rate
  cases pass, along with 6,916 localized name cases across all 14 supplied
  languages. These are local ARM checks; the new category still needs an Eden
  test with the actual mirror/storage/Niko/amiibo/stretching/drop-off prompts.


## 0.3.23 combined Shopping Card funds

This revision supersedes the earlier savings-only affordability and no-fallback
notes. Shops, fitting/bag checkout and regular catalog orders now check the
combined total and spend Savings -> pocket Bell bags -> Wallet. Sales still
deposit to savings. Cash change/refunds use native routines and may repack
remaining cash between wallet and ordinary bags. Capacity stays conservative.

- Host policy: 5,040 boundary cases, all three source priorities, insufficient
  combined funds unchanged, zero/exact/max amounts and debit/refund conservation.
- Compiled callbacks: all eight residents, two bases, every supported payment
  caller, 500k example, partial wallet fallback, change and overflow.
- Actual native cash/savings wrappers, selection/debit/change, addition/overflow
  and encrypted money writes execute at two bases for all eight residents.
  Resident/parameter lookup and inventory iteration/insertion are modeled.
  Ordinary non-money inventory items survive all cash operations.
- HUD shows all sources and clamps the displayed target at 999,999,999. Full
  funds remain spendable, including withdrawing a total above the display cap.
  The original nine-digit layout and all color options remain compatible.
- Native cash feedback is formatted silently while its card is visible; the
  card's native transition supplies one animation/sound. Leaving card scope
  restores native cash feedback. Existing silent pocket refresh coverage remains.

Eden still needs mixed-funds purchase tests in Nook's Cranny, Able Sisters,
fitting bag, Nook Stop and the phone app. Include full/near-full inventory,
change, insufficient total, cancellation, multiple residents and sale proceeds.

Release checks: the devkitA64 build and `tools/check.py core shopping fitting`
passed. All five palette structure checks and suite manifest/resource checks
passed; generated assets retain the previous nine-digit layout bytes.

## 0.3.24 DIY Recipes app previews

The feature is built into Core, with no optional module or new RomFS asset.
Selecting a recipe opens a phone-owned instance of the native workbench detail
page. It borrows the current app list for native left/right navigation and
material/item lookup. A private virtual table disables the craft focus target
and rejects crafting actions; the nested craft-button layout root is hidden.
B runs the return animation, releases the borrowed list and restores the app's
category, selected cell and scroll position. Native workbench objects are intact.

Local checks execute the compiled integration at two ASLR bases, with modeled
UI allocation, loading and animation completion. The actual game code handles
recipe copying (including all six material entries), list count, left/right
wrapping, item lookup, favorite-command forwarding and craft-action rejection.
Additional cases cover sorted/filtered pointer order, one-item lists, invalid
selections, allocation failure, duplicate confirms, reuse, per-resident sessions,
active/already-removed child cleanup, and all instruction-fingerprint conflicts.
Core's 64 package combinations and configuration/compatibility checks pass with
the three added built-in hooks. A preview mismatch leaves amiibo and optional
modules independent.

The 0.3.24 pinned Docker build passes with `-Wall -Werror`. All host checks and
the Core, Shopping Card, Faster Dialogue, Faster Crafting, Pockets, Fitting Room
and Catalog regression groups pass against that build.

Live Eden verification remains pending for model/resource loading, visible
layout/layering, rotation, physical input, B during loading, repeated app/phone
closure and normal workbench crafting/cooking. The local harness does not
establish those results. See `research/recipe-preview/NOTES.md` for native bindings
and the live test list. No running game, installed mod folder or save was changed.

## 0.3.25 DIY preview callback binding correction

The user reported that A in the phone app did nothing in 0.3.24. The callback
scanner rejected tables with more than 32 rows, but native layout initialization
at main+03759DD0 creates 35 (0x23) button-event rows. Consequently it never
replaced the phone's empty confirmation callback. The scanner now accepts the
native row count while retaining bounded dimensions.

The original test fixture supplied six rows and invoked the callback directly,
so it missed this integration failure. The revised fixture executes the actual
native layout-table constructor, asserts two columns/35 button rows and two
columns/five command rows, and feeds confirmation through native translation
(0375AB74), dispatch (0375AA40), and member-function delegates. Only allocation,
resource binding, source-index lookup and focus feedback are modeled at those
boundaries. The revised table fixture fails against the 0.3.24 executable and
passes against 0.3.25. Preview lifecycle/navigation/material/craft-rejection and
Core activation checks remain covered. The pinned Docker build and all host/Core
checks pass, including all 64 optional-package combinations at two load bases.
Live Eden input/rendering verification of this corrected build remains pending.


## 0.3.26 DIY preview construction/heap correction

The user reported a freeze on Flimsy axe with the phone grid still visible.
Their 0.3.25 Eden log shows a null instruction call, LR=83F1E0F8. With main
base 80163000, the native frames resolve to 03DBB0F8 (state-name assignment),
03363648 (first item-icon constructor), and 03492CAC (preview constructor).
The state-name array pointer is null before layout/resource binding occurs.

Native state initialization at 03DBAC90 uses TLS+0xA8 when a thread record
exists; it does not fall back to the default heap when that field is null.
A phone UI manager can inherit its heap (manager+0xB8 is null). The old scope
then left TLS unchanged and used libc malloc for the page. A null current TLS
heap reproduces a null array read at 03DBB0EC, immediately before the null call
in the supplied backtrace. The log does not contain the live heap values, so
this reproduces a sufficient cause rather than establishing allocation history.

The revised scope chooses the manager heap, then the current TLS heap, then
the heap manager's default heap. It restores the previous TLS value even when
that value was null. The page itself uses heap virtual+0x30 with alignment 8,
matching native page factories. Missing heaps/allocation failure restore phone
input instead of entering construction. Confirmation queues the operation;
creation and layout visibility changes run after native input dispatch returns.
Bounded stage messages identify heap selection and construction/binding/loading
progress if live verification exposes another dependency.

The regression executes actual native base/common preview construction, all
270 icon constructors, their state-machine allocations and state-name setting.
Only allocation backends, string assignment and low-level mutex/memory services
are modeled in this constructor path. It reproduces the original null-heap
fault independently, then checks successful construction using manager,
current and default heaps at both load bases. It verifies state arrays/names,
page bounds/ownership and TLS restoration. The previous model had stubbed out
icon construction and therefore missed this dependency.

The pinned devkitA64 Docker build passes with `-Wall -Werror`.
`python3 tools/check.py core` passes all host checks, all 64 module combinations
at two load bases, configuration/compatibility tests and the expanded native
preview regressions. Suite/source/custom ZIP integrity and suite SHA256 manifest
checks pass. Older release archives are preserved.

Live Eden verification of 0.3.26 remains pending. This fixes the reproduced
construction dependency; it does not prove rendering, rotation, asynchronous
resource loading or actual controller/B behavior in Eden. No installed mod,
running game or save was changed.


## 0.3.27 DIY preview initialization timing and active-page updates

The synced `logs/eden_log.txt` reports Core 0.3.26. Every selection logs
`allocating page` followed by `could not create native page`. The manager heap
is nonnull (214E07D318) and previous TLS heap is null. No preview constructor or
layout-binding stage is reached. This establishes allocation failure in the
live app, rather than missing heap selection or an item/model-specific fault.
The 0.3.26 null-TLS reproduction remains a defensive test; it did not establish
the live 0.3.25 allocation history. The app's UI pool was already shrunk, which
also explains why 0.3.25's libc-allocated page failed on internal allocations.

Native UI manager initialization at 0376D9A0 creates an ExpHeap (037C6D48),
builds/initializes its pages, then invokes heap virtual+0x28 = ExpHeap::adjust
(037C75D4). Adjustment returns unused capacity to its parent. The new Core hook
on page initialization (0376890C) identifies only the MenuRecipePage vtable and
prepares its native preview child during this setup interval. It leaves the
preview inactive (lifecycle 2) and activates it only after confirmation dispatch
finishes. This uses the app's reserved construction heap and native ownership;
it does not increase a global resource budget or allocate on first selection.
Failure leaves app input/layout available, with no layout-root hide/show calls.
The preview's layer is set one above the phone layout, matching native factories.

Actual child activation (03769260) clears the parent's +0x164 update flag. The
previous fixture omitted that behavior and incorrectly allowed the phone to
drive preview transitions indefinitely. Transitions now run from the preview's
own update override. After B's return animation and idle reset, native child
removal (03768C7C) returns lifecycle to 2. Restore the parent update flag first,
then detach from the parent's own update after child-callback iteration returns;
removal inside the child callback would invalidate that iterator. Input is
restored after detachment. The prepared child can be reused without further
UI allocations.

The expanded regression executes concrete ExpHeap creation from a buffer
(037C6FFC), allocation (037C77A0), and adjustment: preview-sized allocation
succeeds before shrink and returns null afterward. Base heap bookkeeping and
the parent's backing-block resize are modeled; the free lists/capacity are real
native code. Integration tests forbid allocations after modeled UI setup and
exercise actual child activation/removal with only callback registration and
engine entry/exit services modeled. Parent updates are forbidden while paused;
opening, navigation, B return, restored app updates, reuse and cleanup still pass.
The existing native constructor, data, dispatch and fail-closed checks remain.

The pinned devkitA64 Docker build passes with `-Wall -Werror`.
`python3 tools/check.py core` passes all host checks, all 64 module combinations
at two load bases, configuration/compatibility tests and the expanded native
preview tests. Suite/source/custom ZIP integrity, suite manifest hashes and
binary/source parity checks pass. Older releases are preserved.

Live Eden verification of 0.3.27 remains pending for full resource loading,
rendering/layering, rotation, controller input, app/phone reopening and ordinary
workbench crafting/cooking. The native tests do not establish these live results.
No installed mod, running game, save or synced log was modified.


## 0.3.28 DIY preview initial-state stall and app reopening

The synced Core 0.3.27 log reaches successful construction, layout binding,
child creation and `activating prepared page`; it contains no crash/backtrace.
The user reports that buttons stop responding while background music continues.
The last progress message alone does not establish a hang inside activation.

A native execution reproduction establishes the opening-state defect:
034942F0 constructs the preview's state table and leaves it in `cInit` (0),
with both request slots empty. Its 03494768 update sets a resident capability
flag on the first frame and never transitions to `cIdle` (1). The 0.3.27 compiled
integration waits for state 1 before requesting appearance, so the reproduction
leaves both request slots empty indefinitely with app input suspended. Earlier
activation fixtures incorrectly forced state 1 and masked this condition.

0.3.28 accepts settled initial state 0 and idle state 1, matching the native
appear-readiness contract, and calls the native +0xF0 readiness query before
requesting appearance. A regression holds layout readiness false and verifies
that no dataset is borrowed or appear request queued until readiness returns. Reused pages still wait for idle entry to finish
before borrowing the app's recipe list. A post-activation log records recipe
state and page lifecycle, distinguishing an activation failure from a later
transition failure. No crafting or navigation behavior was changed.

02E9D538 is the app's deactivation callback, not page destruction; native code
only removes its layout from the rendering registry. Discarding the session
there made subsequent app opens lose their prepared preview. The session now
retains its owned child and queues idle to cancel unfinished transitions.
A newly initialized phone page replaces any old resident session. Callback
binding does not depend on a previous session's address. Tests cover reopening
without additional allocations and replacement of a resident's page.

The expanded regression executes native state-table initialization and initial
updates, actual activation begin/end wrappers, recursive update eligibility,
callback registration and unregistration, and child removal. It verifies state
0 remains unchanged by activation, the compiled integration queues appearance,
and callback/active lists recover their sentinels after removal. These run at
both ASLR bases. The resource/rendering service boundary, native manager frame
scheduler and asynchronous transitions remain modeled. In particular, the
0.3.27 assumption of a permanently paused parent was too strong: 03768DB0
recomputes flags and advances lifecycle 3 to 4 on subsequent manager updates.
The existing tests that artificially keep lifecycle 3 cover immediate removal;
they do not establish live manager scheduling or deferred lifecycle-4 removal.

The pinned Docker build and `python3 tools/check.py core` pass. Suite, source and
custom-example archive integrity, suite manifest hashes and deployed/source
parity are checked before release. Older archives are preserved.

Live verification is still required for appearance, model rotation, controller
navigation/B return, repeated app/phone reopening and ordinary workbench
crafting/cooking. This release fixes a reproduced opening stall; local tests
cannot establish that every live preview issue is resolved. No running game,
installed mod, save or synced log is modified.

## 0.3.29 DIY preview appearance-to-selection stall

The synced 0.3.28 log confirms successful activation (`state=0 lifecycle=3`)
and `appear requested`, then ends without a selection message or native crash.
The initial-state fix gets past the first stall, but the next wait is wrong:
the integration expects settled state 3 after appearance. Native 03494B08
instead requests state 5 (`cDisplayList`). Its update, 0349553C, does nothing;
the owner must supply the selection and request state 4 (`cChangePlay`). That
state loads the recipe and requests state 3 (`cPlay`) after its animation.

The new regression executes native state initialization, page-update virtual
02B0C720, deferred-request visitor 02B0E370, and state entry/update/exit handlers.
Against the 0.3.28 binary it records states 2, 5, 5, 5, 5 and fails. Version
0.3.29 selects from settled state 5; the expected opening sequence is now
2, 5, 4, 3, 3. The existing modeled flow fixtures also use state 5 instead of
artificially setting state 3 immediately after appearance.

The native transition regression checks the selected index, model reload
request, empty deferred slots and enabled preview input. It also exercises B's
play exit/display-list entry, native idle cleanup, return of app input and
reopening of the same page. UI type identification, icon/detail rendering,
sound and animation services are modeled. Manager scheduling and actual
rendering remain outside this test. Added fingerprints cover the native appear
and selection updates; conflicts disable only the preview feature.

The pinned Docker build and `python3 tools/check.py core` pass, including host
policy checks, all 64 module subsets at two ASLR bases, Core configuration and
preview regressions. Release archive integrity, manifest hashes, deployed Core
parity and source parity are checked before delivery. Older archives remain.

Live Eden verification remains required for visible appearance, item rotation,
controller navigation/B return, app/phone reopening and workbench crafting.
No running game, installed mod, save or synced log is modified.


## 0.3.30 DIY preview background, model publication, craft button and B

The synced 0.3.29 log reaches `showing`. The user's screenshot shows populated
materials, the phone grid inside the captured model area, and “Need materials!”;
B then stops input. Opening-state regressions alone did not cover these paths.

Native WorkCraftInfoSeq owns the info UI, recipe icons and quantity/dialog
children; its +0x1c1e8 child is a quantity selector, not a 3D renderer. The
workbench owner separately publishes the selected recipe to the shared
ModelOnUI context (0347A600), and owns WorkCraftBgSeq. Core now prepares the
native background before heap shrink and uses the native model publication
routine on initial selection and navigation. It saves/restores the model data
when returning, hides/restores the phone layout, and retains native BG layer 14
and info/capture layer 23. Installed WorkBenchBG/WorkBenchInfo assets are loaded
normally; Core ships no replacement of either, including no fixed aspect ratio.

Craft-button +0x1b0 was incorrectly treated as a layout root. Native 03471FF0
shows its layout roots at +0x2e0; visibility is now changed on that root's
controller. The test poisons the previous wrong field. Craft rejection and
removal of the focus target remain independent safeguards.

Native cDisplayList reverses content channel 3 without hiding the entire layout.
Core now uses the same channel-busy query as 0349530C before idle and detach.
The regression keeps the root visible, holds a non-null animation busy, then
releases it and verifies input restoration and reuse. Native model publication,
BG state initialization/updates, recipe navigation and context restoration run
in the ARM harness at two ASLR bases. Resource loading/rendering and animation
services remain modeled; the tests do not prove visible 3D rendering, rotation,
16:10 appearance or controller behavior in Eden.

Validation: pinned Docker build, `python3 tools/check.py core`, archive integrity,
manifest hashes, deploy/source parity. Live Eden retest remains required for
model appearance/rotation, absence of the craft button, 16:10 layout, B back to
the same recipe grid, app reopening and normal workbench crafting/cooking.
No running game, installed mod, save or synced log is modified.
