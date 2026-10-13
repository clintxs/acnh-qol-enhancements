#pragma once
// Start here: Controls decisions used directly by the native callbacks.
#include <string_view>

namespace qol::controls::features {
// A hold locks only after the initial native reservation succeeds. State is
// resident/handle correlated by the adapter; none of these decisions own items.
enum class PickupCategory { None, Item, Crop, Weed, Moss };
inline void ObservePickupHold(PickupCategory &locked, bool &firstPress,
                              bool identityChanged, bool heldY, bool pressedY) {
    if (identityChanged || !heldY || pressedY) {
        locked = PickupCategory::None;
        firstPress = heldY && pressedY;
    } else firstPress = false;
}
inline bool RepeatPickup(PickupCategory locked, bool firstPress, bool freshPickupPress) {
    return !firstPress && locked != PickupCategory::None && !freshPickupPress;
}
inline bool RepeatMatches(PickupCategory locked, PickupCategory candidate) {
    return candidate != PickupCategory::None && candidate == locked;
}
inline bool LatchPickup(bool nativeSucceeded, bool firstPress, bool repeating) {
    return nativeSucceeded && firstPress && !repeating;
}
// The probe and commit can choose different targets. The reservation hook
// applies RepeatMatches again BEFORE native inventory/reservation work.
template<class Picker>
unsigned PickUpWhileHeld(Picker &picker, bool repeating) {
    if (repeating) {
        if (picker.ProbeMatchesLockedCategory()) picker.RequestGroundPickup();
        // Consume even a failed repeat; never fall through to furniture/error
        // dialogs, ground-design erasure, or another native Y action.
        return 1;
    }
    const auto result = picker.RequestInitialPickup();
    picker.FinishInitialAttempt(); // An unsuccessful first attempt cannot latch later.
    return result;
}

// Context operations are lazy, synchronous borrows. Each ownership check must
// pass before inspecting the next native object. Null means eligible; reasons
// also feed the existing bounded diagnostic build.
template<class Context>
const char *DialogueHurryBlock(Context &context) {
    if (!context.HasOwners()) return "missing-owner";
    if (!context.ManagerOwnsText()) return "manager-text";
    if (!context.TextOwnsManager()) return "text-manager";
    if (!context.InRequestedPhase()) return "manager-state";
    if (!context.HasContext()) return "missing-context";
    if (!context.TextSharesContext()) return "text-context";
    if (!context.ContextReady()) return "context-phase";
    if (!context.SupportedWindow()) return "window";
    if (!context.HasChoices()) return "missing-choices";
    return context.ChoicesInactive() ? nullptr : "choice-active";
}
inline bool IsNewCatchAnnouncement(std::string_view bank, std::string_view label) {
    return label == "001" && (bank == "SYS_Get_Fish" || bank == "SYS_Get_Insect" || bank == "SYS_Get_DiveFish");
}
template<class Context>
const char *DialogueAdvanceBlock(Context &context) {
    if (const auto block = DialogueHurryBlock(context)) return block;
    if (!context.AtPageEndOrInputPause()) return "text-not-ready";
    if (!context.WaitingForManualAdvance()) return "scripted-timer";
    if (!context.HasMessage()) return "missing-message";
    if (!context.MessageReady()) return "message-transition";
    // New catches still reveal quickly, but their first announcement requires
    // manual input. Choices and conversation end stay under native control.
    return context.NewCatchAnnouncement() ? "new-catch" : nullptr;
}
// An eligible Y request selects the existing fixed nominal 30x tier; the
// adapter encodes that tier for shared reveal and supplies only local advance.
inline bool HurryRequested(const char *block, bool heldY) { return !block && heldY; }

template<class Camera>
bool AllowSwimmingCameraTilt(Camera &camera) {
    if (camera.NativeTiltAllowed()) return true;
    // On the verified profile the only other rejection is surface stride.
    // Global lock still wins; native right-stick/tilt-position handling stays.
    return !camera.GloballyLocked();
}
}

// Traversal and swimming have longer stateful policies in these two files.
#include "traversal.hpp"
#include "swimming.hpp"
