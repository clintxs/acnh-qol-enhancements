# Shared input, captions and resource contracts (stage 5)

Features declare ordered input behavior, owned caption keys and minimum resource
requirements through the existing Core services. These are bounded startup
registries for this mod, with no runtime rebinding/unregistration or external SDK.

## Input

`PageInputContext` is a synchronous borrow of page/snapshot plus native blocking
and a semantic dispatch/suppression reason. Domain adapters interpret native
objects. Handlers return `InputOutcome::PassThrough` or `Consume` and register
with explicit priority. Priorities preserve the shipped order: Pockets, Preview,
Workbench Tilt, Quantity Wrap, Quantity Shortcut. Equal priorities preserve
registration order. There are five handler and five observer slots.

Pass-through processing is handlers, before-observers, native input once, then
after-observers. A consuming handler suppresses native/after dispatch and sends
a blocked suppression sample to skipped typed handlers and before-observers.
Domain adapters cancel/drain queued per-frame movement and gestures; held input
continues to be sampled so blocked presses cannot fire late. The internal
suppression bit is removed by typed bridges and never enters native dispatch.

Registration reports Ready, Identical, Invalid, Conflict, Capacity or Unavailable.
Identical registrations revalidate owned code; incompatible priority/callback
pairs and full registries fail before publishing a dependent callback. The
native adapter bridge is the same registry, not a parallel legacy subsystem.

## Captions

Features now request `RegisterCaption(CaptionKey::Bag, BagCaption)` or the UseItem
and DropOrRelease equivalents. Each of the three known keys has one exclusive
provider slot. Another provider for an owned key is a conflict. Identical reuse
rechecks shared hook ownership. Private key names are resolved centrally;
ordinary/unowned keys retain native initialization and translation behavior.

A provider must return valid, immutable, process-lifetime UTF-16. Current
providers return static fallbacks or entries from catalogs loaded once per
process. The service scans at most 128 units and accepts 1–127 non-NUL units.
Missing, empty or overlength text uses an immutable service fallback. No temporary
text pointer is retained; native text setters synchronously receive the provider
or static fallback. Existing catalog parsing owns UTF-8/locale validation.

The service owns native prepare-operation, setter and binding notification
ordering, which is required for footer sizing. Registration validates the shared
initializer and notification code. At runtime, publication requires a payload,
recognized pane/group setter, interface and prepare callback.

Deliberate suppression policy: registered private keys have no native MSBT
translation. After normal native initialization, state 12 suppresses that lookup
even if the runtime adapter cannot publish. An unsupported setter, missing
payload/interface or missing prepare callback produces no guessed text call or
notification. This preserves the prior blank/suppressed fallback and native
cleanup instead of requesting a nonexistent translation. Valid adapters publish
prepare -> set -> notify. Pockets still resolves native-language Drop labels
before this service when no custom override exists.

## Resources

`RequireMinimum(normalizedPath, bytes)` owns a copy of each exact path in four
bounded slots (maximum 95 bytes, no embedded NUL). Existing paths can only raise
their floor. Invalid/full declarations fail before hook publication; identical
or lower requirements revalidate the hook and do not lower allocations.

Registration is synchronous during content-mount activation, before dependent
UI/resource use; the table is read-only after startup. The native size query
retains successful lookup metadata and larger allocations. Missing entries get
the required floor and zero native flags, matching prior behavior. Compression
suffixes, case changes and near matches do not receive another path's floor.
Shopping/Fitting preparation now delegates to these requirements. Resource
support is activated from installed regular package markers independently of
whether their gameplay feature is enabled.

## Validation

The pinned normal build passed without compiler diagnostics. All 17 host suites
and 46 ARM/resource scripts passed across saved focused/integration logs under
ignored `dist/refactor-stage5/`. The initial UI run stopped on a stale removed
provider symbol after Fitting/Shopping passed; the updated Pockets run passed
all six scripts. Core and remaining gameplay runs passed, including combined
activation, capacity/conflicts, partial publication and the preserved stage 0
hook/readiness and 978-pocket-scenario comparison.

New tests cover explicit input priority, before/native/after order, suppression
cancellation, caption-key conflicts, missing/empty/overlength text, maximum text,
invalid setter/prepare/payload suppression and owned exact resource paths with
monotonic floors. Native sound, footer sizing and rendering still need Eden
validation. No installation, release or main-branch update is performed.
