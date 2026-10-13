# Preview session and adapter contracts (stage 6)

Feature installation is now a small declaration of phone-preview/workbench
behavior. `services/preview_session.hpp` describes when a preview opens,
navigates, rotates, closes or times out. Native model, layout, entity and drawing
mechanics live in `game/preview_phone.cpp`, `preview_layout.cpp`,
`preview_entities.cpp` and `preview_render.cpp`. These are internal components
for the current mod and supported ACNH build.

## Session policy and asynchronous work

| Phase | Accepted work / transition |
| --- | --- |
| Idle | A validated, exact selected item can queue Open. |
| Queued | The next native phone update attempts to acquire the renderer. Failure retains the grid and returns Idle. |
| Starting | Poll native readiness, then request the item model. Failed creation or more than 120 updates closes/restores. |
| Showing | Navigate, place the banner and rotate during phone update. One pending navigation step can wait for loading; holds repeat after 12 then every 4 updates. |
| Closing | Explicitly release/restore owned native state and enter DrainBack. |
| DrainBack | Suppress native grid input until B is released, then return Idle. |

`Open` starts a lifecycle, not synchronous model creation. The native adapter
validates phone/selection/focus/renderer state and copies all eight item bytes
before queueing it. `Navigate`, `Rotate` and `RequestClose` express policy without
native offsets or state numbers. Input adapters own button/stick interpretation,
consumption, edge history and blocked/suppressed sampling. UI-thread session
policy owns timeout/repeat/pending-input state. It never owns a native pointer.

The phone adapter privately owns the saved layout flags, parameter bytes,
controller association and model reservation. Native phone update runs before
session work. Starting work rechecks the current controller/parameter/mode
association before dereferencing/requesting the cached model. A changed lease
cancels, clears publication and restores the owned phone UI; it does not modify
foreign renderer state. Showing navigation/rotation retain their existing
current-context/material-readiness gates.

Close is an explicit operation, not a stack destructor pretending to own future
engine state. Native model release and parameter/mode restoration occur only
when the acquired controller still belongs to the current supported context.
Grid/scroll/cursor visibility, capture/surface visibility, banner hierarchy,
footer refresh and input draining retain the original lifecycle. Unobserved
native destruction cannot be detected from a raw address. Controller pointers
are correlations under supported native callbacks, not fabricated stable handles.

## Thread ownership and publication

- Phone sessions, saved UI state and input history belong to the UI dispatcher.
- UI pose/recipe/handle fields publish together through a single-writer version
  clock. Workbench controller/handle/mode/tilt uses its own clock. Every payload
  access and clock access is sequentially consistent; an unchanged even version
  across the complete read establishes one coherent packet in that total order.
- A reader makes one bounded attempt. An active or intervening write declines
  the override and retains native behavior for that call. Readers do not spin,
  lock the UI or inspect mutable session storage. Writer scopes contain only
  synchronous atomic stores, not native calls, model waits or deferred cleanup.
- Origin, selected item, epoch, requested pose policy, current pose and full
  generation-bearing root handle publish in one transaction on model creation
  and navigation. Ordinary rotation publishes another complete pose update.
- Actor/carrier registry traversal and pose caches remain on their original
  supported native entity-update path. Full root/child handles, parent identity,
  kind, live flags and component-owner checks remain mandatory. Render workers
  receive native components from their supported callback and published values;
  they never traverse the entity registry or read UI session objects.
- Fixed-pose/scene-owned flags are separate scalar gates for retaining native
  matrices. They are not a claim that native controller/component memory forms
  a coherent mod-owned snapshot. Native component lifetime and scene-to-render
  scheduling remain engine contracts; offline tests do not prove those live.

Layer background publication is a single layer-identity pointer. The draw
adapter reserves one of eight command scopes, fills renderer/info/view, then
publishes the command with release ordering. The nested drawing callback reads
that command with acquire ordering. Metadata remains immutable until the outer
synchronous native draw returns; the command is then cleared. Serial recording
of a native command buffer and the synchronous call lifetime establish the
scope's coherence. Atomic fields alone would not. A missing/full/mismatched
scope keeps native drawing. No native clear color is overwritten globally.

## Layouts and restoration

The layout adapter identifies MenuRecipe before patching and owns up to 16
immutable, aligned resource copies for the process lifetime. Native Layout::Build
retains its input; no temporary or reused buffer is supplied. Busy patching or
capacity/format failure leaves that resource unchanged. Banner placement and
restoration use the existing native hierarchy. Model matrices preserve their
indirect return ABI and synchronous temporary-copy lifetime; generation checks
and native scene pose/framing behavior remain intact.

## Verification and live limits

The extraction was committed separately from publication/lease corrections.
Policy tests exercise asynchronous failure, timeout, repeated open/close,
navigation/retry and draining. Publication tests force active and intervening
writes and stress a concurrent writer/reader for mixed payloads. The native
suite retains failed creation, timeout, navigation, stale generations,
phone/workbench coexistence, native matrix/SIMD behavior, cleanup and contention
assertions. New cases replace controller, parameters or mode during Starting
and verify no model request, unchanged foreign bytes and restored owned UI at
two load addresses.

Logs and independent outputs are saved under ignored `dist/refactor-stage6/`.
Live Eden qualification remains required for pose, clipping, render layers,
focus, animations, audio and native resource cleanup. No offline result is
reported as integration readiness or proof of GPU/engine lifetime behavior.

The final normal publication build passed all 19 host suites and 46 ARM/resource
scripts, including the preserved baseline comparison. No compiler warnings or
errors were reported. The independently saved publication build remains under
`dist/refactor-stage6/coherent-build/` with its checksums.

A subsequent independent pocket input correction was checked with all 19 host
suites, six Pockets scripts and the stage 0 comparison again. It changes no
preview behavior. Those final outputs and hashes are saved separately under
`dist/refactor-stage6/final-build/`; the complete 46-script integration evidence
for the publication build remains retained alongside it.
