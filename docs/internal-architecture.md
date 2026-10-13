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
| [Faster Dialogue](../source/program/dialogue/features/faster_dialogue.hpp) | Explicit message classification, automatic UI/unnamed/NPC boost and native-request base/boost choice. | `dialogue/game/dialogue_native.cpp` supplies the pre-policy request and lazy verified identity; `services/reveal_policy.hpp` copies startup policy/rates and expresses single-tier hurry precedence. Native gates, input compatibility, name capture and rate forwarding remain underneath. [Current handoff](native-first-dialogue.md). |
| [Controls](../source/program/controls/features/controls.hpp) | Pickup category lock, lazy hurry stop conditions and camera choice; `features/traversal.hpp` exposes hop/tool preference and borrow/restore/cancel; `features/swimming.hpp` owns stroke timing/propulsion. | `controls/game/*_native.cpp` supplies callback-scoped operations, native hook/ABI/layout checks, correlated state, probes and original requests. The production hooks call the policies directly, without a new dynamic registry. Shared inventory/reveal consumers retain their boundaries. [Current handoff](native-first-controls.md). |
| [Crafting settings](../source/program/crafting/settings.hpp) | Typed member/schema declarations and explicit numeric policies; use the shared overlay parser. | `core/settings_schema.hpp` validates encoding/size, parses entries and preserves omission/invalid inheritance. Duplicates restore the value before that file, including invalid-first duplicates. Module metadata validation and regular/custom ordering remain in Core. [Overlay contract](settings-schemas.md). |
| [Feature activation](../source/program/core/modules.cpp) | Actual enabled conditions, installers and dependency names; consume the aggregate result. | `core/feature_activation.*` records independent availability and partial publication; `hook_plan.*` and the bounded registry validate/reserve/publish owned sites. `Failed` or `Unsupported` can still have `published=true`; preparation is not transactional rollback. Resources follow installed markers independently of gameplay enablement. [Activation contract](feature-activation.md). |
| [Shared input](../source/program/core/page_input.hpp) | Typed synchronous handler/observer and explicit priority; cancellation must drain pending gestures. | `page_input.cpp` owns five handler/five observer slots, stable equal-priority ordering, before/native/after dispatch and suppression of skipped handlers. Pockets → phone preview → workbench → quantity wrap → quantity shortcut. Consumption skips native/after calls and sends blocked cancellation to lower typed consumers. [Input contract](shared-input-ui.md#input). |
| [Captions](../source/program/core/hint_text.hpp) and [resource minimums](../source/program/core/resource_budget.hpp) | An owned CaptionKey and immutable process-lifetime UTF-16 provider; exact normalized resource path and positive byte floor. | Services own three exclusive caption keys, bounded text/fallback, native prepare/set/notify ordering, and four copied paths of at most 95 bytes. Minimums only increase, retain larger native sizes/flags and remain active for installed replacement assets with disabled gameplay. Registration failures are explicit. [UI/resource contracts](shared-input-ui.md). |
| [Crafting policy](../source/program/crafting/features/crafting_features.cpp) and [preview lifecycle](../source/program/crafting/features/preview_features.cpp) | Speed curve, quantity endpoints/timing, Y quantity/X Favorite; preview selection, input, acquisition/create/show/close, navigation retries and rotation. Non-template implementation lives in `.cpp` files. | Callback-scoped quantity/preview interfaces expose supported operations; native adapters own ABI/layout/guards, actual eligibility/actions, renderer acquisition/restoration, models, poses, layout and publication. The old header-only Session and mixed quantity/speed implementations were removed. [Current Crafting handoff](native-first-crafting.md), [preview contract](preview-sessions.md). |
| [Catalog policy](../source/program/catalog/features/catalog_features.cpp) | Surface/reward eligibility, recipe versus catalog/donation history, art setting, marker choice, row badge visibility and variation-label text. | `catalog/game/catalog_ownership.*` and `recipe_knowledge.*` validate native metadata/save views and reuse history getters/read locks. `game/*_native.cpp` own hook phases, borrowed UI objects, name formatting/copying, label scopes and row-local pane resolution/reset. [Current handoff](native-first-catalog.md). |
| [Fitting Room policy](../source/program/fitting/features/fitting_features.cpp) | Exact-item bag membership, conservative capacity, checkout composition/revalidation, bag-only confirmation, repeat/cleanup, explicit badge animation, B focus-then-confirm and footer limits. | Callback-local fitting operations expose current native observations/actions; `fitting/game/*_native.cpp` own controller/input correlation, native list/price ABI, original payment/delivery/ticket/body, records, typed panes/counter/cursor, temporary input forwarding and copied captions. [Current handoff](native-first-fitting.md). |

| [Shopping Card policy](../source/program/shopping/features/shopping_features.cpp) | Reviewed purchase contexts, combined affordability, savings-first payment/native cash change order, sale proceeds/message, conservative purchase space and capped balance formatting. | `shopping/game/card_payment.hpp` exposes callback-local native money paths; `shopping_card_native.cpp` owns exact caller/room/EventFlow scope, encrypted access, original debit/deposit branches, resource preparation and native HUD/auxiliary lifecycle/feedback. Void money actions do not promise atomic completion. [Current handoff](native-first-shopping.md). |

| [Core assembly](../source/program/core/features/core_features.cpp) | Seven optional module declarations, package minimums, explicit resource dependency IDs, Catalog feature dependencies and nonzero amiibo timeout enablement. | `modules.cpp` owns post-mount once-only loading/overlays/compatibility/status; `native/core_native.cpp` validates Catalog code and changes only the native amiibo initialization register. Countdown and expiry remain native. Shared activation/hook/input/resource services retain their boundaries. [Current handoff](native-first-core.md). |

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
| Traversal native mechanics | Feature preference and borrow/restore/cancel decisions are in `controls/features/traversal.hpp`; `game/traversal_native.cpp` retains native probe scopes, one-use movement proofs, resident/handle correlation, action decoding and request layouts. These are verified native mechanics, not stronger handle/lifetime guarantees. See [the Controls handoff](native-first-controls.md). |
| Quantity native scaffolding | Gesture/priority/timing decisions are in `crafting/features/crafting_features.cpp`; `game/quantity_*_native.cpp` retains verified page/sequence identity, native cyclic row binding, footer descriptors and FSM request fields. The binder normalization is supported adapter scaffolding, not a universal native-list capability. |
| Shopping display mechanics | Payment/routing/balance policy is in `shopping/features/shopping_features.cpp`; `game/shopping_card_native.cpp` retains pane/controller/number-transition handling, catalog/storage presentation correlation and native feedback. These remain engine-specific adapter mechanics, not a general renderer or transaction service. |

Stage 7 left these areas unchanged; the subsequent Pockets, Faster Dialogue, Controls, Crafting, Catalog, Fitting Room, Shopping Card and Core passes separated their actual policies. Workbench
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
