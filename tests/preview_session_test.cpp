#include "program/crafting/services/preview_session.hpp"
#include <cassert>
#include <iostream>
using namespace qol::crafting::preview;
struct Runtime {
    bool available = true;
    ModelProgress model = ModelProgress::Waiting;
    NavigationProgress navigation = NavigationProgress::Retry;
    unsigned begins{}, creates{}, closes{}, declined{}, moves{}, banners{}, rotations{}, reports{}, refreshes{};
    void RefreshHints() { ++refreshes; }
    bool Begin() { ++begins; return available; }
    void OpenDeclined() { ++declined; }
    void Close() { ++closes; }
    ModelProgress CreateModel() { ++creates; return model; }
    NavigationProgress Navigate(int direction) { assert(direction == -1 || direction == 1); ++moves; return navigation; }
    void PlaceBanner() { ++banners; }
    void Rotate() { ++rotations; }
    void ReportShowing() { ++reports; }
};
int main() {
    Session s; Runtime runtime;
    s.Rotate(1,1); s.Navigate(1);
    assert(s.rotation.x == 0 && s.navigation.sample == 0);
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
    assert(s.State() == Phase::Showing);
    s.Navigate(1); s.Rotate(1,-1); s.Frame(runtime);
    assert(s.navigation.pending == 1 && runtime.moves == 1 && runtime.banners == 1 && runtime.rotations == 1);
    // One pending navigation survives loading/retry; it cannot grow a backlog.
    s.Frame(runtime); assert(s.navigation.pending == 1 && runtime.moves == 2);
    runtime.navigation = NavigationProgress::Moved;
    s.Frame(runtime); assert(s.navigation.pending == 0 && runtime.moves == 3);
    for (unsigned frame = 0; frame < 26; ++frame) s.Frame(runtime);
    assert(runtime.reports == 1);
    s.RequestClose(); s.Frame(runtime);
    assert(s.State() == Phase::DrainBack && runtime.closes == 3 && !s.navigation.pending && !s.rotation.x);
    for (unsigned cycle = 0; cycle < 20; ++cycle) {
        s.ReleaseBack(); s.Open(); s.Frame(runtime); s.Frame(runtime);
        assert(s.State() == Phase::Showing);
        s.RequestClose(); s.Frame(runtime); assert(s.State() == Phase::DrainBack);
    }
    Navigation navigation;
    navigation.sample = 1; assert(navigation.Step() == 1 && navigation.wait == 12);
    navigation.pending = 0;
    for (unsigned frame = 0; frame < 12; ++frame) { navigation.sample = 1; assert(navigation.Step() == 0); }
    navigation.sample = 1; assert(navigation.Step() == 1 && navigation.wait == 4);
    std::cout << "PASS: asynchronous preview open/fail/timeout, bounded pending navigation, rotation scope, close/drain and repeated lifecycle\n";
}
