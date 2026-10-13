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
