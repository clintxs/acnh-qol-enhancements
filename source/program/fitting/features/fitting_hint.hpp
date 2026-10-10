#pragma once
#include <cstdint>
namespace able_bag {
bool FocusedItemIsBagged(std::uintptr_t owner);
bool ValidateFittingHint();
void InstallFittingHint();
}
