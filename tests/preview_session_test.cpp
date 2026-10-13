#include "program/crafting/features/recipe_preview.hpp"
#include <cassert>
#include <iostream>
using namespace qol::crafting::preview;
struct Runtime : qol::crafting::game::PreviewRuntime {
    bool available = true;
    ModelProgress model = ModelProgress::Waiting;
    NavigationProgress navigation = NavigationProgress::Retry;
    unsigned shows{}, begins{}, creates{}, closes{}, declined{}, moves{}, banners{}, rotations{}, reports{}, refreshes{};
    void RefreshHints() override { ++refreshes; }
    bool AcquireRenderer() override { ++begins; return available; }
    void OpenDeclined() override { ++declined; }
    void ReleaseRenderer() override { ++closes; }
    ModelProgress CreateItemModel() override { ++creates; return model; }
    void ShowPreview() override { ++shows; }
    NavigationProgress Navigate(int direction) override { assert(direction == -1 || direction == 1); ++moves; return navigation; }
    void PlaceBanner() override { ++banners; }
    void Rotate() override { ++rotations; }
    void ReportShowing() override { ++reports; }
};
int main() {
    Session s; Runtime runtime;
    s.Rotate(1,1); s.Navigate(1);
    assert(s.TakeRotationInput().x == 0);
    s.Open(); s.Open(); s.Frame(runtime);
    assert(s.State() == Phase::Starting && runtime.begins == 1 && !runtime.creates);
    for (unsigned frame = 0; frame < 120; ++frame) s.Frame(runtime);
    assert(s.State() == Phase::Starting && runtime.creates == 120 && !runtime.closes);
    s.Frame(runtime);
    assert(s.State() == Phase::DrainBack && runtime.closes == 1 && runtime.creates == 120);
    s.Open(); assert(s.State() == Phase::DrainBack);
    s.ReleaseBack(); s.Open(); runtime.available = false; s.Frame(runtime);
    assert(s.State() == Phase::Idle && runtime.declined == 1);
    runtime.available = true; runtime.model = ModelProgress::Failed;
    s.Open(); s.Frame(runtime); s.Frame(runtime);
    assert(s.State() == Phase::DrainBack && runtime.closes == 2);
    s.ReleaseBack(); runtime.model = ModelProgress::Created;
    s.Open(); s.Frame(runtime); s.Frame(runtime);
    assert(s.State() == Phase::Showing && runtime.shows == 1);
    s.Navigate(1); s.Rotate(1,-1); s.Frame(runtime);
    assert(runtime.moves == 1 && runtime.banners == 1 && runtime.rotations == 1);
    // One pending navigation survives loading/retry; it cannot grow a backlog.
    s.Frame(runtime); assert(runtime.moves == 2);
    runtime.navigation = NavigationProgress::Moved;
    s.Frame(runtime); assert(runtime.moves == 3);
    for (unsigned frame = 0; frame < 26; ++frame) s.Frame(runtime);
    assert(runtime.reports == 1 && runtime.moves == 3);
    s.RequestClose(); s.Frame(runtime);
    assert(s.State() == Phase::DrainBack && runtime.closes == 3 && s.TakeRotationInput().x == 0);
    for (unsigned cycle = 0; cycle < 20; ++cycle) {
        s.ReleaseBack(); s.Open(); s.Frame(runtime); s.Frame(runtime);
        assert(s.State() == Phase::Showing);
        s.RequestClose(); s.Frame(runtime); assert(s.State() == Phase::DrainBack);
    }
    // Exercise private navigation through the production session operations.
    s.ReleaseBack(); s.Open(); s.Frame(runtime); s.Frame(runtime);
    runtime.navigation = NavigationProgress::Moved;
    auto moves = runtime.moves;
    s.Navigate(1); s.Frame(runtime); assert(runtime.moves == ++moves);
    for (unsigned frame = 0; frame < 12; ++frame) {
        s.Navigate(1); s.Frame(runtime); assert(runtime.moves == moves);
    }
    s.Navigate(1); s.Frame(runtime); assert(runtime.moves == ++moves);
    for (unsigned frame = 0; frame < 4; ++frame) {
        s.Navigate(1); s.Frame(runtime); assert(runtime.moves == moves);
    }
    s.Navigate(1); s.Frame(runtime); assert(runtime.moves == ++moves);
    // Cancellation drops a loading retry but preserves held-repeat timing.
    runtime.navigation = NavigationProgress::Retry;
    s.Navigate(-1); s.Frame(runtime); assert(runtime.moves == ++moves);
    s.CancelPendingInput(); s.Navigate(-1); s.Frame(runtime);
    assert(runtime.moves == moves);
    for (unsigned frame = 0; frame < 11; ++frame) {
        s.Navigate(-1); s.Frame(runtime); assert(runtime.moves == moves);
    }
    s.Navigate(-1); s.Frame(runtime); assert(runtime.moves == ++moves);
    // Without a fresh held sample, cancellation rearms on the next frame.
    s.CancelPendingInput(); s.Frame(runtime); assert(runtime.moves == moves);
    s.Navigate(-1); s.Frame(runtime); assert(runtime.moves == ++moves);
    s.ResetNavigation(); s.Frame(runtime); assert(runtime.moves == moves);
    s.Navigate(-1); s.Frame(runtime); assert(runtime.moves == ++moves);
    // Clearing only this frame's sample retains the pending loading retry.
    s.ClearFrameInput(); s.Frame(runtime); assert(runtime.moves == ++moves);
    s.CancelPendingInput(); s.Frame(runtime); assert(runtime.moves == moves);
    s.Rotate(0.5f, -1); auto rotation = s.TakeRotationInput();
    assert(rotation.x == 0.5f && rotation.y == -1);
    rotation = s.TakeRotationInput(); assert(rotation.x == 0 && rotation.y == 0);
    assert(s.TiltBy(6) == 6 && s.TiltBy(100) == 60 && s.TiltBy(-200) == -60);
    s.Rotate(1, 1); s.CancelRotationInput();
    assert(s.TakeRotationInput().x == 0 && s.TiltBy(0) == -60);
    s.Rotate(1, 1); s.ResetItemRotation();
    assert(s.TakeRotationInput().x == 0 && s.TiltBy(0) == 0);
    s.TiltBy(12); s.Rotate(1,1); s.Navigate(1);
    s.RequestClose(); s.Frame(runtime);
    assert(s.TakeRotationInput().x == 0 && s.TiltBy(0) == 12);
    s.ReleaseBack(); s.Open(); s.Frame(runtime); s.Frame(runtime);
    s.Frame(runtime); assert(runtime.moves == moves); // No stale navigation after close.
    std::cout << "PASS: asynchronous preview open/fail/timeout, bounded pending navigation, rotation scope, close/drain and repeated lifecycle\n";
}
