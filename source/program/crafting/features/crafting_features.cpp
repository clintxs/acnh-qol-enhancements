#include "crafting.hpp"
#include <algorithm>
namespace qol::crafting::features {
float CraftingRate(unsigned elapsedFrames, unsigned maximum) {
    // Sample 1 + 2 * log(1 + 9*t) / log(10), t = frame/60. A small constant
    // table avoids a runtime math-library dependency in the injected module.
    // Starts at 1x, rises quickly, then eases into the native maximum of 3x.
    static constexpr float rates[CraftingRampFrames + 1]{
        1.000000000f, 1.121395681f, 1.227886705f, 1.322736004f, 1.408239965f,
        1.486076097f, 1.557507202f, 1.623507722f, 1.684845362f, 1.742135725f,
        1.795880017f, 1.846491748f, 1.894316063f, 1.939644032f, 1.982723388f,
        2.023766722f, 2.062957834f, 2.100456706f, 2.136403448f, 2.170921459f,
        2.204119983f, 2.236096193f, 2.266936911f, 2.296720022f, 2.325515663f,
        2.353387219f, 2.380392160f, 2.406582756f, 2.432006687f, 2.456707564f,
        2.480725379f, 2.504096896f, 2.526855987f, 2.549033931f, 2.570659670f,
        2.591760035f, 2.612359948f, 2.632482600f, 2.652149605f, 2.671381143f,
        2.690196080f, 2.708612084f, 2.726645720f, 2.744312545f, 2.761627185f,
        2.778603405f, 2.795254183f, 2.811591761f, 2.827627705f, 2.843372951f,
        2.858837851f, 2.874032215f, 2.888965344f, 2.903646071f, 2.918082785f,
        2.932283465f, 2.946255707f, 2.960006743f, 2.973543469f, 2.986872461f,
        3.000000000f,
    };
    const auto nativeRate = rates[elapsedFrames < CraftingRampFrames ? elapsedFrames : CraftingRampFrames];
    // Preserve the confirmed default curve exactly. Other settings scale its
    // gain while retaining the same timing and per-action restart behavior.
    return maximum == 3 ? nativeRate : 1.0f + (nativeRate - 1.0f) * (maximum - 1) / 2.0f;
}

void ResetQuantityHold(QuantityHold &hold) { hold = {}; }
void CancelQuantityHold(QuantityHold &hold) { hold.armed = hold.wrapPaused = false; }
QuantityMotion HandleQuantityEndpoints(QuantityHold &state, const QuantityInput &input, game::QuantityList &list) {
    constexpr float PressThreshold = .5f, ReleaseThreshold = .15f;
    constexpr std::uint64_t WrapPauseTicks = 19'200'000 * 3 / 10; // 300 ms, not UI frames.
    const auto x = input.x, y = input.y;
    const bool validStick = x >= -1 && x <= 1 && y >= -1 && y <= 1;
    const bool stickUp = validStick && y > PressThreshold;
    const bool stickDown = validStick && y < -PressThreshold;
    const bool up = input.buttonUp || (stickUp && y*y > x*x);
    const bool down = input.buttonDown || (stickDown && y*y > x*x);
    const bool fresh = state.armed;
    // Centering rearms; easing below the press threshold or moving diagonally
    // cannot turn a held direction into a fresh endpoint press.
    const bool released = !input.unavailable && validStick && !input.buttonUp && !input.buttonDown &&
                          x*x + y*y <= ReleaseThreshold*ReleaseThreshold;
    if (released) { state.armed = true; state.wrapPaused = false; }
    else if (input.unavailable || !validStick || up || down ||
             x > PressThreshold || x < -PressThreshold || input.confirmOrBack) state.armed = false;
    if (state.wrapPaused) {
        if (list.Tick() - state.pauseStarted < WrapPauseTicks) list.NeutralizeVerticalInput();
        else state.wrapPaused = false;
        return QuantityMotion::Native; // Native repeat resumes; wrap stays disarmed.
    }
    if (up && down) { list.NeutralizeVerticalInput(); return QuantityMotion::Native; }
    if (input.selected >= input.count || up == down ||
        (up ? input.selected != 0 : input.selected != input.count-1)) return QuantityMotion::Native;
    if (!fresh || input.unavailable || !validStick || input.confirmOrBack ||
        input.verticalButtonConsumed || ((stickUp || stickDown) && input.stickConsumed) ||
        !input.cyclicLayoutReady) {
        list.NeutralizeVerticalInput();
        return QuantityMotion::Native;
    }
    list.StepAcrossBoundary(up); // Original native cyclic select/bind/slide/sound.
    state.armed = false;
    state.wrapPaused = true;
    state.pauseStarted = list.Tick(); // Sample after movement, as in the native adapter baseline.
    list.NeutralizeVerticalInput();
    return up ? QuantityMotion::WrappedUp : QuantityMotion::WrappedDown;
}
bool HandleQuantityShortcut(ShortcutHold &hold, game::QuantityShortcut &shortcut) {
    if (!shortcut.InputReady()) { hold.armed = hold.favoriteArmed = false; return false; }
    const bool fresh = hold.armed, freshFavorite = hold.favoriteArmed;
    hold.armed = !shortcut.QuantityHeld();
    hold.favoriteArmed = !shortcut.FavoriteHeld();
    // Every Y sample belongs to quantity, even a held/ineligible press; it
    // must never fall through to the old native Favorite binding.
    shortcut.ConsumeQuantity();
    if (shortcut.ConfirmOrBackHeld() || shortcut.HasResult()) return false;
    if (freshFavorite && shortcut.FavoriteHeld() && !shortcut.QuantityHeld() &&
        !shortcut.FavoriteConsumed() && shortcut.FavoriteAvailable()) {
        shortcut.RequestFavorite(); // Native queued Favorite action; no direct toggle.
        shortcut.ConsumeFavorite();
    }
    if (!fresh || !shortcut.QuantityHeld() || shortcut.QuantityConsumed() || !shortcut.SupportsQuantity()) return false;
    if (!shortcut.TransitionAvailable() || !shortcut.QuantityChildReady()) return false;
    shortcut.RequestQuantitySelector(); // Native MultiCreation enforces recipe/material limits.
    shortcut.ConsumeQuantity();
    shortcut.ReportQuantityOpened();
    return true;
}
}
