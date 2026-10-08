#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace catalog_aware_shops::game {
inline constexpr char ExpectedBuildId[] = "FF1D1C05670DB6021C85B624A710B963";
struct InstructionFingerprint {
    std::size_t offset;
    std::uint32_t instruction;
};

// Module-relative offsets for the supplied 3.0.3 main, not absolute addresses.
// This small runtime fingerprint is NOT a full build-ID/hash verification.
// The build preparation script separately verifies the source NSO build ID.
// Catalog-owned sites only: pockets may already have installed its hooks.
inline constexpr std::array ExpectedCode{
    InstructionFingerprint{0x009d8430, 0xd10603ff}, InstructionFingerprint{0x009d8794, 0xd105c3ff},
    InstructionFingerprint{0x01b0b8d4, 0xa9ba7bfd}, InstructionFingerprint{0x0254a1e0, 0xa9bd7bfd},
    InstructionFingerprint{0x030117c8, 0xf000d568},
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
} // namespace catalog_aware_shops::game
