#pragma once
#include <cstdint>
namespace qol::core {
// ResourceSizeTable queries use the normalized path without compression suffix.
inline constexpr char CounterResourcePath[] = "Layout/LBellCounterRT.Nin_NX_NVN";
// 0.3.13's texture archive expands to 463,528 bytes; vanilla permits 77,952.
// Reserve room for the archive and native layout allocations. The package
// builder checks its expanded size against this floor before shipping.
inline constexpr std::uint32_t CounterResourceMinimumBytes = 1048576;
// The basket texture expands the item row archive beyond its native 144,512
// byte allowance. Leave room for the archive and native layout objects.
inline constexpr char FittingResourcePath[] = "Layout/LBtnListShopTailorBtn.Nin_NX_NVN";
inline constexpr std::uint32_t FittingResourceMinimumBytes = 524288;
// Installed RomFS assets need space even when their feature is disabled.
// Both paths share one hook, but each allowance requires its own module marker.
bool PrepareShoppingResources();
bool PrepareFittingResources();
}
