# Shopping Card: readable payment, proceeds and balance policy

This module continues `refactor/semantic-feature-api` from accepted Fitting Room
commit `24ba35dc76fe6af39667c75df7d3be563eefb517`. It stops at Shopping Card.

## Read in this order

1. [Feature implementation](../source/program/shopping/features/shopping_features.cpp):
   start at `AcceptsCard`, then `PlanPayment` and `PayForPurchase`. These show the
   supported contexts, affordability, savings-first allocation and actual ordered
   native actions. `RouteSaleToSavings`, `UseOrdinarySaleMessage`,
   `PurchaseSpaceBells`, `DisplayedBalance` and `FormatSavings` cover proceeds,
   capacity and presentation policy.
2. [Feature declarations](../source/program/shopping/features/shopping_features.hpp)
   and [money values/declarations](../source/program/shopping/payment_policy.hpp).
   The former header-only payment implementation is removed; ordinary behavior
   lives in `.cpp`.
3. [Supported payment operations](../source/program/shopping/game/card_payment.hpp):
   current funds and the three verified synchronous native actions. There is no
   independently writable bank/wallet/Bell-bag abstraction.
4. [Native adapter](../source/program/shopping/game/shopping_card_native.cpp):
   current field and exact caller validation, EventFlow correlation, original
   handlers, encrypted money access, native display/controller transitions,
   resource preparation, compatibility fingerprints and production installation.
   [Public integration](../source/program/shopping/shopping_card.hpp) retains
   `AvailableBells` for Fitting Room and the existing installation/resource API.

## A real before/after

Previously `shopping_card.cpp::PayWithCard` mixed raw resident access, native
addresses, payment planning and mutation. The feature now executes:

```cpp
const auto plan = native.ReadFunds(balance) ? PlanPayment(balance, price) : Payment{};
if (!plan.affordable) return plan;
if (plan.savings) native.WithdrawSavings(plan.savings);
if (plan.cashDebit) native.SpendNativeCash(plan.cashDebit);
if (plan.walletRefund) native.ReturnNativeChange(plan.walletRefund);
```

For 300,000 savings, 200,000 in bags, 99,000 wallet and a 500,000 price, it
withdraws 300,000, requests native cash debit of 299,000, then returns 99,000
through native cash addition. That consumes the bags while preserving the
wallet despite native cash debit being wallet-first. Native change/overflow
may repack cash. A one-Bell short combined total performs no mutation.

## Description-to-code map

| Player-facing behavior | Feature policy | Native authority retained |
| --- | --- | --- |
| Purchases in both Nook's Cranny shops and Able Sisters, including fitting checkout | `AcceptsCard(ShopPurchase)` | Exact reviewed BL return addresses and fresh native indoor field/type/room lookup. Shared cash helpers remain ordinary elsewhere. |
| Nook Shopping orders at Nook Stop or on the phone, including outdoors | `AcceptsCard(CatalogOrder)` | Exact catalog affordability/debit callers. Native order eligibility, confirmation, delivery, records and cancellation. |
| Tom Nook storage expansions, all six fees | `AcceptsCard(StorageUpgrade)`, `PayForPurchase` | Exact current EventFlow subflow, node bounds/counter/event identity, serialized command names and subtraction/nonnegative amount guards. Affordability resident and payment event are scoped to one synchronous original dispatch. Other fees retain original handling. |
| Combined affordability and savings → Bell bags → wallet | `ValidBalances`, `TotalBells`, `PlanPayment`, `PayForPurchase` | Native resident/money accessors, savings withdrawal, original cash debit, native cash addition/change and encrypted writes. Invalid reads/bounds and insufficient funds reject before mutations. |
| Fitting Room uses the same funds | Shared `TotalBells`/payment policy via public `AvailableBells` | Combined funds only while Shopping Card is installed and the current shop is recognized; ordinary wallet accessor otherwise. Existing fitting preflight/checkout integration is preserved. |
| Nook's counter sales go to savings | `RouteSaleToSavings` | Native Nook-market observation and exact sale-capacity caller; returning zero selects the original sale body's existing savings-deposit branch, including its native cap. No new direct sale deposit is introduced. |
| Ordinary localized sale success message | `UseOrdinarySaleMessage` | Only the native `cShop` money-capacity query in Nook's markets selects case zero. Other query contexts remain original. |
| Conservative delivery space and normal tailor tickets | `PurchaseSpaceBells` | Only two reviewed shop/fitting preflight callers omit Bell-spending credit. Original capacity implementation still owns empty slots, reservations and ticket accounting; change may reuse bag slots. |
| Combined top-right balance, nine-digit cap and separators | `TotalBells`, `DisplayedBalance`, `FormatSavings` | Native HUD/auxiliary controller updates, current resident, live bound pane and typed group lookup. Full uncapped combined funds remain available for purchases. |
| Correct opening/closing and one balance animation/sound | Existing native display adapter, `DisplayedBalance` | Catalog resident bits and storage presentation bits follow original open/close/destroy/Out transitions. HUD alone steps the native number value and feedback; auxiliary text updates immediately. Initial/switch/unchanged balances remain silent; hidden cash text stays current. No new deferred UI API is claimed. |
| Default and alternative card colors | Existing resource/packaging configuration | Unchanged layout/texture generators, packaged assets, native resource floor and texture lookup. Resource support remains necessary for installed-but-disabled behavior. |

## Scope, failure and cancellation

- `CardPayment` and its resident pointer are callback-local synchronous borrows.
  No request, interface, resident or save pointer escapes payment. Balances and
  plans are copied values; native guards precede construction/use.
- An affordable zero price reads funds but calls no money mutation. Failed reads,
  invalid bounds and insufficient combined funds invoke none of the three actions.
  Original native affordability/error dialogue is retained.
- Native mutation actions return void. An affordable returned plan means the
  planned calls returned, not atomic completion, successful item delivery or
  rollback. This refactor adds no reservation, idempotency or transaction framework.
  Original purchase bodies own the accepted transaction timing and cancellation.
- Ordinary/shared cash callbacks keep original handling. The adapter establishes
  reviewed contexts; the feature decides whether those contexts accept the card.
  It does not broaden caller ranges or cache a current room/player.
- Storage query and debit scopes restore previous values after native dispatch,
  including nested dispatch. Closing/destruction ends transaction correlation;
  presentation can finish its original Out without dereferencing a retained event.
- Display plumbing remains a native-adapter reading area: pane/number/controller
  state and animation stepping are engine-specific. Only balance selection/capping
  and number formatting were extracted. No general renderer/lifecycle capability
  is asserted by this module.

## Verification and preservation

The accepted ELF, NSO, NPDM and Shopping Card module were copied to
`dist/native-first-shopping/baseline` before edits/building. The preservation
manifest records all 3,277 pre-existing distribution files. Candidate copies,
hashes, identity and logs live in `dist/native-first-shopping`; these are ignored
local evidence, not a release or installed candidate. Main and the infrastructure
branch remain unchanged. Configurations, defaults, version, translations, card
colors and packaged resource behavior are unchanged.

Implementation/tests commit: `5d0d05f8d675b1b3ea7e94efe2b5fc7e4cd1f8dd`.

- Pinned normal Docker build passed; diagnostics remain disabled.
- All 25 host suites passed, including the original 5,040 payment combinations
  plus actual ordered native-operation, read-failure, context, zero-price and
  formatting assertions. Shopping host tests link the production feature `.cpp`.
- 21 distinct focused ARM/resource entries were exercised across Shopping Card,
  Core and Fitting Room. `tools/check.py core fitting` completed successfully with
  the accepted baseline. Shared infrastructure code was unchanged; unrelated
  module-specific suites were not rerun.
- Four inherited Shopping Card replays retained their assertions and matched
  stdout plus 144,764 tracked native calls: routing/payment 19,798, display
  42,348, native cash 28,486, storage 54,132. The new registration assertion first
  stopped the storage fixture at address zero; it was corrected to preserve
  that inherited direct-callback fixture. Storage then passed separately on
  both ELFs with byte-identical logs/digests. The earlier three comparisons
  remain valid; the source ELF was unchanged throughout.
- Real ASLR fixtures verify and use callbacks published by production activation;
  zero-address native-body fixtures remain explicitly separate. Production hook
  sites/readiness match for Shopping Card alone, with Fitting Room and with the
  previously migrated modules. All 128 module combinations pass at two bases.
- Fitting Room's six baseline replays match 21,925 native calls and 8,114 outcomes.
  Existing registration, native checkout, input, feedback and caption checks pass.
- Layout/resource/texture checks passed, including disabled marker behavior,
  shared Fitting Room allocation, native relocation/lookup and decoded pixels.
  Packaging/manifest checks and a temporary source archive inclusion/removal
  check passed. No release archive was replaced.
- All 3,277 older distribution files, accepted binaries and original branch refs
  were verified against the pre-edit manifest. Candidate files match the final
  root outputs. No game installation or live test was performed.

The initial Shopping check log and the corrected storage/resource/shared logs
are retained, with their relationship recorded in `verification.json`.

Live Eden rendering, sound, Able Sisters/catalog/storage interaction and scene
transitions remain playtest requirements. Synthetic save/UI services and native
ARM execution do not establish live rendering or transaction success guarantees.
