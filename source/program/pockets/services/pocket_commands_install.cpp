#include "pocket_commands_dispatch.hpp"
#include "program/pockets/game/quick_store_profile.hpp"
#include "program/core/hook_plan.hpp"
#include "program/core/native/runtime.hpp"
#include "program/core/page_input.hpp"
namespace qol_pockets::commands {
namespace {
namespace profile = game::v303::quick_store;
namespace address = profile::address;
HOOK_DEFINE_TRAMPOLINE(EnterPocketMenu) {
    static void Original(void *owner) { Orig(owner); }
    static void Callback(void *owner) { EnterMenu(owner, Original); }
};
HOOK_DEFINE_TRAMPOLINE(UpdatePocketMenu) {
    static void Original(void *owner) { Orig(owner); }
    static void Callback(void *owner) {
        UpdateMenu(owner, Original, EnterPocketMenu::Original);
    }
};
HOOK_DEFINE_TRAMPOLINE(QuickStoreCommand) {
    static void Original(void *owner) { Orig(owner); }
    static void Callback(void *owner) { UpdateCommand(owner, Original); }
};
HOOK_DEFINE_TRAMPOLINE(QuickStorePlay) {
    static void Original(void *owner) { Orig(owner); }
    static void Callback(void *owner) { EnterPlay(owner, Original); }
};
} // namespace
// Instruction fingerprints are exact 3.0.3 ARM words, intentionally left as
// hex. They guard hook locations and native assumptions; do not edit them to
// silence a mismatch without re-checking the game's disassembly.
bool InstallShortcuts(std::span<const ShortcutRule> rules) {
    if (!ValidShortcuts(rules)) return false;
    const auto executable = qol::native::MainExecutable();
    for (const auto &check : profile::Checks) {
        const auto result = executable.Check(check);
        if (!result) {
            Logging.Error("[QoL Enhancements/Pockets] QUICK_STORE_DISABLED code mismatch at 0x%lx\n", check.offset);
            qol::native::ReportMismatch(result);
            return false;
        }
    }

    qol::core::HookPlan hookPlan{
        qol::core::Site<QuickStoreCommand>("QuickStoreCommand", address::Command, profile::HookWords(address::Command), true),
        qol::core::Site<QuickStorePlay>("QuickStorePlay", address::Play, profile::HookWords(address::Play), true),
        qol::core::Site<EnterPocketMenu>("EnterPocketMenu", address::EnterMenu, profile::HookWords(address::EnterMenu), true),
        qol::core::Site<UpdatePocketMenu>("UpdatePocketMenu", address::UpdateMenu, profile::HookWords(address::UpdateMenu), true),
    };
    if (!hookPlan) return false;
    if (!qol::core::RegisterPageInput(QuickStoreInput)) {
        Logging.Error("[QoL Enhancements/Pockets] QUICK_STORE_DISABLED shared input unavailable\n");
        return false;
    }
    SetShortcutRules(rules);
    if (!hookPlan.Publish<QuickStoreCommand>(address::Command)) return false;
    if (!hookPlan.Publish<QuickStorePlay>(address::Play)) return false;
    if (!hookPlan.Publish<EnterPocketMenu>(address::EnterMenu)) return false;
    if (!hookPlan.Publish<UpdatePocketMenu>(address::UpdateMenu)) return false;
    Logging.Debug("[QoL Enhancements/Pockets] OPEN_READY single-action A menu bypass\n");
    Logging.Debug("[QoL Enhancements/Pockets] QUICK_STORE_READY Y storage; ZL drop/release; ZR hold/wear/put-away\n");
    return true;
}
} // namespace qol_pockets::commands
