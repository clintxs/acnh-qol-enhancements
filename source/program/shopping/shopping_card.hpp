#pragma once
#include <cstdint>
namespace qol::shopping {
bool Install();
// Fitting Room's C++ preflight must use the same account as native checkout.
unsigned AvailableBells(const std::uint8_t *player);
}
