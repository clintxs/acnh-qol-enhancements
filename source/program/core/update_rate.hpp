#pragma once
#include <cstdint>
namespace qol::core {
// Nominal update rate of the verified SpeedFix profile, initialized before
// feature installation. Real-time gestures continue using system ticks.
inline unsigned UpdateFrequency = 30;
inline unsigned NativeFrames(unsigned updates) {
    return static_cast<unsigned>(std::uint64_t(updates) * 30 / UpdateFrequency);
}
inline unsigned UpdateFrames(unsigned nativeFrames) {
    return static_cast<unsigned>((std::uint64_t(nativeFrames) * UpdateFrequency + 29) / 30);
}
inline float UpdateScale() { return 30.f / UpdateFrequency; }
}
