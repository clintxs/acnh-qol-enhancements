#pragma once
#include <algorithm>
namespace qol::crafting::preview {
enum class Phase { Idle, Queued, Starting, Showing, Closing, DrainBack };
enum class ModelProgress { Waiting, Created, Failed };
enum class NavigationProgress { Retry, Moved, End };
// UI-thread policy. Runtime owns native leases, layouts, model requests and
// restoration. Open is asynchronous: Queued -> Starting -> Showing or failure.
class Session {
    struct Navigation {
        int sample{}, held{}, pending{};
        unsigned wait{};
        int Step() {
            const int input = sample; sample = 0;
            if (!input) { held = 0; wait = 0; }
            else {
                const bool fresh = input != held;
                held = input;
                if (!fresh && wait) --wait;
                else { pending = input; wait = fresh ? 12 : 4; }
            }
            return pending;
        }
    };
    static constexpr float MaximumTiltDegrees = 60.0f;
    Phase phase_ = Phase::Idle;
    unsigned frames_{};
    Navigation navigation_;
    float rotationX_{}, rotationY_{}, tilt_{};
public:
    struct RotationInput { float x{}, y{}; };
    Phase State() const { return phase_; }
    void Open() { if (phase_ == Phase::Idle) phase_ = Phase::Queued; }
    void RequestClose() { if (phase_ != Phase::Idle && phase_ != Phase::DrainBack) phase_ = Phase::Closing; }
    void FinishClose() { ResetNavigation(); CancelRotationInput(); phase_ = Phase::DrainBack; }
    void ReleaseBack() { phase_ = Phase::Idle; }
    void Navigate(int direction) { if (phase_ == Phase::Showing) navigation_.sample = direction; }
    void Rotate(float x, float y) { if (phase_ == Phase::Showing) { rotationX_ = x; rotationY_ = y; } }
    // A non-showing dispatch drops this frame's samples, not a loading retry.
    void ClearFrameInput() { navigation_.sample = 0; CancelRotationInput(); }
    // Suppression/invalid snapshots cancel queued work but retain held-repeat
    // history. Only a frame with no navigation sample rearms that history.
    void CancelPendingInput() { navigation_.sample = navigation_.pending = 0; CancelRotationInput(); }
    void ResetNavigation() { navigation_ = {}; }
    void CancelRotationInput() { rotationX_ = rotationY_ = 0; }
    RotationInput TakeRotationInput() {
        const RotationInput input{rotationX_, rotationY_};
        CancelRotationInput();
        return input;
    }
    // Called only after the adapter verifies the renderer can accept rotation.
    float TiltBy(float degrees) { tilt_ = std::clamp(tilt_ + degrees, -MaximumTiltDegrees, MaximumTiltDegrees); return tilt_; }
    // Changing/creating an item resets tilt as well as unconsumed rotation;
    // closing merely drains input, retaining tilt until the next item setup.
    void ResetItemRotation() { tilt_ = 0; CancelRotationInput(); }
    template<class Runtime> void Frame(Runtime &runtime) {
        runtime.RefreshHints();
        if (phase_ == Phase::Closing) { runtime.Close(); FinishClose(); return; }
        if (phase_ == Phase::Queued) {
            if (runtime.Begin()) { phase_ = Phase::Starting; frames_ = 0; }
            else { phase_ = Phase::Idle; runtime.OpenDeclined(); }
            return;
        }
        if (phase_ == Phase::Showing) {
            const auto direction = navigation_.Step();
            if (direction && runtime.Navigate(direction) != NavigationProgress::Retry) navigation_.pending = 0;
            runtime.PlaceBanner();
            runtime.Rotate();
            if (++frames_ == 30) runtime.ReportShowing();
        }
        if (phase_ != Phase::Starting) return;
        if (++frames_ > 120) { runtime.Close(); FinishClose(); return; }
        const auto progress = runtime.CreateModel();
        if (progress == ModelProgress::Failed) { runtime.Close(); FinishClose(); }
        else if (progress == ModelProgress::Created) phase_ = Phase::Showing;
    }
};
}
