#pragma once
#include <cstdint>
namespace qol::controls::features {
// Small neutral zone, then a linear analog speed range. Native steering still
// uses the game's own stick handling; this only sets swimming propulsion.
constexpr float StickDeadZone = .15f;
constexpr float QuietStrideScale = .8f; // Native FlapFoot speed multiplier.
constexpr float BoostFraction = .625f;
constexpr std::uint64_t TicksPerSecond = 19'200'000ull;
constexpr std::uint64_t StrokeInterval = TicksPerSecond * 2;
constexpr std::uint64_t BoostDelay = TicksPerSecond / 2;
constexpr std::uint64_t BoostDuration = TicksPerSecond * 3 / 2;
constexpr std::uint64_t SessionGap = TicksPerSecond / 2;
struct StrokeState {
    std::uint64_t lastStroke{}, lastUpdate{};
    float cruiseSpeed{};
    bool moving{}, underwater{}, armPending{}, pendingManual{};
    bool hasStroke{}, armAnimation{};
};
inline float StickThrottle(float x, float y) {
    const float squared = x*x + y*y;
    // Reject corrupt/nonfinite inputs before sqrt; clamp diagonal overtravel.
    if (!(squared > StickDeadZone*StickDeadZone && squared <= 2.f)) return 0.f;
    const float magnitude = __builtin_sqrtf(squared);
    return magnitude >= 1.f ? 1.f : (magnitude - StickDeadZone) / (1.f - StickDeadZone);
}
inline float BoostEnvelope(const StrokeState &state, std::uint64_t now) {
    const auto elapsed = now - state.lastStroke;
    // Match the arm push rather than the start of the animation. Keep the
    // full 1.5-second fade after this delay, independent of stroke cooldown.
    if (!state.hasStroke || elapsed < BoostDelay || elapsed >= BoostDelay + BoostDuration) return 0.f;
    return 1.f - float(elapsed - BoostDelay) / float(BoostDuration);
}
struct StrokeRequest { bool active{}, pulse{}, armEntry{}; float boost{}; };
// One policy for surface/diving entry/update. The adapter owns identity resets
// and lends the resulting A input only during the original synchronous call.
inline StrokeRequest SwimWithStick(StrokeState &state, std::uint64_t now,
                                  bool underwater, bool entry, bool nested,
                                  float throttle, bool manualA, bool transition) {
    // Preserve the boost cooldown across diving/surfacing, but relearn each
    // mode's native cruise speed. A gap identifies a new swimming session.
    if (state.underwater != underwater || now - state.lastUpdate > SessionGap) {
        state.cruiseSpeed = 0.f;
        state.moving = state.armPending = state.pendingManual = state.armAnimation = false;
    }
    state.underwater = underwater;
    state.lastUpdate = now;
    const bool moving = throttle > 0.f;
    const bool ready = !state.hasStroke || now - state.lastStroke >= StrokeInterval;
    if (!nested && !entry) {
        if (!moving && !state.pendingManual) state.armPending = false;
        const bool manual = manualA;
        if (ready && (manual || (moving && !state.moving))) {
            state.armPending = true;
            state.pendingManual = manual;
        }
        state.moving = moving;
    }
    // Y keeps priority over a queued stroke when diving/surfacing/catching.
    if (transition) state.armPending = state.pendingManual = false;
    const bool armEntry = entry && state.armPending;
    // Consume before native entry so a nested movement helper can see the
    // boost immediately. Entries between requests select quiet foot kicking.
    if (entry) {
        state.armAnimation = armEntry;
        if (armEntry) {
            state.armPending = false;
            state.pendingManual = false;
            state.hasStroke = true;
            state.lastStroke = now;
        }
    }
    const float boost = BoostEnvelope(state, now);
    // Keep a neutral-stick stroke active through its wind-up as well as the
    // impulse, so manual A does not lose its animation before propulsion.
    const bool strokeActive = state.hasStroke && now - state.lastStroke < BoostDelay + BoostDuration;
    const bool active = !transition && (moving || state.armPending || strokeActive);
    const bool pulse = !entry && state.armPending && !transition;
    return {active, pulse, armEntry, boost};
}
inline float LearnCruiseSpeed(float nativeSpeed, bool armAnimation) {
    return nativeSpeed * (armAnimation ? QuietStrideScale : 1.f);
}
inline float SwimmingPropulsion(float cruiseSpeed, float throttle, float boost, bool strokeBoost) {
    return cruiseSpeed * (throttle + (strokeBoost ? BoostFraction * boost : 0.f));
}
}
