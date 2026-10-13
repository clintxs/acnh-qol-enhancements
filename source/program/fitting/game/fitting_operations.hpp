#pragma once
#include "item.hpp"

namespace able_bag::game {
inline constexpr unsigned OutfitLimit = 10;
// Supported adapter limit for the extended native list, not an engine-wide
// capacity promise. Feature bag slots plus the ten native outfit deliveries.
inline constexpr unsigned DeliveryLimit = 266;
struct DeliveryAlternatives {
    unsigned sendCount{}, wearCount{};
    Item send[DeliveryLimit]{}, wear[DeliveryLimit]{};
};
struct Space { unsigned storage{}, pockets{}; };
// Synchronous borrows. Every operation reads the current native controller;
// collection copies values and preserves native send/old-clothing ordering.
// ReadOutfit writes at most OutfitLimit items to a caller-owned buffer. Successful
// delivery reads contain at most OutfitLimit native items per alternative; space
// reads validate the resident and native storage/pocket bounds.
class PurchaseSource {
public:
    virtual unsigned ReadOutfit(Item *items) = 0;
    virtual bool ReadDeliveryCounts(unsigned &send, unsigned &wear) = 0;
    virtual bool ReadDeliveries(DeliveryAlternatives &items) = 0;
    virtual bool HasStock(const Item &item) = 0;
    virtual int Quote(const Item &item) = 0;
    virtual bool ReadSpace(Space &space) = 0;
protected:
    ~PurchaseSource() = default;
};
class FittingSelection : public PurchaseSource {
public:
    virtual bool SelectedItem(Item &item) = 0;
    virtual void ShowSpaceError() = 0;
    virtual void PlayCursorTap() = 0;
    virtual void ConsumeToggle() = 0;
};
enum class Question { Other, Purchase, WearOrSend };
// These requests use the existing native question/delegate/state machinery.
// NativeCheckout is synchronous and returns no success/failure value; it owns
// payment, tickets, delivery, clothing, records and milestones. No rollback is promised.
class Checkout : public PurchaseSource {
public:
    virtual int TotalPrice() = 0;
    virtual unsigned AvailableBells() = 0;
    virtual void StoreTotal(int total) = 0;
    virtual void OpenConfirmation(int total, unsigned funds, bool affordable) = 0;
    virtual Question CurrentQuestion() = 0;
    virtual bool IdleCommand() = 0;
    virtual bool OutfitSlotsEmpty() = 0;
    virtual void SelectSendQuestion() = 0;
    virtual unsigned FundsError() = 0;
    virtual unsigned SpaceError() = 0;
    virtual unsigned ReturnToBrowsing() = 0;
    virtual unsigned NativeAnswer(unsigned answer) = 0;
    virtual void NativeCheckout(unsigned wear) = 0;
    virtual void CheckoutReturned() = 0;
};
enum class ChoiceFocus { Unavailable, Send, Wear };
class WearChoice {
public:
    virtual ChoiceFocus CurrentFocus() = 0;
    virtual void ConsumeBack() = 0;
    virtual void FocusSend() = 0;
protected:
    ~WearChoice() = default;
};
class Footer {
public:
    virtual int Count() const = 0;
    virtual int Capacity() const = 0;
    virtual bool Ready() const = 0;
    virtual bool Contains(const char *key) const = 0;
    virtual void AppendYHint(const char *key) = 0;
protected:
    ~Footer() = default;
};
} // namespace able_bag::game
