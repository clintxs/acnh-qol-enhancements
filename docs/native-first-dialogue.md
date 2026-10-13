# Faster Dialogue: readable policy over native reveal state

This module continues the accepted Pockets refactor on
`refactor/semantic-feature-api`, starting from clean commit
`c9466f60a0f2843758e7956bc0e9d4f2a08e5173`. Earlier branches and the accepted
Pockets outputs remain comparison points. This work stops at Faster Dialogue;
Controls is exercised as a shared consumer, without migrating its implementation.

## Read in this order

1. [ChooseSpeed and the feature policies](../source/program/dialogue/features/faster_dialogue.hpp):
   `ChooseSpeed` classifies the message, then `SelectSpeed` chooses base or boost
   from the native request and `ShouldAutomaticallyBoost`. The same file shows
   the UI/unnamed/NPC conditions, configured identity matching and exact reviewed
   fallback banks. These functions run in production and also compile without
   Exlaunch for policy tests.
2. [Module wiring and settings](../source/program/dialogue/faster_dialogue.cpp):
   the existing parser/reporting owns process-lifetime settings;
   `SelectConfiguredDialogueSpeed` connects `ChooseSpeed` to a copied startup
   callback/rate declaration. No settings/default/version syntax changed.
3. [Reveal policy contract](../source/program/dialogue/services/reveal_policy.hpp):
   registration copies the declaration, rejects invalid/conflicting callbacks
   or rates, and accepts only its two configured speed choices. A bad callback
   result uses the configured base tier. `ChooseRevealSpeed` makes shared hurry
   precedence explicit: one tier wins, with no multiplication of overrides.
4. [Dialogue context](../source/program/dialogue/game/dialogue_context.hpp):
   native request, presentation, name visibility, active bank and lazily resolved
   identity are synchronous borrows. Missing identity remains unknown. This
   interface supplies no page-advance operation.
5. [Native adapter](../source/program/dialogue/game/dialogue_native.cpp):
   the original seven hook sites, SIMD context, fingerprints, native fields,
   name cache, handle/type validation, tier encoding and rate forwarding remain
   underneath the feature code. The old native classification decision tree
   and settings-header speed policy have been removed, rather than retained as
   parallel implementations.

## A real before/after

Before, `DialogueSelectSpeed` read the held field, invoked a classifier mixing
boost categories with native registry/layout checks, selected a speed, and
encoded native flags. `SelectSpeed` itself lived alongside configuration parsing.

Now the production feature decision reads:

```cpp
const auto message = ClassifyMessage(dialogue.message);
return {SelectSpeed(settings, message.kind, message.resourceId,
                    dialogue.nativeBoostRequested, message.localizedName), message};
```

`SelectSpeed` visibly chooses `boostSpeed` when the native request or automatic
policy is true, otherwise `baseSpeed`. `ShouldAutomaticallyBoost` visibly checks
UI/unnamed switches and the configured NPC IDs/localized names. Native input
bits and memory fields are absent from these functions.

The native adapter provides presentation and verified name visibility first.
`ClassifyMessage` requests `message.Speaker()` only when talk identity is needed.
UI, confirmed unnamed and reviewed-bank paths retain their original early exit,
without adding entity/RTTI calls merely to create a complete-looking context.

## Native request, permission, tier and actual rate

These are separate observations, not interchangeable Boolean flags:

| Value | Verified phase and meaning |
| --- | --- |
| Delivered boost request | Native `01AFDEBC` loads text+`21B4` into `w9`. The inline hook at `01AFDEC0` samples that register before policy changes it. The existing Print-input compatibility hook has normalized the request to current B/R input by this point; this is not an untouched vanilla latch. |
| Native permission | Native mode/forced/acceleration-disabled branches precede `01AFDEC0`. Rejected paths do not invoke the feature policy. Later scripted-rate and renderer gates remain authoritative too. |
| Selected effective tier | The adapter writes `w9` as 1/3 for configured base/boost. Controls may replace it with 5 for its hurry tier. Native `01AFDEC8` stores this into text+`21B7`; the forwarding hook preserves the whole selected byte for the main/render paths. |
| Actual reveal rate | Eligible native fast-rate loads are scaled by the winning nominal speed divided by 2. Native locale/FPS rate, renderer availability, completion and nonnegative scripted overrides determine the actual result. The nominal choice does not promise achieved speed. |

The request is never read from text+`21B7` or a renderer's effective flag. An
automatically boosted UI message therefore has a false manual request even
though its effective tier is boosted. The compiled phase test changes the held
field *after* the native request load and proves the policy consumes the already
loaded value. Under normal native execution, comparisons with the prior ELF
retain the same result.

The input compatibility hook remains necessary for shipped behavior: original
`01AF9FEC..01AFA008` tests B/L/R and only latches true; it does not clear on release.
`DialogueHeldInput` still writes current B/R and excludes L when Faster Dialogue
is enabled. Without Faster Dialogue, shared hooks leave ordinary native B/L/R
input/speed alone. Removing this hook would not preserve the current controls.

## Description-to-code map

| Behavior | Visible feature decision | Native/service responsibility |
| --- | --- | --- |
| Configured base/boost speeds | `ChooseSpeed`, `SelectSpeed` | Native permission, 1/3 tier encoding and main/renderer forwarding/scaling. |
| Hold B/R for boost, release to base | `dialogue.nativeBoostRequested` in `SelectSpeed` | Existing canonical input compatibility hook; exact pre-policy native request load. L remains excluded only where Faster Dialogue already excluded it. |
| Automatic blue UI dialogs | `ClassifyMessage` selects UI; `ShouldAutomaticallyBoost` reads `boostUiMessages` | Adapter reads verified native presentation metadata. |
| Automatic unnamed narration | `ClassifyMessage` checks confirmed unnamed visibility, then the reviewed legacy bank fallback and validated player identity; `ShouldAutomaticallyBoost` reads `boostUnnamedMessages` | Adapter correlates name preparation with manager/context/source/handle and current message IDs. Failed name conversion of a visible name remains Named. |
| Configured NPCs | `ShouldAutomaticallyBoost` compares native resource ID or a nonempty localized name with the configured entries | Native registry, generation, retiring-entity and NPC RTTI checks; bounded owned name capture; no position/voice delegate used for identity. |
| NPC defaults/list customization | Existing `Settings`, `NpcAlias`, `AddNpc`, layered parser and normalization | Same 64-entry/95-byte bounds, ASCII case matching, UTF-8 handling, Kapp'n aliases and regular/custom semantics. These configure one policy, not separate subsystems. |
| Shared Y hurry precedence | `ChooseRevealSpeed` selects the supplied hurry tier over the dialogue tier | Existing Controls `SelectDialogueHurry` runs after Faster Dialogue selection. Native rate paths use that single winning tier. |
| Page advancement/choices | Faster Dialogue supplies no advancement request | Existing Controls handler exclusively owns held-Y advancement, choice/new-catch stops and cancellation. Native A/B behavior remains outside this refactor. |

## Preserved classification, ownership and lifecycle

Classification order is explicit and preserved: unsupported presentation is
unknown; confirmed unnamed outside a dialog is unnamed; unknown name state may
use the five reviewed bank fallbacks; a dialog is UI; other non-talk windows
remain unknown; talk windows resolve validated player/NPC identity. A confirmed
name suppresses a player/bank guess. The historical unknown-name bank fallback
still precedes UI classification, so its existing setting priority remains.
Unknown state alone is never converted into unnamed.

Message/identity strings borrow the adapter's callback-local copies. The feature
returns a synchronous decision; no message view, entity pointer, callback context
or returned string view is queued across frames. The existing name cache owns
converted bytes and clears on every preparation. Its original correlation and
copy checks are retained, not strengthened into a general entity-lifetime
promise. Native name conversion, registry access and diagnostics remain adapter
code; native class/type validation is not automatic-boost policy.

A success path is native input -> canonical B/R request -> native eligibility ->
pre-policy request sample -> message classification/optional identity lookup ->
base/boost choice -> optional Controls hurry override -> original effective-flag
store/forward -> native rate gates and progress. A protected path skips the
policy; an unknown/stale identity falls back to base unless manual boosting is
requested; a scripted nonnegative rate bypasses our scaling. No extra parser
iteration, voice-position callback or page-advance request is introduced.

Settings are loaded and normalized before startup registration. The callback
uses process-lifetime configured settings; native rates are copied into the
startup declaration. There is no runtime rebinding, polling, SDK or independent
plugin/library contract. Existing hook validation remains authoritative on
repeated installation; no new identical-reinstall guarantee is claimed.

## Verification and retained outputs

Baseline ELF/NSO/NPDM, the unchanged module configuration and hashes of existing
saved outputs are under ignored `dist/native-first-dialogue/baseline/`. Separate
diagnostic outputs are under `diagnostic/`; the final candidate uses the normal
build. Existing releases, Pockets artifacts and installed Eden files are preserved.

New host checks cover conservative classification/priority, absent identity,
query laziness, configured ID/localized-name matching, request-versus-automatic
boost, equal tiers and hurry precedence. New compiled checks execute production
registration and the real SIMD/native reveal path, including copied declarations,
conflict rejection, exact request phase, unavailable/blocked native paths,
scripted override and invalid callback output. No original observable assertion
was removed; the settings test now includes the feature policy's new location.

A separate baseline script compares current and saved ELF behavior without
replacing the working executable. It checks native reveal bytes, parser calls,
identity-query frequency, B/R/L input, dynamic rates, message categories, stale
identity, protected/scripted branches and Controls-only/combined hurry at two
load addresses. All 1,136 targeted comparisons pass. The diagnostic mode also
passes the new policy checks and all 6,916 inherited name cases across 14 locales.

Live Eden remains required for actual input, audio, reveal timing, name-tag
capture and visual behavior. Compare hands-off/B/R/release/L on long named NPC,
UI and unnamed messages; configured IDs and localized names; unknown/failed name
states; scripted waits/rates; Controls alone and combined; Y hold/release,
choices and first-catch stops. Baseline and normal candidate executables remain
separately identifiable. Offline emulation is not live qualification.

## Final results — 2026-10-13

Executable source commit: `c7cfb928ade542107e5c0c790352431147ed6948`.
Documentation is committed separately. No executable-source change followed
the tested normal build.

| Check | Result |
| --- | --- |
| Pinned normal and diagnostic Docker builds | Both passed, exit 0; no compiler warnings/errors. `build.log`, `diagnostic-build.log`. Final candidate is normal mode, all diagnostic flags 0. |
| New compiled policy/phase checks | Passed at two bases in normal and diagnostic modes. `policy-arm.log`, `diagnostic-policy.log`; also included in the final full run. |
| Diagnostic native-name checks | Passed: 6,916 cases across all 14 supplied locales. `diagnostic-names.log`. |
| Full normal regression run | Passed, exit 0: all 21 host suites and 49 ARM/resource scripts. `full-check.log`. Includes all 128 module combinations, shared reveal installation orders, Controls-only/combined hurry, names and the accepted Pockets behaviors. |
| Inherited dialogue coverage | Passed: 83,236 extended-speed checks per base, all 1..30 speed pairs and both rate paths; 6,672 additional ARM cases and 6,916 name cases. |
| Current/baseline comparison | Passed: 1,136 Dialogue comparisons for native bytes/calls/query counts/input/rates/gates/hurry; original 978 Pockets scenarios and migrated hook sites/readiness also match the saved starting ELF. |
| Preservation | Passed: all 3,148 pre-existing saved distribution/comparison files unchanged; `main` and the original infrastructure-refactor branch unchanged. `preservation.log`. |
| Saved normal candidate | `dist/native-first-dialogue/candidate/` contains matching executable/configuration copies, source identity and SHA-256 manifest. |
| Live Eden | Not run. Actual input/audio/rendering and name-tag qualification remain pending. |

The module is ready for review at the Faster Dialogue boundary. No Controls,
Pockets or other module implementation was migrated during this pass.
