# Fitting Room: readable bag, checkout and choice behavior

This module continues `refactor/semantic-feature-api` from accepted Catalog
commit `898768aa7efe5d77c9b0536c0034cc22a8394db7`. It stops at Fitting Room.
Implementation commit: `59dbb4e055a1e242ffdc242b234fa4ec15c20f36`.

## Read in this order

1. [Feature implementation](../source/program/fitting/features/fitting_features.cpp):
   start at `ToggleBagSelection`, then `CanAddToBag`, `PrepareBagPurchase`,
   `OpenBagConfirmation`, `AnswerBagConfirmation` and `CommitBagCheckout`.
   These show real item selection, removal priority, price/capacity checks,
   purchase composition, bag-only confirmation, revalidation and native execution.
2. In the same file read `HandleBackShortcut` for B's focus-then-confirm behavior,
   `BagBadgeVisual`/motion helpers for row feedback, and `AppendBagHint` for Y's
   footer limits. `PriceWithBag` and `GrossPriceWithBag` explain total/statistic
   additions. `Bag` methods retain exact-item membership and overlap accounting.
3. [Declarations and feature state](../source/program/fitting/features/fitting_features.hpp)
   and [bag declarations](../source/program/fitting/features/bag_policy.hpp).
   Substantial implementation is in `.cpp`; the previous header-only Bag policy
   is removed. The tiny exact eight-byte value comparison remains inline in
   [the native item type](../source/program/fitting/game/item.hpp).
4. [Supported operations](../source/program/fitting/game/fitting_operations.hpp):
   callback-local purchase observations, native confirmation/checkout requests,
   focus and footer primitives. Their types do not depend on feature classes.
5. [Native bag adapter](../source/program/fitting/game/shopping_bag_native.cpp)
   and [hint adapter](../source/program/fitting/game/fitting_hint_native.cpp):
   controller/page/input correlation, raw state mappings, native collections and
   price ABI, pane/counter/cursor mechanics, transient input forwarding, caption
   loading and original hook/vtable installation. Verified instruction evidence
   remains in `game/fingerprints.inc`.

## A real before/after

Previously `features/shopping_bag.cpp::PreparePurchase(owner)` interleaved
controller fields, stock pointers, native list headers and the mod's composition
rules. `features/bag_policy.hpp` also contained the Bag implementation. Now the
actual feature loop reads:

```cpp
if (!source.HasStock(entry.item)) return invalid();
const int quote = source.Quote(entry.item);
if (quote < 0 || quote > 1000000) return invalid();
entry.price = quote;
if (Bag::Contains(outfit, count, entry.item)) continue;
// After checking the bounded plan capacity:
plan.send[plan.sendCount++] = entry.item;
plan.wear[plan.wearCount++] = entry.item;
plan.extraPrice += quote;
```

The feature decides which selections are extra, which delivery alternatives
receive them, and what is charged. `source` exposes current native stock, price,
collection and capacity observations. The game still executes payment, tickets,
clothing swaps, storage/pocket delivery, purchase records and milestones through
its original synchronous checkout body.

## Description-to-code map

| Player-facing behavior | Actual feature decisions | Native work retained |
| --- | --- | --- |
| Y adds/removes the selected exact item | `BagInputEligible`, `ToggleBagSelection`, `Bag::Find`/`Toggle` | Actual focused item through current stock/filtered indices; owner/page/vtable, resident input, native suppression, consumption and pending-command gates. Fresh-edge state is reset on session/input-row changes. |
| Multiple colors/variants, up to 256 selections | `Bag::Toggle`, exact item equality, `ExtraCount`/`ExtraPrice` | Copies the entire eight-byte native item, including flags and extra. Stored prices are native quotes. Removal remains possible at full capacity and skips new price/space queries. |
| Conservative storage/pocket capacity | `CanAddToBag`, `Fits`, `PrepareBagPurchase` | Original send/old-clothing collectors, native resident/HasStorage/free-space queries and their bounds. The feature reserves the larger delivery alternative; payment freeing Bell/ticket slots is deliberately not assumed. |
| Native no-space feedback on Y | `ToggleBagSelection` chooses a capacity error only for actual NoSpace below the bag limit | Existing storage/pocket error question and transition. Invalid/unresolved observations do not falsely report full storage. A valid selected-item attempt consumes Y even when rejected; an unresolved selection falls through. |
| Bag row badges and add/remove feedback | `StartBagBadgeMotion`, `BagBadgeVisual`, `AdvanceBagBadgeMotions`, membership test | Bounded row-local pane discovery, native scale/alpha fields, dirty-matrix bit, native cursor animation and direct selected-row/cursor target identity checks (row+218 and cursor+218), plus visibility/resident guards. The target is not a pane and its parent fields are never traversed. Native row rendering runs first; invalid/reused rows get the current hidden/default result. |
| Total price includes bag extras once | `PriceWithBag`, `Bag::ExtraPrice` | Original current-outfit pricing (including tickets), exact outfit deduplication, native total field and number-counter transition/retry. Exact overlap is neither charged nor delivered twice. |
| Purchase prompt, including bag-only checkout | `OpenBagConfirmation` | Native purchase-command mapping, total/funds fields, existing confirmation/error question and pending-state transition. Initial preflight is diagnostic; answer/commit revalidate independently. |
| Bag-only confirmation skips the wear question | `AnswerBagConfirmation` requires a confirmed purchase, idle command and exact empty outfit slots, then selects native send | All ten persistent stock indices must be the empty sentinel. Failed/out-of-range stock resolution is not interpreted as no outfit. Original answer handling queues native send; no payment occurs in this callback. |
| Checkout composition and failure precedence | `PrepareBagPurchase`, `AnswerBagConfirmation` | Both native delivery alternatives copied before payment; actual outfit/collector agreement; exact stock membership; fresh native quotes; resident/space guards. Funds error retains precedence, invalid plans return to browsing, and actual NoSpace uses the native error descriptor. |
| Native execution, repeat guard and cleanup | `CommitBagCheckout`, `CheckoutStep` | Original checkout called once within the active frozen-plan scope. Native collection consumes module-owned pointer arrays; payment, tickets, routing, clothing and milestones stay native. Cleanup follows native return; `done` prevents repeated payment. |
| Complete totals/statistics beyond 16 items | `GrossPriceWithBag` adds frozen extra price only during the matching active checkout | Native serializer's fixed 16-item detail preview is bounded; payment totals, catalog registration and per-item native loops retain all purchases. Supported extended list capacity is 266, statically tied to 256 bag slots plus ten native outfit entries. |
| B focuses No, then confirms it on another fresh press | `HandleBackShortcut` explicitly chooses `FocusSend` or `ConfirmSend` | Exact live wear-choice delegate, ready/transition/answer/two-choice/style checks, enabled current cursor, native FocusButton and original input/answer dispatch. Already-focused No can confirm on the first fresh press. Physical A remains native. |
| X applies/removes a native tailor ticket | `RemapTailorTicketInput` | Rebind only the fitting page ticket button input unit from bit index 7 (Y) to 6 (X), before native footer construction and private input forwarding. Native input dispatch and footer both read unit+14; eligibility, repeat, ticket assignment and payment remain native. |
| Y / Bag footer and editable caption | `AppendBagHint`, `BagHintKey` | Native footer/owner validation, copied Y descriptor and resident byte, fitting-room INI read once, all 16 locale captions, English fallback and shared owned-caption validation/copy/notification. One visible slot and one descriptor spare are retained for native footer behavior. |

## Ordering, ownership and cancellation

- A single session is correlated with the native controller pointer/vtable and
  its begin/destroy lifecycle. Begin clears bag, plan, input edges and motions
  before native setup; matching destroy clears them before native destruction.
  No new generation/lifetime guarantee is attached to that controller pointer.
- Feature operation objects are synchronous stack borrows. Item/price/plan
  values are copied; UI pointers are retained only by the existing native
  session or callback scope. The diagnostic reporter is a synchronous,
  unretained feature observer, not an engine action or deferred registration.
- Opening while Y/B is already held does not create a press. Rejected held input
  must be released before acting later. Native input still runs once, and
  simultaneous buttons, consumed input, suppressed contexts, transitions and
  pending commands retain their original guards.
- Y removal precedes new-item validity/capacity rules. Successful changes start
  the feature's badge motion before requesting the supported native cursor tap;
  rejected changes do not create success feedback. The existing 16-motion ring
  is independent of the 256-item bag limit. The copied native EquipIn curve,
  per-UI-update timing, alpha and one-frame removal behavior are unchanged.
- Preflight validates both alternatives and outfit agreement, then rechecks each
  bag item's stock and price, appends only exact extras, and finally checks free
  space. Quotes can update the mod-owned bag even when later validation fails,
  as before. No game inventory/money writes occur during plan construction.
- Confirmation queues native work. Capacity, stock and funds are checked again
  at commit, after any intervening dialogue/cancellation or inventory changes.
  `active` suppresses reentrant checkout; `done` suppresses repeat payment even
  after the bag has been cleared. Empty-bag checkout retains original behavior.
- Frozen delivery values and their module-owned reference arrays live through
  the complete synchronous original checkout. Only the native list header is
  replaced in that active scope; native loops do not append/destroy the extended
  list. This is a verified fitting adapter, not a general native-list capability.
- Native checkout has no returned success flag. `NativeReturned` means that its
  validated original body returned, after which the existing cleanup clears the
  bag and dirty state. This refactor adds no rollback, reservation or guarantee
  against unsupported external mutations during that synchronous body.
- B's first action consumes B and calls native focus; confirmation forwards one
  temporary native A only when No is already focused. The original held bits,
  accompanying bit 16 and previous A-consumption state are restored immediately.
  No fabricated A survives into another page or later sample. Other dialogs,
  delegates, styles and invalid targets keep native behavior.
- Native row updates/counter retry, cursor hierarchy/animation, caption loading,
  typed UI setters and installation mechanics remain adapter responsibilities.
  Invalid-preflight errors remain mandatory diagnostics; moved source line
  numbers are replaced by a feature-validation label. Debug summaries are
  diagnostic, not transaction outcomes.
- Settings/defaults/version, translations, assets, user controls and bag-only
  behavior are unchanged. No release archive or installed emulator package is
  replaced. Shopping Card and Catalog behavior remain owned by their modules.

## Verification and preservation

- Fresh normal build passed using the pinned devkitA64 container; all diagnostic
  build flags are zero. The saved/root outputs match the final implementation.
- The full `QOL_BASELINE_ELF=dist/native-first-fitting/baseline/project.elf python3 tools/check.py`
  run passed **25 host suites and 55 ARM/resource scripts**, exit 0. The new
  production-registration check was then added and passed separately on the
  same outputs: **56 distinct ARM/resource scripts** validated in total.
  The hook-site comparison was rerun with the corrected `fitting-room` package
  slug, including fitting-only and combined activation; it passed.
- Fitting baseline replay passed all **six inherited suites**, comparing
  **21,925 tracked native calls and 8,114 observed outcomes** between current and
  accepted ELFs. Original assertions remain intact. Traces retain item values,
  payment/delivery/ticket/record/statistic arguments and call order; observations
  cover bag contents/prices, native commands, held/consumed input, feedback and
  captions. Temporary stack addresses and diagnostic log text are excluded.
- `fitting_registration_arm_test.py` boots Core and activates the real package,
  invokes installed lifecycle/input vtables, and reads the registered caption
  provider. It covers enabled/absent packages, fresh/held Y, exact-item addition,
  removal without new price/space queries, native capacity error, once-only
  original input, caption file I/O and matching destruction at two load bases.
  Engine/trampoline services are explicitly modeled; policy/provider state is
  not manually assigned after activation. Coverage commit: `750954ccd6f159dbf6e6386bc3b4d8887883623a`.
- Production activation passed **all 128 module subsets at two load addresses**.
  Shared baseline comparisons also passed: Dialogue 1,136; Crafting 93,804;
  Controls 2,156; Pockets 978 callbacks; Catalog 307,891 tracked native calls.
  Catalog fitting coverage includes 26,077 cases and 5,207 clothing IDs.
- Host policy tests link the actual feature `.cpp`, including the inherited
  372,075 capacity cases. New cases verify observation/action order, overlap,
  failure precedence, repeat/reentrant guards, post-return cleanup, lazy B focus,
  badge resets, footer limits and synchronous invalid-preflight diagnostics.
- Packaging/resource/config/manifest checks passed with `--revision 999999`
  without writing a release. A temporary source archive included the new
  policy/adapters/fingerprints and excluded the superseded mixed source, then
  was removed. All **3,252 pre-existing `dist` files** retained their hashes.
  `main` and `refactor/internal-feature-interfaces` remain unchanged.

Logs are `final-build.log`, `full.log`, `baseline.log`, `registration.log`,
`bindings.log` and `package.log` under `dist/native-first-fitting/`.
`candidate/SHA256.json` records final output/module hashes; `candidate/identity.json`
and `verification.json` record source/baseline/test identity and verification.
Earlier exploratory logs remain identifiable; an intermediate broad run was
stopped to restore preflight diagnostics before the final build/run.

The accepted executables and Fitting Room module were copied before edits to
`dist/native-first-fitting/baseline/`. `identity.json` records the starting commit,
branch, refs and output hashes. `preservation.json` records all 3,252 pre-existing
ignored `dist` files. Final normal outputs, module files, hashes and verification
metadata remain separately identifiable under `candidate/`.

Offline fixtures execute real local 3.0.3 checkout/input/price/animation code
with synthetic UI/save views and selected engine services. They validate native
calls, values, completion ordering and state; they do not establish live GPU
rendering, resource loading or arbitrary native lifetime safety. Eden validation
still needs to confirm loaded bag icons, pooled/reused rows, badges without cursor
movement, long localized captions, B focus-then-confirm, ticket/card behavior,
bag-only checkout and larger wear/send purchases. No live game was run.
