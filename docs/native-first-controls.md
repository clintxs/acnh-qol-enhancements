# Controls: readable decisions over native interaction and movement

This pass continues `refactor/semantic-feature-api` from the accepted Faster
Dialogue commit `aa29060dfb5f57a574ff57b6028e7192c8a9e9bb`. It stops at Controls.
Swimming and the camera fix are included because they already ship in Controls;
their controls, settings and behavior are retained.

## Read in this order

1. [Controls feature code](../source/program/controls/features/controls.hpp):
   start at `ObservePickupHold` / `PickUpWhileHeld`, then read
   `DialogueHurryBlock` / `DialogueAdvanceBlock` and `AllowSwimmingCameraTilt`.
   These are the actual production decisions, with no native fields, register
   numbers or hook-installation work in their bodies.
2. [Traversal and equipment lifecycle](../source/program/controls/features/traversal.hpp):
   `TraverseWhileHeld` shows native-hop priority, placed/held/pocket tool order,
   native timing, and check versus commit. `BorrowTraversalTool`,
   `ContinueBorrowedTraversal`, `FinishTraversalAction` and
   `RestoreTraversalTool` show the complete borrow/cancel/restore lifecycle.
3. [Swimming policy](../source/program/controls/features/swimming.hpp):
   `SwimWithStick`, `StickThrottle`, `BoostEnvelope` and `SwimmingPropulsion`
   show analog propulsion, opening/manual strokes, cooldown, Y priority and
   surface/diving/session behavior. This preserves the existing mod-owned
   propulsion calculation; it is not presented as a new native engine ability.
4. [Settings and assembly](../source/program/controls/controls_module.cpp):
   the same settings and activation IDs connect to the existing installers.
   Each adapter calls the feature handlers directly with a callback-scoped
   context; there is no new dynamic registry or declaration lifetime.
5. [Native adapters](../source/program/controls/game/): `pickup_native.cpp`,
   `hurry_native.cpp`, `traversal_native.cpp`, `swimming_native.cpp` and
   `camera_native.cpp` own native layouts, ABI, hook sites, guards, identity
   tables, synchronous scopes and execution. The original mixed source files
   were removed. [Traversal concepts](../source/program/controls/game/traversal.hpp)
   distinguish tool kinds and native crossing outcomes without exposing item
   enum values; the existing version profile remains authoritative.

## A real before/after

Previously, `AutoVaultEntry::Callback` combined `Orig`, raw held-tool values,
position proofs, terrain probes, settings, preference order and tool borrowing
in one native hook body. Now `TraverseWhileHeld` contains the real preference:

```cpp
if (options.placedLadder) {
    auto placed = movementPhase ? traversal.TransferProof(proof, TraversalKind::PlacedLadder)
                                : traversal.CheckLanding(TraversalKind::PlacedLadder);
    if (placed.Valid()) {
        if (!traversal.HoldComplete()) {
            if (!movementPhase) traversal.RememberBank(TraversalKind::PlacedLadder);
            return Crossing::Waiting;
        }
        if (movementPhase) return Crossing::Ready;
        return traversal.RequestTraversal(TraversalKind::PlacedLadder);
    }
}
```

The preceding code finishes native ordinary/adjusted hop searches; the following
code visibly tries the held tool, pocket ladder, then pocket pole. The adapter's
`CheckLanding` performs the existing native check-only calls and distinguishes
an actual valid climb from the native ladder-error dialogue. `RequestTraversal`
revalidates and lets the game reserve the landing and enter its normal action.
`Crossing::Ready` during movement is permission to progress native scheduling,
not a promise that a jump was committed.

## Description-to-code map

| Player-facing behavior | Actual feature decisions | Native responsibility retained |
| --- | --- | --- |
| Hold Y for item/crop/weed/moss pickup | `ObservePickupHold`, `LatchPickup`, `RepeatPickup`, `RepeatMatches`, `PickUpWhileHeld` | Physical Y mapping, context pickup gate, loaded parameter/field-kind decoding, dry-run targeting, actual-source reservation, native capacity/animations. Repeat furniture/fence/design fallbacks stay excluded. |
| Hold Y for nominal 30x reveal | `DialogueHurryBlock`, `HurryRequested` | Reciprocal manager/text ownership, presentation/phase decoding, live active-controller/application fallback, fixed tier encoding and the existing shared SIMD reveal machinery. Faster Dialogue still chooses one tier with hurry precedence. |
| Advance pages/input pauses, stop for choices/scripted waits/new catches | `DialogueAdvanceBlock`, `IsNewCatchAnnouncement` | A callback-local advance request enters the original Advance handler. Native page cleanup, choice handoff, close/reset and state transitions run once. No persistent controller or confirm input is written. |
| Automatic pole/handheld/placed ladder and vine traversal | `TraverseWhileHeld` | Native strength/speed/contact/direction search; wall-contact extension only for climbing; placed-object search; strict native hop timer; native geometry and landing preparation. |
| Borrow a pocket traversal tool silently | `BorrowTraversalTool`, `ContinueBorrowedTraversal` | Bounded copied pocket items, exact equipped association, native ToolChange request/header copy, native entry completion, current terrain revalidation. No inventory writes. |
| Restore tool/empty hands, cancel on changed intent or manual equipment | `ContinueBorrowedTraversal`, `FinishTraversalAction`, `RestoreTraversalTool` | Resident/player/generation correlation, full eight-byte item equality, exact bank/facing, native action decoding, reordered item lookup and native silent restoration. |
| Analog surface/underwater swimming, opening stroke, timed manual A boosts | `SwimWithStick`, `StickThrottle`, `BoostEnvelope`, `LearnCruiseSpeed`, `SwimmingPropulsion` | Native swimming guards, original animation/steering/world updates, system tick, scoped A substitution/restoration, movement helper ABI and pending-stroke native gate return. |
| Right-stick camera tilt while surface swimming | `AllowSwimmingCameraTilt` | Original eligibility initialization/snapshot handling, fingerprinted two-rejection contract, global lock and native three-position right-stick behavior. |

## Ownership, timing and failure

- Pickup state has eight resident slots and is correlated with the native player
  pointer and handle. Release, fresh press or identity change clears the lock.
  Only a successful initial native reservation latches a supported category.
  A failed/unsupported initial attempt cannot latch later during that hold.
  Repeats probe without reserving, then recheck the actual commit source before
  inventory work. A rejected repeat is consumed so other Y fallbacks cannot run.
- Pickup reservation and traversal probe scopes are synchronous game-thread
  borrows. Previous scopes are restored after nested native calls. They provide
  no cross-thread or retained-pointer guarantee.
- Stable traversal validation produces a one-use movement proof, consumed on
  the next attempt. Same player/handle, held kind and exact alternate bank must
  match. Each actual traversal is revalidated; this is not a cached reservation.
  The native strict `counter > threshold` comparison is unchanged (30 waits;
  31 is ready for the default parameter).
- Borrow state copies full item values, slots, player identity, bank and facing.
  It waits for ToolChange entry and subsequent locomotion entry before trying
  traversal. Strong stick intent, matching facing/bank and native landing are
  checked again after equip. Cancellation or rejected commit requests restoration.
  A manual tool change or identity change drops ownership without restoration.
- Traversal restoration waits for actual traversal entry followed by completed
  native locomotion entry; queued Move/Idle gaps cannot trigger it. Restoration
  also runs when stationary/input unavailable. Exact item lookup handles pocket
  reordering; removal restores empty hands. State clears before requesting the
  restore, so rejection cannot replay it. Native request rejection still has
  the baseline's limited guarantees: there is no new retry or atomic rollback.
- Hurry context operations are lazy: ownership and phase checks pass before the
  next borrowed object is read. Fixed first-catch IDs stop only synthetic advance,
  retaining fast reveal and manual input. Unknown/transient states fail closed.
  Native text protections, authored timers and rate overrides remain native.
- Swimming state resets for changed identity and relearns cruise speed after
  mode/session changes while preserving the existing stroke cooldown. Scoped A
  changes are restored after the native callback; unrelated input and native
  steering remain untouched. Stroke boost configuration changes propulsion,
  not stroke scheduling. Camera activation remains independent of auto-swim.
- Compatibility checks, partial-feature availability, hook publication and
  diagnostics stay in adapters. This pass adds no installation/idempotency or
  native lifetime guarantee. Shared inventory operations and shared dialogue
  reveal infrastructure do not depend on Controls' feature implementations.

## Validation and retained baseline

The accepted baseline executable/resource copy and branch/source identity are in
`dist/native-first-controls/baseline/`; `preservation.json` records all 3,173
pre-existing saved files. Normal and diagnostic outputs/logs are kept separately.
No release archive, installed mod or earlier branch is replaced.

Implementation commit: `3c1e2138c70400378e107ca0a7f13af461079840`. The final normal
run passed with exit 0; results follow. Host checks compile
the actual feature headers without the
native runtime. Compiled comparisons boot/install both builds through production
configuration; they do not assign private policy state. Existing assertions were
retained. The fixture callback lookup now distinguishes a callback from a policy
template containing its local context type, and the pickup fixture has a main
guard so the same machine can be imported for baseline comparisons. Packaging
source-path checks follow the moved files; a temporary source archive was checked
and removed without creating a release.

| Check | Result |
| --- | --- |
| Pinned normal build | Passed without compiler warnings/errors; `final-build.log`. Final root outputs match the saved `candidate/` hashes. |
| Full normal regression | Passed, exit 0: all 22 host suites and 50 ARM/resource scripts; `full-check.log`. Includes production activation, all 128 module subsets at two address bases, shared reveal installation orders and native compatibility/failure paths. |
| Current/baseline Controls comparison | Passed: 2,156 comparisons at two address bases, through production configuration/installation. Pickup event order, swim input/speed/restoration, camera results, traversal query/request frequency, borrow cancellation/restoration and hurry transitions match the accepted build. |
| Shared regressions | Passed: 1,136 Dialogue baseline comparisons, original 978 Pockets callback comparisons, reveal/name tests, Crafting previews, Shopping Card transactions, Fitting Room and Catalog. |
| Traversal diagnostic | Build and all four traversal scripts passed, including 268 vault, 144 handheld ladder, 180 placed ladder and 150 borrow/restore cases; native GP/SIMD trace assertions retained. Separate outputs/logs in `vault-diagnostic/` and `vault-diagnostic-*.log`. |
| Hurry diagnostic | Build and four diagnostic/Controls dialogue scripts passed, including bounded rejected-frame logs, 1,024 independent Controls dialogue, 712 hurry and 772 catch scenarios. Separate outputs/logs in `hurry-diagnostic/` and `hurry-diagnostic-*.log`. |
| Packaging/resources | Resource/config/manifest checks passed; temporary source archive includes the actual policies/adapters and excludes superseded files/build outputs. No release archive was written or replaced. |
| Preservation | All 3,173 pre-existing saved files retain their hashes. Controls resource/config bytes, original `main` and the earlier infrastructure branch remain unchanged. |

`dist/native-first-controls/verification.json` records the final results. The
normal candidate and both diagnostic folders contain source/build identities and
SHA-256 manifests. The normal candidate records the implementation commit above,
zero diagnostic flags and the pinned compiler image. Documentation is committed
separately; it does not require rebuilding the executable.

## Live validation still required

Earlier Eden confirmations are baseline evidence, not validation of this new
binary. No emulator installation, game launch or save mutation was performed.
Check repeated pickup against capacity/protected targets; ordinary/placed/pole
and uphill/downhill traversal; cancellation during silent equipment changes;
restoration after landing idle and manual ownership changes; swimming cadence,
steering, Y transitions and camera locks; hurry on choices, explicit pauses and
first catches with Faster Dialogue both present and absent. Offline tests execute
selected original native fragments with modeled inventory, terrain, resources,
animation and renderer services; they do not establish live scheduling, collision,
multiplayer behavior, sound or visual smoothness.
