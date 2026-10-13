#include "quick_store.hpp"
#include "program/pockets/services/pocket_commands.hpp"

namespace qol_pockets::features {
bool InstallQuickStore() {
    using namespace commands;
    static constexpr ShortcutRule shortcuts[]{
        {UiButton::Y, PocketAction::PutInStorage},
        {UiButton::ZL, PocketAction::DropOrRelease},
        {UiButton::ZR, PocketAction::EquipOrConsume},
    };
    static_assert(ValidShortcuts(shortcuts));
    return InstallShortcuts(shortcuts);
}
} // namespace qol_pockets::features
