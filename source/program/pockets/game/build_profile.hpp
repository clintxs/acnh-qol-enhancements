#pragma once
#include "program/core/native/executable_view.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace qol_pockets::game {
inline constexpr char ExpectedBuildId[] = "FF1D1C05670DB6021C85B624A710B963";
using InstructionFingerprint = qol::native::Fingerprint<1>;

// Module-relative offsets for the supplied 3.0.3 main, not absolute addresses.
// This small runtime fingerprint is NOT a full build-ID/hash verification.
// The build preparation script separately verifies the source NSO build ID.
inline constexpr std::array ExpectedCode{
    InstructionFingerprint{0x03076348, {0xd10183ff}, "pockets.Split"}, // Generic Split prologue.
    InstructionFingerprint{0x0307e7e0, {0xd10203ff}, "pockets.BuildMenu"}, // Pocket menu builder.
    InstructionFingerprint{0x03088df0, {0xd10383ff}, "pockets.BuildRows"}, // Menu row construction.
    InstructionFingerprint{0x0307d490, {0xd10083ff}, "pockets.MapAction"}, // Action-to-callback lookup.
    InstructionFingerprint{0x03089484, {0x97ffd4d7}, "pockets.MenuBuilderCall"}, // Original menu builder call.
};

inline qol::native::CodeResult ValidateExpectedCode(const qol::native::ExecutableView &text) {
    for (const auto &expected : ExpectedCode) {
        const auto result = text.Check(expected);
        if (!result) return result;
    }
    return {.status = qol::native::CodeStatus::Match};
}

} // namespace qol_pockets::game
