#include "recipe_preview.hpp"
#include "program/core/update_rate.hpp"
#include <algorithm>
namespace qol::crafting::preview {
int Session::Navigation::Step() {
    const int input = sample; sample = 0;
    if (!input) { held = 0; wait = 0; }
    else {
        const bool fresh = input != held;
        held = input;
        if (!fresh && wait) --wait;
        else { pending = input; wait = core::UpdateFrames(fresh ? 12 : 4); }
    }
    return pending;
}
Phase Session::State() const { return phase_; }
void Session::Open() { if (phase_ == Phase::Idle) phase_ = Phase::Queued; }
void Session::RequestClose() { if (phase_ != Phase::Idle && phase_ != Phase::DrainBack) phase_ = Phase::Closing; }
void Session::FinishClose() { ResetNavigation(); CancelRotationInput(); phase_ = Phase::DrainBack; }
void Session::ReleaseBack() { phase_ = Phase::Idle; }
void Session::Navigate(int direction) { if (phase_ == Phase::Showing) navigation_.sample = direction; }
void Session::Rotate(float x, float y) { if (phase_ == Phase::Showing) { rotationX_ = x; rotationY_ = y; } }
// Non-showing dispatch drops this frame's samples, but retains loading retries.
void Session::ClearFrameInput() { navigation_.sample = 0; CancelRotationInput(); }
// Suppression cancels queued work while retaining held-repeat history.
void Session::CancelPendingInput() { navigation_.sample = navigation_.pending = 0; CancelRotationInput(); }
void Session::ResetNavigation() { navigation_ = {}; }
void Session::CancelRotationInput() { rotationX_ = rotationY_ = 0; }
Session::RotationInput Session::TakeRotationInput() {
    const RotationInput input{rotationX_, rotationY_}; CancelRotationInput(); return input;
}
// Apply only after the native adapter validates rotation ownership/readiness.
float Session::TiltBy(float degrees) { tilt_ = std::clamp(tilt_ + degrees, -MaximumTiltDegrees, MaximumTiltDegrees); return tilt_; }
void Session::ResetItemRotation() { tilt_ = 0; CancelRotationInput(); }
void Session::Frame(game::PreviewRuntime &runtime) {
    runtime.RefreshHints();
    if (phase_ == Phase::Closing) { runtime.ReleaseRenderer(); FinishClose(); return; }
    if (phase_ == Phase::Queued) {
        if (runtime.AcquireRenderer()) { phase_ = Phase::Starting; frames_ = 0; }
        else { phase_ = Phase::Idle; runtime.OpenDeclined(); }
        return;
    }
    if (phase_ == Phase::Showing) {
        const auto direction = navigation_.Step();
        if (direction && runtime.Navigate(direction) != NavigationProgress::Retry) navigation_.pending = 0;
        runtime.PlaceBanner();
        runtime.Rotate();
        if (++frames_ == core::UpdateFrames(30)) runtime.ReportShowing();
    }
    if (phase_ != Phase::Starting) return;
    if (++frames_ > core::UpdateFrames(120)) { runtime.ReleaseRenderer(); FinishClose(); return; }
    const auto progress = runtime.CreateItemModel();
    if (progress == ModelProgress::Failed) { runtime.ReleaseRenderer(); FinishClose(); }
    else if (progress == ModelProgress::Created) {
        ResetItemRotation();
        runtime.ShowPreview();
        phase_ = Phase::Showing;
    }
}
}
namespace qol::crafting::features {
float PreviewStickAxis(float value) {
    constexpr float DeadZone = .15f;
    if (!(value >= -1.f && value <= 1.f)) return 0; // Reject nonfinite/corrupt samples.
    if (value > DeadZone) return (value - DeadZone) / (1.f - DeadZone);
    if (value < -DeadZone) return (value + DeadZone) / (1.f - DeadZone);
    return 0;
}
float TiltWorkbench(float previous, float stickY) { return std::clamp(previous - stickY * 6.f * core::UpdateScale(), -60.f, 60.f); }
bool ShowRecipePreviewHint(bool categoryFocus, unsigned count, unsigned selected, preview::Phase phase) {
    return !categoryFocus && count && count <= 4096 && selected < count &&
        (phase == preview::Phase::Idle || phase == preview::Phase::DrainBack);
}
bool QueueRecipePreview(preview::Session &session, game::RecipeOpening &recipes, std::uint64_t &copiedItem) {
    const auto count = recipes.Count(), selected = recipes.Selected();
    const bool row = recipes.FocusedRecipeRow();
    if (!recipes.PhoneReady() || !row || selected >= count || count > 4096 ||
        !recipes.HasRecipeList() || !recipes.HasRecipe(selected) || !recipes.HasValidItem(selected) ||
        !recipes.HasSurface() || !recipes.HasGrid()) {
        recipes.ReportDeclined(row, selected, count);
        return false;
    }
    copiedItem = recipes.CopyItem(selected); // Full item value, no retained native row.
    session.Open(); // Acquire/create occurs on a later native phone frame.
    recipes.ReportQueued(selected);
    return true;
}
bool HandleRecipePreviewInput(preview::Session &session, bool &confirmHeld, bool &backHeld, game::PreviewInput &input) {
    using preview::Phase;
    if (session.State() != Phase::Showing) session.ClearFrameInput();
    if (session.State() == Phase::Idle) input.RefreshInputRow();
    if (!input.ValidSnapshot()) { session.CancelPendingInput(); return session.State() != Phase::Idle; }
    const bool confirm = input.ConfirmHeld(), confirmPress = confirm && !confirmHeld;
    confirmHeld = confirm;
    if (session.State() == Phase::Idle) {
        if (!confirmPress) return false;
        input.ReportConfirm();
        if (input.PageBlocked() || input.ConfirmConsumed() || !input.QueueSelectedPreview()) return false;
        input.ConsumeConfirm();
        return true;
    }
    const bool back = input.BackHeld(), backPress = back && !backHeld;
    backHeld = back;
    if (session.State() == Phase::DrainBack) {
        if (!back) session.ReleaseBack(); // Drain B before returning input to the grid.
    } else if (backPress && !input.DispatchBlocked() && !input.BackConsumed()) {
        session.RequestClose();
        input.ConsumeBack();
    }
    if (session.State() == Phase::Showing) {
        const bool blocked = input.PageBlocked();
        if (blocked) session.CancelPendingInput();
        else {
            const bool left = input.LeftUnconsumed(), right = input.RightUnconsumed();
            if (left || right || !input.LeftStickConsumed()) {
                const auto stick = input.LeftStick();
                const bool horizontal = stick.x >= -1 && stick.x <= 1 && stick.y >= -1 && stick.y <= 1 &&
                    (stick.x > .5f || stick.x < -.5f) && stick.x*stick.x > stick.y*stick.y;
                session.Navigate(left || right ? int(right)-int(left) :
                    horizontal && !input.LeftStickConsumed() ? (stick.x > 0 ? 1 : -1) : 0);
            }
            input.ConsumeNavigation(); // D-pad has priority; opposite directions cancel.
        }
        const auto stick = input.RightStick();
        const auto x = PreviewStickAxis(stick.x), y = PreviewStickAxis(stick.y);
        input.ReportRotationSample(x, y, blocked);
        if (blocked) session.CancelRotationInput();
        else if (!input.RightStickConsumed()) {
            session.Rotate(x, y);
            input.ConsumeRotation();
        }
        // A repeated already-consumed dispatch preserves this frame's queued
        // rotation; the renderer-side operation drains it exactly once.
    }
    return true; // Freeze native grid/category/sort while the preview owns input.
}
preview::NavigationProgress NavigateRecipePreview(preview::Session &session, game::RecipeNavigation &recipes, int direction) {
    using preview::NavigationProgress;
    if (!direction) return NavigationProgress::End;
    if (!recipes.RendererReady()) return NavigationProgress::Retry;
    const auto count = recipes.Count(), current = recipes.Selected();
    if (!count || count > 4096 || current >= count || !recipes.HasRecipeList() ||
        (direction < 0 ? current == 0 : current + 1 == count)) return NavigationProgress::End;
    const auto selected = direction < 0 ? current - 1 : current + 1;
    if (!recipes.HasValidItem(selected)) return NavigationProgress::End; // No skipping or wraparound.
    const auto item = recipes.CopyItem(selected);
    recipes.RestoreBanner();
    if (!recipes.SelectRecipe(selected, count)) return NavigationProgress::Retry;
    recipes.SetItem(item);
    session.ResetItemRotation();
    recipes.ReplaceModel(selected, count); // Native replacement preserves the existing scene until ready.
    return NavigationProgress::Moved;
}
void RotateRecipePreview(preview::Session &session, game::PreviewRotation &renderer) {
    const auto [x,y] = session.TakeRotationInput();
    if (renderer.FixedPose() || (x == 0 && y == 0) || !renderer.RendererReady()) return;
    const auto tilt = session.TiltBy(-y * 6.f * core::UpdateScale());
    renderer.QueueRotation(x * 6.f * core::UpdateScale(), tilt);
}
}

namespace qol::crafting::features {
void DeactivateRecipePreview(preview::Session &session, bool &confirmHeld, bool &backHeld, game::PreviewRuntime &runtime) {
    if (session.State() != preview::Phase::Idle) runtime.ReleaseRenderer();
    session.ReleaseBack();
    backHeld = false;
    confirmHeld = true; // Reopening with A held cannot queue another preview.
}
}
