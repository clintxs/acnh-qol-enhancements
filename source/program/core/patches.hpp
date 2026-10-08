#pragma once
#include <array>
#include <cstdint>

namespace qol::patches {
// Audited Faster Interactions 0.3.0 and Amiibo Scan 1.0.0 payloads.
// Main-relative branches retain their original addresses; the 48-byte helper
// uses verified zero RX padding, never writable state. No IPS is distributed.
struct Patch { std::uintptr_t offset; unsigned words; std::array<std::uint32_t, 12> original, replacement; };
inline constexpr Patch DialoguePatches[]{
    {0x03f06fb0, 12, {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000}, {0x36080054, 0x1e202800, 0x176fdc25, 0x36080048, 0x1e202800, 0x176e3e89, 0xb50000a0, 0xf9401668, 0x52843689, 0x3829691f, 0x176fcc0d, 0x176fcc04}},
    {0x01a969e0, 1, {0x14000002}, {0x1491c177}},
    {0x01af9fe8, 1, {0xb4000120}, {0x149033f8}},
    {0x01af9ff0, 1, {0x528c0049}, {0x52880049}},
    {0x01af9ff8, 1, {0x540000a0}, {0x1a9f07ea}},
    {0x01afa004, 1, {0x5280002a}, {0xd503201f}},
    {0x01afdec8, 1, {0x3934dec9}, {0x2a090529}},
    {0x01afdecc, 1, {0x340006e9}, {0x32000134}},
    {0x01afded0, 1, {0x52800034}, {0x3934ded4}},
    {0x01afdefc, 1, {0x14000054}, {0x1490242d}},
};
inline constexpr Patch AmiiboPatches[]{
    {0x00bc4140, 1, {0x52803848}, {0x52846508}},
};
bool InstallDialogue();
bool InstallAmiibo();
} // namespace qol::patches
