# Unified QoL validation — 0.3.1 Shopping Card prototype

Validated locally on October 8, 2026 against the supplied original ACNH 3.0.3
main (build FF1D1C05670DB6021C85B624A710B963). The user confirmed the player
dialogue fix and subsequently the corrected 0.2.3 NPC diagnostic build work in Eden. The configuration layering and new Shopping Card prototype
still need a live check. No running game or save files were accessed by the
local tests below.

## Build and loader

- Pinned devkitPro Docker build, networking disabled: passed with `-Wall -Werror`.
- Core deploys only `subsdk6` plus the existing minimally adjusted `main.npdm`.
  No package contains subsdk9; avoiding the slot does not prove future
  Jubbsticks Additions hook/NPDM compatibility.
- All 64 combinations of six feature packages at two ASLR bases passed.
  `exl_main` installs only the content-mount continuation hook and performs no
  configuration reads. Activation executes the real compiled registry/parser and
  installers; a second activation performs no additional reads or installations.
- Full suite installs 55 disjoint modeled hook ranges (Core 1, Catalog 16,
  Pockets 16, Faster Dialogue 5, Fitting Room 11, Amiibo 1, Shopping Card 5), plus the private
  fitting input vtable entry. No embedded binary replacement arrays, IPS or
  dialogue code-cave helper remain in the collection.
- Per-feature absent, disabled, invalid, missing-version, oversized, below-minimum
  and newer-version configurations were checked. Newer valid packages warn and
  run. Below-minimum packages leave feature code untouched. Open/read/size errors
  close handles where appropriate and disable requested features. Occupied
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
  original five modules; Shopping Card requires minimum 0.3.0.
- Compiled Amiibo C++ callback feeds 9,000 into the unchanged native halfword
  timeout store at both load addresses. The original immediate is untouched.

The loader harness models FS services, code-hook allocation, writable aliases
and cache services. It does not execute Eden's loader or establish that the
content-mount continuation is safe in a live process. Exlaunch's existing inline
adapter is selected with full SIMD-register preservation for discovery. The
SIMD context declaration was corrected to match the assembly stack layout, with
compile-time offset assertions. Ordinary InlineCtx layout is unchanged.

## Shopping Card

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
  savings. The cShop capacity query selects existing wire-to-account text;
  other capacity modes retain the native result. Residents without a placed
  tent retain ordinary sale text, avoiding the wire-flow housing rejection.
  Native cap clamping remains.
- Purchase-space callback passes zero Bell payment and unchanged ticket count
  to the original space helper, preventing false slots from Bell bags that
  savings payment does not consume. Shared Fitting Room affordability uses
  savings only while Shopping Card is installed and in a supported shop.
- Every Shopping Card instruction fingerprint is individually corrupted; each
  conflict disables the module before any of its hooks installs.
- Existing native fitting checkout and feedback regressions are rerun with the
  shared account accessor. Actual shopping gameplay, native field state and
  both modules together require the live checks below.

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
  checked against independently captured SHA-256 values.
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

7. Shopping Card: use the existing save backup. Test a floor item and cabinet
   item in both Nook's Cranny versions, clothing from Mabel and the fitting room,
   then the fitting bag with both modules enabled. Compare savings at the ABD
   before/after; confirm pocket Bells and Bell bags do not change. Test cash
   rich/savings poor and cash empty/savings sufficient, cancellation, tailor
   tickets, full storage/pockets, wearing/sending, and several residents. Sell
   items/turnips at the counter and verify the deposit and wire message. Check
   drop-off box, ABD transfers, Harv and another vendor retain normal behavior.
   Restart with Shopping Card disabled/removed and verify native payment.
