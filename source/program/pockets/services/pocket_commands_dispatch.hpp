#pragma once
#include "shortcut_declarations.hpp"
#include "program/pockets/game/pocket_commands_native.hpp"
namespace qol_pockets::commands {
// Internal callback/installation bridge, not a feature-facing execution API.
DeclarationMatch CompareShortcutRules(std::span<const ShortcutRule> rules);
void SetShortcutRules(std::span<const ShortcutRule> rules);
bool QuickStoreInput(void *, void *, unsigned);
void EnterMenu(void *, game::pocket_commands_native::NativeCallback);
void UpdateMenu(void *, game::pocket_commands_native::NativeCallback, game::pocket_commands_native::NativeCallback);
void UpdateCommand(void *, game::pocket_commands_native::NativeCallback);
void EnterPlay(void *, game::pocket_commands_native::NativeCallback);
}
