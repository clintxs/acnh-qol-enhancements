#pragma once
#include <cstdint>
namespace qol::shopping {
// ResourceSizeTable queries use the normalized path without compression suffix.
inline constexpr char CounterResourcePath[] = "Layout/LBellCounterRT.Nin_NX_NVN";
// 0.3.13's texture archive expands to 463,528 bytes; vanilla permits 77,952.
// Reserve room for the archive and native layout allocations. The package
// builder checks its expanded size against this floor before shipping.
inline constexpr std::uint32_t CounterResourceMinimumBytes = 1048576;
}
