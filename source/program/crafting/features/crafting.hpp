#pragma once
#include "../game/quantity_operations.hpp"
namespace qol::crafting::features {
// Start reading Crafting at crafting_features.cpp. Ordinary implementation is
// out of line; these declarations have no native runtime dependency.
inline constexpr unsigned CraftingRampFrames = 60;
// Installation handles zero (native/manual speed) and bounds maximum to 1..30.
float CraftingRate(unsigned elapsedFrames, unsigned maximum);
struct QuantityHold { bool armed{}, wrapPaused{}; std::uint64_t pauseStarted{}; };
struct QuantityInput {
    float x{}, y{};
    bool buttonUp{}, buttonDown{}, confirmOrBack{}, unavailable{};
    bool verticalButtonConsumed{}, stickConsumed{}, cyclicLayoutReady{};
    unsigned selected{}, count{};
};
enum class QuantityMotion { Native, WrappedUp, WrappedDown };
void ResetQuantityHold(QuantityHold &);
void CancelQuantityHold(QuantityHold &);
QuantityMotion HandleQuantityEndpoints(QuantityHold &, const QuantityInput &, game::QuantityList &);
struct ShortcutHold { bool armed{}, favoriteArmed{}; };
bool HandleQuantityShortcut(ShortcutHold &, game::QuantityShortcut &);
}
// DIY preview input/lifecycle and workbench tilt are implemented separately.
#include "recipe_preview.hpp"
