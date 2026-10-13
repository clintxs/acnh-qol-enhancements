#pragma once
#include "program/core/native/executable_view.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace catalog_aware_shops::game {
inline constexpr char ExpectedBuildId[] = "FF1D1C05670DB6021C85B624A710B963";
using InstructionFingerprint = qol::native::Fingerprint<1>;

// Module-relative offsets for the supplied 3.0.3 main, not absolute addresses.
// This small runtime fingerprint is NOT a full build-ID/hash verification.
// The build preparation script separately verifies the source NSO build ID.
// Catalog-owned sites only: pockets may already have installed its hooks.
inline constexpr std::array ExpectedCode{
    InstructionFingerprint{0x009d8430, {0xd10603ff}, "catalog.CatalogOrder"}, InstructionFingerprint{0x009d8794, {0xd105c3ff}, "catalog.CatalogPurchase"},
    InstructionFingerprint{0x01b0b8d4, {0xa9ba7bfd}, "catalog.CatalogMessage"}, InstructionFingerprint{0x0254a1e0, {0xa9bd7bfd}, "catalog.CatalogHistory"},
    InstructionFingerprint{0x030117c8, {0xf000d568}, "catalog.CatalogBadge"},
};

inline qol::native::CodeResult ValidateExpectedCode(const qol::native::ExecutableView &text) {
    for (const auto &expected : ExpectedCode) {
        const auto result = text.Check(expected);
        if (!result) return result;
    }
    return {.status = qol::native::CodeStatus::Match};
}

} // namespace catalog_aware_shops::game
