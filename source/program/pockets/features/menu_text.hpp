#pragma once
#include <string_view>
namespace qol_pockets::features {
const char16_t *PocketMessage(std::string_view key);
bool ValidateMenuText();
void InstallMenuText();
} // namespace qol_pockets::features
