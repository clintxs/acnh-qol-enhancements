# Inventory values and pocket commands (stage 4)

This stage starts from `d82c5a5` on `refactor/internal-feature-interfaces`.
`features/quick_store.cpp` now declares three semantic shortcuts and installs
them once through `services/pocket_commands.hpp`:

```cpp
static constexpr ShortcutRule shortcuts[]{
    {UiButton::Y, PocketAction::PutInStorage},
    {UiButton::ZL, PocketAction::DropOrRelease},
    {UiButton::ZR, PocketAction::EquipOrConsume},
};
```

The registered table has static lifetime. The service validates supported UI
buttons/actions and rejects duplicate button declarations before publication.
It owns the common input loop, native hook group and pending-command lifecycle.
Features cannot manufacture an eligible request or execute an integer action.
The existing single-row Open bypass uses the same service, with its distinct
entry/update timing and setting unchanged.

## Boundaries

| Layer | Responsibility |
| --- | --- |
| Feature declaration | UI button to semantic action mapping |
| Pocket command service | Fresh edges, blocked/consumed input, opaque requests, queueing, cancellation, multi-frame completion and feedback policy |
| Native pocket adapter | Live object fields, native menu construction/row eligibility, state transitions, synchronous dispatch and widget/cursor calls |
| Borrowed resident inventory view | Native capacity/accessors and immediate complete item copies |

`UiButton` is a scoped enum of UI snapshot bits, not HID bits. `PocketAction`
describes supported actions without native identifiers. The version adapter's
`MenuRowId` names the original ItemUIContextMenu row IDs; these are distinct
from the operation numbers produced by native `MapStackAction`. Dispatch still
uses the original native command update and operation mapper.

`game/inventory.hpp` retains the callback-scoped `ResidentView`. Its `ItemAt`
copies all eight native bytes immediately. `ItemValue::Identity()` produces an
owned `ItemIdentity` with exact equality across every bit, including
customization, count and durability. Neither queued requests nor feedback
requests retain inventory pointers or a borrowed view.

## Command lifecycle

The private `FindEligible` constructs a request only after validating the
controller, native phase/queues, mode, slot/capacity, item, home restriction,
menu state/binding and native rows. It records the owner address, copied
resident byte, slot, input row, exact item identity, selected native row,
semantic action and observed lifecycle generation. Equipment remains preferred
over consumption; the first valid consume row remains the fallback. Native row
flags, extra fields and custom callback restrictions remain mandatory.

`Queue` stores the evidence and requests the native Command transition; it
does not move an item. Open-menu entry instead preserves the native input lock
and waits for menu UPDATE before queuing Command. Before starting, Command
UPDATE checks the selection, exact item, resident, generation and eligible row
again. A failed check cancels to browsing when its native queue permits it.

Once started, an incomplete command continues through native UPDATE without
restarting or revalidating an item/menu the action itself may modify. A resident
change still cancels it. The temporary native menu fields are restored after
each update, and the request is cleared only on cancellation or a native queued
transition. Storage's Decide feedback remains after successful removal;
Drop/Release and Equip/Consume emit it once before their first dispatch; Open
adds no feedback to the native A press.

Cursor feedback waits for the native return to Play. It stores only owner,
slot, input row, generation and resident. Play resolves the current page,
widget, focused pane and animation resources afresh, consumes the tap once,
clears pending work and resets the input edge detector. Hold/Release transitions
that close Pockets cannot carry a tap into reopening.

The owner address is a correlation check, not a stable handle. Generations
advance only at the existing verified Play/menu entry hooks. A controller
recycled at the same address across an observed entry cannot reuse an old
request. These hooks do not prove arbitrary pointer lifetime or detect an
unobserved engine destruction; all access remains restricted to the supported
native callbacks. No borrowed widget pointer survives a frame: pre-removal
feedback validity and its widget borrow are used synchronously within dispatch.

## Verification and preservation

`pocket_commands_test.cpp` covers typed boundaries, invalid registrations and
exact item identity. The original 978 pocket callback assertions remain intact
and comparable with the saved baseline. The callback fixture now models the
registered rule span using the actual feature table from the loaded ELF, beside
its existing modeled input-handler registration. Older ELFs retain their
original setup.

`pocket_commands_arm_test.py` adds invalid registration checks and enables 112
additional scenarios at two load addresses: changes to each upper item bit,
resident changes before/during execution, capacity shrinkage, reopening at the
same owner/page addresses, replacement widgets, delayed cursor feedback and a
remapped Y-to-Drop declaration with multi-frame completion. It also verifies
that a started action can modify its item and menu without being restarted.
The existing storage, recycling and hold-select tests remain in the Pockets
group. Engine services and inline adapters are modeled; live Eden effects and
rendering still require later integration qualification.

Stage 4 logs and independent ELF/NSO/NPDM copies are under ignored
`dist/refactor-stage4/`. Main remains at `35ec0c8`; the 2,979 files covered by
stage 0/1/2/3 preservation manifests still match their checksums. No release
archive or installed Eden mod is replaced by this stage.

Final validation: the normal pinned Docker build completed with no compiler
warnings or errors. All 16 host suites and all 42 ARM/resource scripts passed
across the three saved regression logs. The current ELF and preserved stage 0
ELF passed identical original 978-scenario pocket assertions and matching
migrated hook-site/readiness comparisons. Pockets also passed the 112 new
service scenarios (1,090 total in its extended run), ten invalid-registration
cases, and inherited storage-hint, recycling and hold-select regressions.
