#include "fitting_features.hpp"

namespace able_bag {
void Bag::Clear() { count = 0; }
int Bag::Find(const Item &item) const {
    for (unsigned i = 0; i < count; ++i) {
        if (entries[i].item == item) {
            return i;
        }
    }
    return -1;
}
ToggleResult Bag::Toggle(Item item, int price, bool fits) {
    const int found = Find(item);
    if (found >= 0) {
        for (unsigned i = found + 1; i < count; ++i) entries[i - 1] = entries[i];
        --count;
        return ToggleResult::Removed;
    }
    if (item.id >= 0xfffe || price < 0 || price > 1000000) {
        return ToggleResult::Invalid;
    }
    if (!fits || count == Limit) {
        return ToggleResult::Full;
    }
    entries[count++] = {item, price};
    return ToggleResult::Added;
}
bool Bag::Contains(const Item *items, unsigned size, const Item &item) {
    for (unsigned i = 0; i < size; ++i) if (items[i] == item) return true;
    return false;
}
unsigned Bag::ExtraCount(const Item *outfit, unsigned size) const {
    unsigned result = 0;
    for (unsigned i = 0; i < count; ++i) result += !Contains(outfit, size, entries[i].item);
    return result;
}
int Bag::ExtraPrice(const Item *outfit, unsigned size) const {
    int result = 0;
    for (unsigned i = 0; i < count; ++i)
        if (!Contains(outfit, size, entries[i].item)) result += entries[i].price;
    return result;
}
bool Fits(unsigned storage, unsigned pockets, unsigned oldClothes,
          unsigned purchases, unsigned extras) {
    const unsigned base = oldClothes > purchases ? oldClothes : purchases;
    return std::uint64_t(base) + extras <= std::uint64_t(storage) + pockets;
}
bool PressEdge::Update(bool held) {
    const bool fresh = seen && held && !down;
    seen = true; down = held;
    return fresh;
}
void PressEdge::Reset() { seen = down = false; }

Preflight CanAddToBag(const Bag &bag, const Item &item, game::PurchaseSource &source) {
    Item outfit[game::OutfitLimit]{};
    const auto count = source.ReadOutfit(outfit);
    unsigned extras = bag.ExtraCount(outfit, count);
    if (!Bag::Contains(outfit, count, item)) ++extras;
    unsigned send{}, wear{};
    if (!source.ReadDeliveryCounts(send, wear)) {
        return Preflight::Invalid;
    }
    game::Space space;
    if (!source.ReadSpace(space)) {
        return Preflight::Invalid;
    }
    return Fits(space.storage, space.pockets, wear, send, extras) ? Preflight::Ready : Preflight::NoSpace;
}
Preflight PrepareBagPurchase(Bag &bag, PurchasePlan &plan, game::PurchaseSource &source,
                             InvalidPlanReporter reportInvalid) {
    const auto invalid = [reportInvalid] {
        if (reportInvalid) reportInvalid();
        return Preflight::Invalid;
    };
    if (plan.active || plan.done) {
        return invalid();
    }
    plan = {};
    if (!source.ReadDeliveries(plan)) {
        return invalid();
    }
    Item outfit[game::OutfitLimit]{};
    const auto count = source.ReadOutfit(outfit);
    // Native purchase collection must agree with persistent outfit membership.
    if (count != plan.sendCount) {
        return invalid();
    }
    for (unsigned i = 0; i < count; ++i)
        if (!Bag::Contains(plan.send, plan.sendCount, outfit[i])) {
            return invalid();
        }
    for (unsigned i = 0; i < bag.count; ++i) {
        auto &entry = bag.entries[i];
        if (!source.HasStock(entry.item)) {
            return invalid();
        }
        const int quote = source.Quote(entry.item);
        if (quote < 0 || quote > 1000000) {
            return invalid();
        }
        entry.price = quote;
        if (Bag::Contains(outfit, count, entry.item)) continue;
        if (plan.sendCount == game::DeliveryLimit || plan.wearCount == game::DeliveryLimit) {
            return invalid();
        }
        plan.send[plan.sendCount++] = entry.item;
        plan.wear[plan.wearCount++] = entry.item;
        plan.extraPrice += quote;
    }
    game::Space space;
    if (!source.ReadSpace(space)) {
        return invalid();
    }
    const auto reserve = plan.sendCount > plan.wearCount ? plan.sendCount : plan.wearCount;
    return reserve <= space.storage + space.pockets ? Preflight::Ready : Preflight::NoSpace;
}
bool BagInputEligible(bool fresh, bool allowed, bool consumed, bool otherButtons, bool browsing) {
    return fresh && allowed && !consumed && !otherButtons && browsing;
}
ToggleOutcome ToggleBagSelection(Bag &bag, BadgeMotions &motions, unsigned &nextMotion,
                                 game::FittingSelection &selection) {
    ToggleOutcome outcome;
    if (!selection.SelectedItem(outcome.item)) {
        return outcome;
    }
    outcome.handled = true;
    const bool removing = bag.Find(outcome.item) >= 0;
    outcome.price = removing ? 0 : selection.Quote(outcome.item);
    const auto capacity = removing ? Preflight::Ready : CanAddToBag(bag, outcome.item, selection);
    const bool limit = !removing && bag.count == Bag::Limit;
    outcome.result = capacity == Preflight::Invalid || limit ? ToggleResult::Invalid
        : bag.Toggle(outcome.item, outcome.price, capacity == Preflight::Ready);
    if (capacity == Preflight::NoSpace && !limit) {
        selection.ShowSpaceError();
    }
    if (outcome.result == ToggleResult::Added || outcome.result == ToggleResult::Removed) {
        StartBagBadgeMotion(motions, nextMotion, outcome.item, outcome.result == ToggleResult::Added);
        selection.PlayCursorTap();
    }
    selection.ConsumeToggle();
    return outcome;
}
bool OpenBagConfirmation(Bag &bag, PurchasePlan &plan, game::Checkout &checkout,
                         bool purchaseRequested, InvalidPlanReporter reportInvalid) {
    if (!bag.count || !purchaseRequested) {
        return false;
    }
    // As before, the initial prompt opens even if preflight is not ready.
    // The answer and commit paths independently revalidate before native payment.
    PrepareBagPurchase(bag, plan, checkout, reportInvalid);
    const int total = checkout.TotalPrice();
    const auto funds = checkout.AvailableBells();
    checkout.StoreTotal(total);
    checkout.OpenConfirmation(total, funds, total >= 0 && unsigned(total) <= funds);
    return true;
}
unsigned AnswerBagConfirmation(Bag &bag, PurchasePlan &plan, game::Checkout &checkout,
                               unsigned answer, InvalidPlanReporter reportInvalid) {
    if (!bag.count) {
        return checkout.NativeAnswer(answer);
    }
    // Bag-only checkout has nothing new to wear. Exact empty stock sentinels,
    // rather than failed item resolution, establish that the outfit is empty.
    if (checkout.CurrentQuestion() == game::Question::Purchase && answer == 1 &&
        checkout.IdleCommand() && checkout.OutfitSlotsEmpty()) {
        checkout.SelectSendQuestion();
        answer = 0;
    }
    if (checkout.CurrentQuestion() == game::Question::WearOrSend) {
        const auto result = PrepareBagPurchase(bag, plan, checkout, reportInvalid);
        const int total = checkout.TotalPrice();
        checkout.StoreTotal(total);
        if (total < 0 || unsigned(total) > checkout.AvailableBells()) {
            return checkout.FundsError();
        }
        if (result == Preflight::Invalid) {
            return checkout.ReturnToBrowsing();
        }
        if (result == Preflight::NoSpace) {
            return checkout.SpaceError();
        }
    }
    return checkout.NativeAnswer(answer);
}
int PriceWithBag(int native, const Bag &bag, const Item *outfit, unsigned count) {
    return native + bag.ExtraPrice(outfit, count);
}
int GrossPriceWithBag(int native, const PurchasePlan &plan, bool ownerPrice) {
    return plan.active && ownerPrice ? native + plan.extraPrice : native;
}
CheckoutStep CommitBagCheckout(Bag &bag, PurchasePlan &plan, game::Checkout &checkout, unsigned wear,
                               InvalidPlanReporter reportInvalid) {
    if (plan.active || plan.done) return CheckoutStep::Skipped;
    if (!bag.count) {
        checkout.NativeCheckout(wear);
        return CheckoutStep::NativePassthrough;
    }
    const auto result = PrepareBagPurchase(bag, plan, checkout, reportInvalid);
    const int total = checkout.TotalPrice();
    if (result != Preflight::Ready || total < 0 || unsigned(total) > checkout.AvailableBells()) {
        return CheckoutStep::Rejected;
    }
    checkout.StoreTotal(total);
    plan.active = true;
    checkout.NativeCheckout(wear); // Exactly once; original engine delivery/payment loops.
    plan.active = false;
    plan.done = true;
    bag.Clear();
    checkout.CheckoutReturned();
    return CheckoutStep::NativeReturned;
}
BackAction HandleBackShortcut(bool fresh, bool allowed, bool consumed, bool exclusive,
                              game::WearChoice &choice) {
    if (!fresh || !allowed || consumed || !exclusive) {
        return BackAction::Ignore;
    }
    const auto focus = choice.CurrentFocus();
    if (focus == game::ChoiceFocus::Unavailable) {
        return BackAction::Ignore;
    }
    choice.ConsumeBack();
    if (focus == game::ChoiceFocus::Send) {
        return BackAction::ConfirmSend;
    }
    choice.FocusSend();
    return BackAction::FocusSend;
}
namespace {
float BadgeScale(unsigned frame) {
    // Preserve native EquipIn BFLAN Hermite keys (frame,value,slope).
    constexpr float keys[][3]{{0,.5f,.26666667f},{3,1.4f,0},{6,.9f,0},{7,1,0}};
    for (unsigned i = 0; i < 3; ++i) if (float(frame) <= keys[i+1][0]) {
        const float span = keys[i+1][0]-keys[i][0], t = (float(frame)-keys[i][0])/span;
        const float t2=t*t, t3=t2*t;
        return (2*t3-3*t2+1)*keys[i][1]+(t3-2*t2+t)*span*keys[i][2]
             +(-2*t3+3*t2)*keys[i+1][1]+(t3-t2)*span*keys[i+1][2];
    }
    return 1;
}
} // namespace
void StartBagBadgeMotion(BadgeMotions &motions, unsigned &next, const Item &item, bool added) {
    for (auto &motion : motions) if (motion.active && motion.item == item) {
        motion = {item, 0, added, true}; return;
    }
    motions[next++ % motions.size()] = {item, 0, added, true};
}
bool HasBagBadgeMotion(const BadgeMotions &motions) {
    for (const auto &motion : motions) if (motion.active) return true;
    return false;
}
void AdvanceBagBadgeMotions(BadgeMotions &motions) {
    for (auto &motion : motions)
        if (motion.active && ++motion.frame > (motion.added ? 7u : 1u)) motion.active = false;
}
BadgeVisual BagBadgeVisual(const Bag &bag, const BadgeMotions &motions, const Item *item) {
    BadgeVisual visual;
    visual.shown = item && bag.Find(*item) >= 0;
    for (const auto &motion : motions) if (motion.active && item && motion.item == *item) {
        if (motion.added) {
            visual.scale = BadgeScale(motion.frame);
            visual.alpha = motion.frame ? 255 : 0;
        } else {
            visual.shown = motion.frame == 0;
        }
        break;
    }
    return visual;
}
void AppendBagHint(game::Footer &footer) {
    const int count = footer.Count(), capacity = footer.Capacity();
    // Leave one visible slot for the native ticket hint, and retain its spare
    // descriptor slot. Duplicate appends leave the existing native footer alone.
    if (count < 0 || count >= 4 || count + 1 >= capacity || !footer.Ready()) {
        return;
    }
    if (footer.Contains(BagHintKey)) return;
    footer.AppendYHint(BagHintKey);
}
} // namespace able_bag
