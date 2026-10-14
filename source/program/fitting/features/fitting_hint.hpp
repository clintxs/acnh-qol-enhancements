#pragma once
#include <cstdint>
namespace able_bag {
// Change the native ticket input unit so dispatch and its footer agree.
void RemapTailorTicketInput(std::uintptr_t owner);
bool ValidateFittingHint();
bool InstallFittingHint();
}
