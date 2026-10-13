#include "quick_store.hpp"
#include "pockets.hpp"
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

namespace qol_pockets::commands {
__attribute__((noinline)) bool InstallShortcuts(std::span<const ShortcutRule> rules) {
    using game::FeedbackTiming;
    // Store confirms only after native success; trigger actions confirm once
    // before dispatch. Direct Open already received the native A click.
    return InstallCommandHandlers(rules, {
        {features::StoreAtHome, FeedbackTiming::AfterSuccess, true},
        {features::DropOrRelease, FeedbackTiming::BeforeExecution, true},
        {features::EquipOrConsume, FeedbackTiming::BeforeExecution, true},
        {features::OpenContainerDirectly, FeedbackTiming::None, false}});
}
}
