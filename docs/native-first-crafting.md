# Crafting: readable speed, quantity and preview behavior

This module continues `refactor/semantic-feature-api` from the accepted Controls
commit `28baa3d47fd8d5b984c9153f44be9b12ddaf7e8f`. It stops at Crafting.
Implementation commit: `163aafb24946e2142828cce43337477e42e9d294`.

## Read in this order

1. [Crafting feature implementation](../source/program/crafting/features/crafting_features.cpp):
   `CraftingRate` shows the preserved speed curve; `HandleQuantityEndpoints`
   shows endpoint eligibility, neutral rearming, native movement and pause
   timing; `HandleQuantityShortcut` shows Y quantity versus X Favorite priority,
   consumption and native eligibility/transition checks.
2. [Preview feature implementation](../source/program/crafting/features/preview_features.cpp):
   read `QueueRecipePreview`, `HandleRecipePreviewInput`, then `Session::Frame`,
   `NavigateRecipePreview`, `RotateRecipePreview` and `DeactivateRecipePreview`.
   The actual acquire/create/show/close, retry, input and rotation decisions are
   here, not inside installer wrappers.
3. [Declarations](../source/program/crafting/features/crafting.hpp) and
   [preview session types](../source/program/crafting/features/recipe_preview.hpp):
   headers declare types, state and functions. Substantial non-template policy
   lives in the two `.cpp` files. The old implementation-heavy
   `services/preview_session.hpp` was removed, rather than retained as a duplicate.
4. [Quantity operations](../source/program/crafting/game/quantity_operations.hpp)
   and [preview operations](../source/program/crafting/game/recipe_preview_operations.hpp):
   small callback-scoped borrowed interfaces expose native list movement,
   actual eligibility, queued actions, recipe selection and renderer operations.
   Contexts are stack objects in adapters; policy never retains or deletes them.
   No dynamic callback registry, external SDK or universal game facade was added.
5. [Settings and assembly](../source/program/crafting/crafting_module.cpp):
   configuration and activation remain here. Existing defaults, aliases,
   numeric parsing, version, assets and controls are preserved.
6. Native adapters: [speed](../source/program/crafting/game/crafting_speed_native.cpp),
   [quantity wrap](../source/program/crafting/game/quantity_wrap_native.cpp),
   [quantity shortcut](../source/program/crafting/game/quantity_shortcut_native.cpp),
   and [phone/workbench integration](../source/program/crafting/game/preview_phone.cpp).
   These own fingerprints, layouts, register/float ABI, input binding, correlation
   and supported native execution. The former mixed speed/wrap/shortcut files
   were removed. Existing entity, render, layout, publication and version-profile
   code remains the engine boundary.

## A real before/after

Previously, `QuantityShortcutInput` combined raw button masks, release latches,
sequence fields, eligibility queries and writes to the native transition queue.
The actual decision now reads:

```cpp
if (!fresh || !shortcut.QuantityHeld() || shortcut.QuantityConsumed() || !shortcut.SupportsQuantity()) return false;
if (!shortcut.TransitionAvailable() || !shortcut.QuantityChildReady()) return false;
shortcut.RequestQuantitySelector();
shortcut.ConsumeQuantity();
shortcut.ReportQuantityOpened();
return true;
```

The preceding code visibly gives Y ownership of every Y sample and considers
X Favorite only when Y is absent. `SupportsQuantity` calls the existing native
recipe/material/multiple queries. `RequestQuantitySelector` publishes the same
supported native MultiCreation transition; it does not fabricate a recipe limit
or directly craft an item. Native opening, crafting and B cancellation remain.

The preview has a similarly visible successful-create transition:

```cpp
const auto progress = runtime.CreateItemModel();
if (progress == ModelProgress::Failed) { runtime.ReleaseRenderer(); FinishClose(); }
else if (progress == ModelProgress::Created) {
    ResetItemRotation();
    runtime.ShowPreview();
    phase_ = Phase::Showing;
}
```

`Created` means the supported native model request returned a model handle.
Material/scene readiness is still checked natively before navigation/rotation;
this does not promise that every resource or GPU operation has finished.

## Description-to-code map

| Player-facing behavior | Actual feature decisions | Native work retained |
| --- | --- | --- |
| Automatic crafting/customization/cooking speed | `CraftingRate` | Per-actor native elapsed counter, local-player/readiness/suspension gates, state reset/increments, animation-track writes and original exit handling. Zero skips the hooks and preserves native/manual speed. |
| Quantity endpoints and hold timing | `HandleQuantityEndpoints`, `ResetQuantityHold`, `CancelQuantityHold` | Verified workbench/kitchen selector layouts, snapshot/resident checks, native cyclic row transition/binding/animation/cScroll feedback, scoped vertical neutral input and native ordinary repeat. |
| Y Set quantity / X Favorite | `HandleQuantityShortcut` | Native validity/material/multiple queries, child readiness, pending-transition slot, queued Favorite action and native MultiCreation entry/update/cancel. Existing localized caption and synchronous footer-icon copying remain in the adapter. |
| A opens the selected DIY recipe preview | `QueueRecipePreview`, `HandleRecipePreviewInput`, `ShowRecipePreviewHint` | Actual focused-row membership, native selection/material mapping, validated loaded capture surface, scoped input consumption and native footer pool/batch handling. The full eight-byte item is copied before deferral. |
| Acquire, create, show, close, timeout and deactivate | `Session::Frame`, `DeactivateRecipePreview` | Verified native phone/empty renderer acquisition, native model creation/release, controller/parameter/mode ownership checks, saved parameters/pane restoration and native state/camera balancing. |
| Previous/next recipe and held repeat | `Session::Navigation::Step`, `NavigateRecipePreview` | Current native sorted/filtered list; grid reset/activate/focus and detail updates; existing-model replacement, viewport, carrier continuity and pose publication. |
| Turn/tilt DIY items, preserve fixed native poses | `HandleRecipePreviewInput`, `PreviewStickAxis`, `RotateRecipePreview`, session rotation/reset methods | Live ItemParam identity/kind validation, native starting poses/matrices, Angle32 encoding, late native entity application and coherent render publications. |
| Workbench up/down tilt | `PreviewStickAxis`, `TiltWorkbench` | Verified workbench page/profile, resident input, current controller/handle/item correlation, pending/consumed input guards, native yaw/scroll/confirm behavior and fixed-pose handling. |
| Preview capture/background/Hotel banner | `Session::Frame` explicitly requests show/banner placement; `NavigateRecipePreview` restores the banner before native details update | Existing loaded-layout patch, capture ordering/materials, calibrated cream clear color, native hierarchy restoration, models/poses, scene/render ownership and draw scopes remain in their adapters. |

Crafting/cooking share the verified speed, selector and shortcut paths. The
workbench tilt extension retains its existing workbench-page scope. DIY app
previews remain independently configurable, and direct quantity is active with
Crafting independently of quantity wrapping.

## Ownership, ordering and cancellation

- Speed samples the original actor counter; it adds no actor cache or second
  update hook. The exact sampled 60-update logarithmic curve and the special
  confirmed 3x calculation are preserved; other settings scale its gain.
- Quantity holds are correlated with the native page/grid in four bounded
  slots. Opening/reset disarms them. A direction already held on open cannot
  wrap; rearming requires the smaller centered-stick zone, not pressure jitter
  or a diagonal. Eligible endpoint movement uses the original cyclic path once,
  then samples the clock and neutralizes only vertical input for 300 ms.
  Native repeat resumes afterward with the wrap latch disarmed. Release cancels
  the pause; input suppression, consumption and A/B remain authoritative.
- Cyclic selector preparation is a version-specific adapter responsibility.
  The native binder corrects only one lap; the existing wrapper normalizes
  padded rows when the material-limited range is shorter than the pool. It
  retains native formatting/material setters and restores the borrowed binding
  fields. This is supported adapter scaffolding, not a claim that an arbitrary
  native list offers a complete cyclic operation.
- Shortcut bindings are resident/sequence/page correlated and reset on native
  enter/leave. Every Y sample is consumed even when ineligible, protecting
  against its old Favorite binding. Consumed queries deliberately use the
  original dispatch mask, before this handler's own writes. Favorite stays a
  native queued action; quantity stays a native queued child transition.
- DIY input and session state are UI-thread owned. The adapter validates the
  snapshot/row before reading buttons. Queueing requires native phone readiness,
  a focused recipe row, supported count/index/item and bound preview panes.
  The item is copied; native recipe rows do not survive into deferred work.
  Renderer acquisition happens on a later native phone frame and can decline.
- The normal preview lifecycle is Idle -> Queued -> Starting -> Showing -> Closing ->
  DrainBack. Starting waits through at most 120 updates; the following update
  releases/restores and drains B. B can close queued/loading/showing work, and
  native deactivation releases a non-idle session before its original callback.
  Held A on entry cannot reopen; B drains before ordinary grid input returns.
- Renderer ownership retains the baseline's current-context/controller/parameter/
  mode checks. Losing ownership before creation fails without touching the
  foreign renderer. Release restores only the supported acquired state and
  owned UI. These pointers and copied publications do not establish a native
  object's lifetime, a general lease API or atomic rollback.
- Preview navigation holds one retryable step. Native renderer/material readiness
  can defer it; suppression/invalid snapshots cancel pending work while preserving
  held-repeat history. Neutral input rearms it. D-pad has priority over the left
  stick; opposites cancel, vertical/invalid/consumed input cannot navigate, and
  endpoints/invalid items stop without wrap or skipping.
- Native selection may have intermediate effects before reporting failure;
  Retry does not promise that nothing changed. Only accepted selection resets
  rotation and submits the supported existing-model replacement. Scene/carrier
  ownership remains until native replacement completes, preventing a capture of
  the old mesh with a reset pose.
- Rotation drains each frame's sample once and requires the native renderer to
  be ready/owned. Fixed native categories ignore rotation. Other items use the
  preserved dead zone, 6 degrees per UI update and +/-60 degree tilt. Duplicate
  already-consumed dispatches cannot erase queued rotation. Workbench tilt
  publishes through the existing ownership/publication machinery and leaves
  native horizontal controls and dispatch intact.
- Render/scene workers continue using coherent publications and native-owned
  generation-bearing actor/carrier identities, never the mutable UI Session.
  `preview_entities.cpp`, `preview_render.cpp`, `preview_layout.cpp` and the
  publication/profile headers retain their existing engine/thread contracts.
  See [the preview contract](preview-sessions.md).

## Baseline and validation

`dist/native-first-crafting/baseline/` retains the accepted executable/resource
outputs and source/branch identity; `preservation.json` records all 3,205
pre-existing saved files. Candidate outputs are retained separately. No earlier
branch, release archive or installed mod is replaced.

Focused checks pass on the final normal build: all host policy suites, 43,168
native speed cases, 180 shortcut cases, and the complete preview/wrap/pose/
publication/ownership assertions. The preserved-build comparison matches 1,766
outcome cases plus 92,038 native event/input records at two address bases.
These records include full navigation and quantity timing scenarios, native
call order/frequency, consumed inputs, model angles, restoration and cancellation.
Transient stack event addresses and unobserved actor-request padding are excluded;
all inherited selected-row/material/item/recipe assertions remain intact.

The shared native fixtures/scenarios were extracted for reuse without removing
their assertions. Production configuration/installation and installed hooks are
exercised on both ELFs. Scene/GPU/layout services remain modeled; the inherited
render scenarios also seed scene publications at their explicit fixture boundary.
A foreign/unfinished renderer has no modeled native base-state table, so those
ownership-loss cases use the established installed-phone-callback boundary.
This avoids treating an incomplete foreign fixture as a valid engine controller.

| Check | Final result |
| --- | --- |
| Pinned normal build | Passed without compiler warnings/errors; `final-build.log`. Root outputs match the separately saved `candidate/` hashes. |
| Full normal regression | Passed, exit 0: all 23 host suites and 51 ARM/resource scripts; `full-check.log`. Includes all 128 module subsets at two address bases, actual production activation, shared input/reveal ownership and failure paths. |
| Crafting native regressions | Passed: 43,168 speed and 180 shortcut cases, plus the complete preview/quantity/pose/capture/publication/ownership assertions, including workbench/kitchen coverage. |
| Preserved-build Crafting comparison | Passed: 1,766 direct outcome cases and 92,038 native event/input records, totaling 93,804 comparison units across two address bases. Navigation, cyclic movement, timing and feedback streams retain their original assertions. |
| Shared baseline and coexistence | Passed: 2,156 Controls comparisons, 1,136 Dialogue comparisons, original 978 Pockets callback comparisons, native hook-site/readiness parity, and all remaining transaction/UI/Catalog suites. |
| Packaging | Resource/config/manifest checks and a temporary source archive passed. Moved policies/adapters are included; superseded implementation files/build outputs are excluded. No release archive was written. |
| Preservation | All 3,205 pre-existing saved files retain their hashes. Crafting resource/config bytes, original `main` and the earlier infrastructure branch remain unchanged. |

`dist/native-first-crafting/verification.json` records final results and identities.
The normal `candidate/` contains the exact implementation commit, baseline commit,
pinned compiler image, zero diagnostic flags and a SHA-256 manifest. Documentation
is committed separately and does not require rebuilding the executable.

## Live validation still required

No game/emulator launch, installation update or save mutation was performed.
Earlier accepted Eden behavior is baseline evidence, not validation of this new
binary. Check crafting/customization/cooking ramp restarts and manual speed zero;
quantity endpoints, material-limited ranges, native slide/sound and held input at
supported frame rates; Y quantity/X Favorite and B cancellation on workbench and
kitchen pages; preview opening/loading/closing/reopening, busy renderer handoff,
filtered-list navigation, fixed/rotating items, workbench tilt, capture/cream
background/Hotel banner and Pockets coexistence. Offline native fragments and
modeled services do not establish GPU visuals, sound, live scene scheduling,
multiplayer behavior or native lifetime beyond the documented engine boundaries.
