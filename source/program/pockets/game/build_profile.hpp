#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace qol_pockets::game {
inline constexpr char ExpectedBuildId[] = "FF1D1C05670DB6021C85B624A710B963";
struct InstructionFingerprint {
    std::size_t offset;
    std::uint32_t instruction;
};

// Module-relative offsets for the supplied 3.0.3 main, not absolute addresses.
// This small runtime fingerprint is NOT a full build-ID/hash verification.
// The build preparation script separately verifies the source NSO build ID.
inline constexpr std::array ExpectedCode{
    InstructionFingerprint{0x03076348, 0xd10183ff}, // Generic Split prologue.
    InstructionFingerprint{0x0307e7e0, 0xd10203ff}, // Pocket menu builder.
    InstructionFingerprint{0x03088df0, 0xd10383ff}, // Menu row construction.
    InstructionFingerprint{0x0307d490, 0xd10083ff}, // Action-to-callback lookup.
    InstructionFingerprint{0x03089484, 0x97ffd4d7}, // Original menu builder call.
};

inline bool MatchesExpectedCode(const std::byte *text, std::size_t size) {
    for (const auto &expected : ExpectedCode) {
        if (expected.offset > size || size - expected.offset < sizeof(std::uint32_t)) {
            return false;
        }
        std::uint32_t actual;
        std::memcpy(&actual, text + expected.offset, sizeof(actual));
        if (actual != expected.instruction) {
            return false;
        }
    }
    return true;
}
} // namespace qol_pockets::game
