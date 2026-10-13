#pragma once
#include "bag_policy.hpp"
#include "program/fitting/game/fitting_operations.hpp"
namespace able_bag {
static_assert(Bag::Limit + game::OutfitLimit == game::DeliveryLimit);
enum class Preflight { Invalid, Ready, NoSpace };
struct PurchasePlan : game::DeliveryAlternatives {
    bool active{}, done{};
    int extraPrice{};
};
struct BadgeMotion { Item item{}; unsigned frame{}; bool added{}, active{}; };
using BadgeMotions = std::array<BadgeMotion, 16>;
using InvalidPlanReporter = void (*)();
struct ToggleOutcome { bool handled{}; Item item{}; int price{}; ToggleResult result = ToggleResult::Invalid; };
Preflight CanAddToBag(const Bag &, const Item &, game::PurchaseSource &);
Preflight PrepareBagPurchase(Bag &, PurchasePlan &, game::PurchaseSource &,
                             InvalidPlanReporter reportInvalid = nullptr);
bool BagInputEligible(bool fresh, bool allowed, bool consumed, bool otherButtons, bool browsing);
ToggleOutcome ToggleBagSelection(Bag &, BadgeMotions &, unsigned &nextMotion, game::FittingSelection &);
bool OpenBagConfirmation(Bag &, PurchasePlan &, game::Checkout &, bool purchaseRequested,
                         InvalidPlanReporter reportInvalid = nullptr);
unsigned AnswerBagConfirmation(Bag &, PurchasePlan &, game::Checkout &, unsigned answer,
                               InvalidPlanReporter reportInvalid = nullptr);
int PriceWithBag(int native, const Bag &, const Item *outfit, unsigned count);
int GrossPriceWithBag(int native, const PurchasePlan &, bool ownerPrice);
enum class CheckoutStep { Skipped, NativePassthrough, Rejected, NativeReturned };
CheckoutStep CommitBagCheckout(Bag &, PurchasePlan &, game::Checkout &, unsigned wear,
                               InvalidPlanReporter reportInvalid = nullptr);
enum class BackAction { Ignore, FocusSend, ConfirmSend };
BackAction HandleBackShortcut(bool fresh, bool allowed, bool consumed, bool exclusive,
                              game::WearChoice &);
struct BadgeVisual { bool shown{}; float scale = 1; unsigned alpha = 255; };
void StartBagBadgeMotion(BadgeMotions &, unsigned &next, const Item &, bool added);
bool HasBagBadgeMotion(const BadgeMotions &);
void AdvanceBagBadgeMotions(BadgeMotions &);
BadgeVisual BagBadgeVisual(const Bag &, const BadgeMotions &, const Item *);
inline constexpr char BagHintKey[] = "fitting_room.add_to_bag";
void AppendBagHint(game::Footer &);
} // namespace able_bag
