#pragma once
#include <cstdint>
namespace qol::shopping {
// Allocation support remains necessary while the package assets are installed,
// including when its behavior is disabled through configuration.
bool PrepareResources();
bool Install();
// Fitting Room's C++ preflight must use the same combined funds as checkout.
unsigned AvailableBells(const std::uint8_t *player);
}
