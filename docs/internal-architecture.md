# Internal architecture and contribution guide

These interfaces serve QoL Enhancements on the supported ACNH build. They are
internal implementation details, with no external API, SDK, scripting or binary
compatibility promise. The dependency direction is:

```text
Feature declarations and policies
              ↓
Internal domain services
              ↓
Native adapters and version profiles
              ↓
Exlaunch and game functions
```

Feature files declare behavior, mappings, settings and dependencies. Services own
shared correctness rules and deferred lifecycles. Adapters interpret engine
objects and perform supported operations; version profiles own addresses,
fingerprints and verified layout assumptions. Pure policies compile without
Exlaunch or the game. Module/bootstrap code assembles optional features; shared
services do not include individual feature implementations.

## Current examples and their owners

| Example | Feature author supplies | Implementation owns |
| --- | --- | --- |
| [Pockets policy](../source/program/pockets/features/pockets.hpp) | Explicit action priority, sole-action checks, Grab 5/10 insertion, hold-A intent and fixed hints. `quick_store.cpp` supplies bindings and feedback timing. | `pockets/services/pocket_commands*` copies up to three rules and the chosen policies, rejects conflicts, queues/revalidates requests and applies declared feedback timing. `game/pocket_commands_native.*`, `pocket_input.*` and profiles own eligibility, row interpretation, consumed bits and native phase operations. Full eight-byte item identity, resident/slot/row and generation checks prevent stale work. [Current Pockets handoff](native-first-pockets.md), [earlier lifecycle contract](pocket-commands.md). |
| [Crafting settings](../source/program/crafting/settings.hpp) | Typed member/schema declarations and explicit numeric policies; use the shared overlay parser. | `core/settings_schema.hpp` validates encoding/size, parses entries and preserves omission/invalid inheritance. Duplicates restore the value before that file, including invalid-first duplicates. Module metadata validation and regular/custom ordering remain in Core. [Overlay contract](settings-schemas.md). |
| [Feature activation](../source/program/core/modules.cpp) | Actual enabled conditions, installers and dependency names; consume the aggregate result. | `core/feature_activation.*` records independent availability and partial publication; `hook_plan.*` and the bounded registry validate/reserve/publish owned sites. `Failed` or `Unsupported` can still have `published=true`; preparation is not transactional rollback. Resources follow installed markers independently of gameplay enablement. [Activation contract](feature-activation.md). |
| [Shared input](../source/program/core/page_input.hpp) | Typed synchronous handler/observer and explicit priority; cancellation must drain pending gestures. | `page_input.cpp` owns five handler/five observer slots, stable equal-priority ordering, before/native/after dispatch and suppression of skipped handlers. Pockets → phone preview → workbench → quantity wrap → quantity shortcut. Consumption skips native/after calls and sends blocked cancellation to lower typed consumers. [Input contract](shared-input-ui.md#input). |
| [Captions](../source/program/core/hint_text.hpp) and [resource minimums](../source/program/core/resource_budget.hpp) | An owned CaptionKey and immutable process-lifetime UTF-16 provider; exact normalized resource path and positive byte floor. | Services own three exclusive caption keys, bounded text/fallback, native prepare/set/notify ordering, and four copied paths of at most 95 bytes. Minimums only increase, retain larger native sizes/flags and remain active for installed replacement assets with disabled gameplay. Registration failures are explicit. [UI/resource contracts](shared-input-ui.md). |
| [Phone preview policy](../source/program/crafting/services/preview_session.hpp) | Open/close, navigation/rotation samples and explicit cancellation/reset intent. | Session owns phase, timeout, one retryable navigation step, repeat timing, rotation draining and bounded accumulated tilt. `game/preview_phone.cpp` owns model leases, interpretation and restoration; layout/entity/render adapters own resource copies, generation checks, native matrices and publication. [Preview contract](preview-sessions.md). |

A short feature file alone does not establish a boundary. For example, quick-store
is safe because deferred identity checks and native command completion live in
its service/adapter, and preview is safe only within the documented native lease
and callback assumptions. Do not bypass those rules with a shorter native call.

## Contracts to preserve

- Registration happens synchronously at content-mount startup. Registries have
  fixed capacities and no runtime rebind/unregister. Identical declarations
  revalidate owned code; conflicts/capacity failures preserve active ownership.
  Activation reserves one of 64 result slots before invoking an installer;
  executable ownership has 160 claims and plans at most 20 sites.
- Rule tables and resource paths are copied. Caption providers must return
  immutable process-lifetime storage. Inventory values copy exact item bytes
  and resident identifiers; bounded slot views check capacity and the 40-slot
  ceiling. These values do not establish a native object's lifetime.
- Input page/snapshot contexts and native row views are callback-scoped borrows.
  Do not retain them. Pocket requests carry correlation/identity evidence and
  must revalidate before starting deferred work; after native start, completion
  follows the supported command lifecycle rather than repeating the transfer.
- Fail closed for unsupported profiles, stale identities and conflicting code.
  Independent features can remain active. Some hooks/registrations may already
  be published on failure; report that evidence. Caption adapter failure for a
  registered private key retains deliberate blank/suppressed cleanup, since
  no native translation exists for that key. Bounded resource/layout failure
  follows its documented native fallback, not a guessed allocation or call.
- Input/session policy and mutable UI state run on the supported UI path.
  Render workers read coherent atomic publications and native callback objects,
  never mutable sessions or registry traversal. Readers make one attempt and
  fall back if publication is busy. Native component lifetime and scene/render
  scheduling still depend on verified engine callbacks; atomics cannot prove
  them. Draw-scope metadata is valid only during synchronous nested drawing.
  See [native bindings](native-bindings.md) and [thread contracts](preview-sessions.md#thread-ownership-and-publication).

## Migration status and selective future work

Migrated boundaries include settings schemas, feature activation/hook ownership,
quick-store command declarations and execution, shared input/captions/resource
minimums, and phone session policy versus model/render mechanics. This does not
make the entire mod declarative or engine-independent.

| Remaining mixed area | Current location and selective next boundary |
| --- | --- |
| Storage withdrawal correlation | The sole-action policy is now in `pockets/features/pockets.hpp`; native descriptors/fields/hooks are in `game/storage_withdrawal_native.cpp`. The existing intent-only correlation and narrower callback checks remain explicit limits; see [the Pockets contract](native-first-pockets.md). |
| Traversal/tool restoration | `controls/auto_vault.cpp`, ladder and tool-swap features already use traversal profiles/adapters and bounded inventory, but retain native hooks and transition mechanics beside policy. A later change could separate borrowing/restoration lifecycles without pretending raw pointers are stable handles. |
| Quantity shortcuts/wrap | `crafting/quantity_shortcut.cpp` and `quantity_wrap.cpp` use shared input but still combine native footer/state/layout access with gestures and timing. A future policy extraction should preserve native limits, cooking/workbench distinctions, repeat timing and consumed input. |
| Other older features | Catalog, dialogue, fitting and shopping implementations have useful policies/adapters but also direct native mechanics. Migrate selectively when it clarifies actual work; do not describe their whole feature directories as pure services. |

Stage 7 left these areas unchanged; the subsequent Pockets pass separated its actual policies. Workbench
binding/tilt state is still adapter-owned; the encapsulated Session is the phone
lifecycle, not a new universal preview framework.

## Contributor checklist

- Prefer an existing semantic operation over direct native field access.
- Keep native offsets and ABI details in adapters/profiles.
- Add an abstraction when it owns a real shared rule or materially clarifies a feature.
- Do not generalize for hypothetical consumers.
- Preserve native eligibility and exact item identity.
- Make queued work, cancellation and completion explicit.
- Keep shared services independent of individual feature implementations.
- Preserve bounded storage and supported execution contexts.
- Explain behavior changes separately from mechanical refactoring.

Run focused policies and compiled production-path regressions first, then the
full applicable suite on fresh outputs. Preserve main, baseline manifests and
older archives. Offline success does not qualify rendering, focus, sound or
cleanup in Eden: use the [candidate live-validation handoff](stage7-live-validation.md).
