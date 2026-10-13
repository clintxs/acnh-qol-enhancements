# Internal refactor review follow-up

Work continues on `refactor/internal-feature-interfaces` from `c32f267`.
Main, earlier build snapshots, releases and the working installation are retained.

## Native pocket input boundary

Before, the command service interpreted page/controller vtables, native rows,
focus, masks and consumed fields. It now reads a callback-scoped `PocketInput`,
selects a semantic action, queues validated evidence and asks the adapter to
consume the shortcut. Controller/focus resolution moved from feature helpers
into `pockets/game/pocket_input.*`. Native diagnostics and installation now
live apart from command policy.

The native adapter is the sole owner of shortcut edge state. It tracks valid
samples even while blocked or a command is pending; Play entry resets it before
native Play. First-held samples, chords, wrong input rows, blocked/consumed
buttons and ordinary native dispatch retain their prior behavior. Input-row
borrows never enter pending commands. Semantic phases and opaque row evidence
replace native state numbers and menu identifiers in policy.

Validation: normal pinned build and all 16 host suites plus the four Pockets
scripts passed, including the original 978 and extended 1,090 pocket cases,
storage hints, recycling and 96 hold-select scenarios. Logs are in ignored
`dist/refactor-boundaries/input-*.log`. Native services/rendering remain modeled;
no live Eden validation is claimed.

## Pocket lifecycle dependency

Before, command Play entry called hold-select directly. Now hold-select
registers its own reset with `pocket_lifecycle`, a startup-only one-listener
notification. Shared services include no feature implementations. Identical
registration is idempotent; a different listener fails before hold-select
publishes input/feedback behavior. Timing is unchanged: notify/reset first,
then invalidate pending commands and input edges, call native Play, and finally
resolve/emit a valid cursor tap. This is deliberately not a general event bus.

Validation: normal build, 16 host suites and inherited Pockets scripts passed;
a focused compiled test verifies listener identity/capacity and no dependent
publication when the slot is unavailable at two bases. Logs are
`dist/refactor-boundaries/lifecycle-*.log`.

## Owned shortcut registration

Before, `InstallShortcuts` retained a caller-owned span. It now copies at most
three validated rules into service storage. Caller storage may be temporary or
changed immediately after installation. Identical declarations in the same
order revalidate owned hooks without resetting state or allocating more slots;
incompatible declarations fail before changing the table. Partial publication
keeps the declaration locked to the same rules for a compatible retry.

The private-span remapping test was removed. A fresh-module integration fixture
now installs Y-to-Drop through production `InstallShortcuts`, destroys caller
data, verifies identical reuse/conflict/foreign-hook rejection, then executes
registered input and real native command entry/update across three updates
with exactly one feedback. Only native-world UI data is reused by this fixture;
no service or module state is copied. The callback-only fixtures still model
registration for the preserved 978-case baseline comparison.

Validation: normal build, 16 host suites and six Pockets scripts passed,
including successful production registration at two bases and the remaining
1,088 extended callback cases. The two remapping cases now live in the stronger
production registration test. Logs: `dist/refactor-boundaries/ownership-*.log`.

## Inventory view safety

The verified Capacity/ItemAt/IsEquipped/Held native functions read the first
resident byte as an identifier. Their disassembly copies that byte into local
resident arguments before registry lookup; Held retains its nontrivial X8 result
ABI. `ResidentView` now owns the byte instead of borrowing a caller pointer.
This does not establish resident/player/controller lifetime.

Unchecked `ItemAt` is no longer exposed. `Slots()` snapshots native capacity
once per synchronous callback traversal; `TryItemAt` and `IsEquipped` reject
slots outside that capacity or the 40-slot limit before native access. Quick
store and traversal reuse this snapshot rather than rechecking capacity for
every item. Exact eight-byte copies and native read-lock behavior are retained.
The callback fixture now checks resident-byte contents rather than requiring
the old controller-interior pointer, matching the verified ABI contract.

Validation: normal build, all 16 host suites and all Pockets/Controls scripts
passed, including capacity shrinkage, exact bits, tool restoration, X8 held-item
returns and 978 preserved pocket callbacks. Compile-time checks reject pointer
construction and unchecked inventory access. Logs:
`dist/refactor-boundaries/inventory-*.log`.

## Typed shared input

Handlers now register typed callback-scoped contexts and return `PassThrough`
or `Consume`. Explicit priorities preserve the shipped Pockets, Preview,
Workbench, Quantity Wrap and Quantity Shortcut order, independent of activation
order. Equal priorities preserve registration order. Observer callbacks bracket
native dispatch; consumption instead sends blocked/suppressed cancellation to
skipped typed consumers and before-observers, with no native/after call.

The same bounded five-handler/five-observer registry reports invalid, conflict,
capacity and native-unavailable registration results. Identical registrations
revalidate hook ownership. Existing domain adapters drain blocked snapshots,
so held edges, preview motion and selection gestures do not become delayed
operations. No internal suppression flag reaches native input.

Validation: build, 16 host suites, Pockets/Crafting scripts, the activation suite
and a focused production ordering/cancellation test passed at two bases.
Fixtures invoke the typed bridges rather than discarded legacy wrappers. Logs:
`dist/refactor-stage5/input-*.log`.

## Caption and resource declarations

Wildcard caption-provider searches are replaced by exclusive typed keys with
bounded text/fallback behavior. Resource flags are replaced by owned exact-path
minimum requirements. Native prepare/set/notify, metadata preservation and
allocation floors remain in Core. The deliberate missing-adapter MSBT
suppression policy is documented and tested. See `shared-input-ui.md` for
ownership, lifetime, limits, ordering and all stage 5 validation evidence.

## Preview session extraction

Before, `recipe_preview.cpp` mixed phone input/lifecycle, model allocation,
layout caching, entity registry traversal and render-worker hooks. Its feature
entry points now install phone-preview/workbench behavior through internal
adapters. `crafting/services/preview_session.hpp` owns the UI policy: asynchronous
Open, model-start timeout, Navigate/Rotate input, one pending navigation step,
Close and back-release draining. It knows no offsets, ABI registers or native
state numbers. Its runtime contract owns native work/restoration explicitly.

Native phone/model state is private to `preview_phone.cpp`; saved layout data
and immutable resource caching live behind layout adapters; generation-bearing
entity/parent/component validation lives in `preview_entities.cpp`; rendering
and its native hook contracts live in `preview_render.cpp`. Shared declarations
are internal to those adapters. This mechanical extraction retains the existing
atomic publication rules; coherence changes are reviewed separately next.

Validation: normal build, 18 host suites, six Core scripts (including the full
preview/native-path suite) and both Crafting scripts passed. New policy tests
cover waiting/failure/120-frame timeout, navigation retry/repeat, close/drain and
20 repeated cycles. Existing native assertions for matrix/hidden-result ABI,
entity identity, restoration, phone/workbench coexistence and hook conflicts
remain. Logs: `dist/refactor-stage6/session-*.log`.
