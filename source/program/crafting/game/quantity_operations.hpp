#pragma once
#include <cstdint>
namespace qol::crafting::game {
// Callback-scoped native quantity/input operations. No object or input snapshot
// is retained. These interfaces are borrowed by feature handlers, never owned.
class QuantityList {
public:
    virtual std::uint64_t Tick() = 0;
    virtual void StepAcrossBoundary(bool up) = 0;
    virtual void NeutralizeVerticalInput() = 0;
protected:
    ~QuantityList() = default;
};
// Quantity is Y and Favorite is X on the verified large preview pages.
// Consumed queries expose the original dispatch mask, before our own writes.
class QuantityShortcut {
public:
    virtual bool InputReady() = 0;
    virtual bool QuantityHeld() = 0;
    virtual bool FavoriteHeld() = 0;
    virtual bool QuantityConsumed() = 0;
    virtual bool FavoriteConsumed() = 0;
    virtual bool ConfirmOrBackHeld() = 0;
    virtual bool HasResult() = 0;
    virtual bool FavoriteAvailable() = 0;
    virtual void RequestFavorite() = 0;
    virtual void ConsumeQuantity() = 0;
    virtual void ConsumeFavorite() = 0;
    virtual bool SupportsQuantity() = 0;
    virtual bool TransitionAvailable() = 0;
    virtual bool QuantityChildReady() = 0;
    virtual void RequestQuantitySelector() = 0;
    virtual void ReportQuantityOpened() = 0;
protected:
    ~QuantityShortcut() = default;
};
}
