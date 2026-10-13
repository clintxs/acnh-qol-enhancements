#pragma once
namespace qol::crafting::preview {
enum class Phase { Idle, Queued, Starting, Showing, Closing, DrainBack };
enum class ModelProgress { Waiting, Created, Failed };
enum class NavigationProgress { Retry, Moved, End };
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
struct Rotation { float x{}, y{}, tilt{}; };
// UI-thread policy. Runtime owns native leases, layouts, model requests and
// restoration. Open is asynchronous: Queued -> Starting -> Showing or failure.
class Session {
    Phase phase_ = Phase::Idle;
    unsigned frames_{};
public:
    Navigation navigation;
    Rotation rotation;
    Phase State() const { return phase_; }
    void Open() { if (phase_ == Phase::Idle) phase_ = Phase::Queued; }
    void RequestClose() { if (phase_ != Phase::Idle && phase_ != Phase::DrainBack) phase_ = Phase::Closing; }
    void FinishClose() { ClearInput(); phase_ = Phase::DrainBack; }
    void ReleaseBack() { phase_ = Phase::Idle; }
    void Navigate(int direction) { if (phase_ == Phase::Showing) navigation.sample = direction; }
    void Rotate(float x, float y) { if (phase_ == Phase::Showing) { rotation.x = x; rotation.y = y; } }
    void ClearInput() { navigation = {}; rotation.x = rotation.y = 0; }
    template<class Runtime> void Frame(Runtime &runtime) {
        runtime.RefreshHints();
        if (phase_ == Phase::Closing) { runtime.Close(); FinishClose(); return; }
        if (phase_ == Phase::Queued) {
            if (runtime.Begin()) { phase_ = Phase::Starting; frames_ = 0; }
            else { phase_ = Phase::Idle; runtime.OpenDeclined(); }
            return;
        }
        if (phase_ == Phase::Showing) {
            const auto direction = navigation.Step();
            if (direction && runtime.Navigate(direction) != NavigationProgress::Retry) navigation.pending = 0;
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
