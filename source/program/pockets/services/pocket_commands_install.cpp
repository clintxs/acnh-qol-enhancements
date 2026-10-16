#include "pocket_commands_dispatch.hpp"
#include "program/pockets/game/quick_store_profile.hpp"
#include "program/core/hook_plan.hpp"
#include "program/core/native/runtime.hpp"
#include "program/core/native/speedfix.hpp"
#include "program/pockets/game/quick_store_native.hpp"
#include "program/core/page_input.hpp"
namespace qol_pockets::commands {
namespace {
namespace profile = game::v303::quick_store;
namespace address = profile::address;
unsigned MenuDelayUpdates{};
// SpeedFix's external stub returns to UpdateMenu+12, inside the maximum
// 20-byte entry patch. A relocated trampoline cannot repair that external
// back-edge. Implement this small, fully fingerprinted native predicate in
// C++ for SpeedFix only; vanilla continues using the original trampoline.
void UpdateSpeedFixMenu(void *owner) {
    using game::quick_store_native::Field;
    constexpr std::size_t Menu = 0x218, ResultAndAge = 0x1b0;
    constexpr std::size_t Resident = 0x200, RequestedStates = 0x1f0;
    auto *menu = Field<void *>(owner, Menu);
    const auto pair = Field<std::uint64_t>(menu, ResultAndAge);
    if (static_cast<std::int32_t>(pair >> 32) < static_cast<int>(MenuDelayUpdates) ||
        static_cast<std::uint32_t>(pair) != 1) return;
    const auto resident = Field<std::uint8_t>(owner, Resident);
    auto &requested = Field<std::int32_t>(owner, RequestedStates + resident * sizeof(std::int32_t));
    if (requested == -1) requested = 7;
}
HOOK_DEFINE_TRAMPOLINE(EnterPocketMenu) {
    static void Original(void *owner) { Orig(owner); }
    static void Callback(void *owner) { EnterMenu(owner, Original); }
};
HOOK_DEFINE_TRAMPOLINE(UpdatePocketMenu) {
    static void Original(void *owner) {
        if (MenuDelayUpdates) UpdateSpeedFixMenu(owner);
        else Orig(owner);
    }
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
bool InstallCommandHandlers(std::span<const ShortcutRule> rules, MenuPolicies policies) {
    if (!AcceptsMenuPolicies(policies)) return false;
    if (!ValidShortcuts(rules)) return false;
    const auto match = CompareShortcutRules(rules);
    if (match == DeclarationMatch::Conflict) {
        qol::core::ReportActivationFailure(qol::core::FeatureState::DependencyUnavailable, "incompatible pocket shortcuts");
        return false;
    }
    const auto executable = qol::native::MainExecutable();
    for (const auto &check : profile::Checks) {
        // HookPlan verifies already-owned instructions on identical repeats.
        if (match == DeclarationMatch::Identical && (check.offset == address::Command ||
            check.offset == address::Play || check.offset == address::EnterMenu || check.offset == address::UpdateMenu)) continue;
        const auto result = qol::native::CheckCompatibleCode(executable, check);
        if (!result) {
            Logging.Error("[QoL Enhancements/Pockets] QUICK_STORE_DISABLED code mismatch at 0x%lx\n", check.offset);
            qol::native::ReportMismatch(result);
            return false;
        }
    }

    const auto fps = qol::native::SpeedFixFrequency(executable);
    if (fps != 30) {
        const qol::native::Fingerprint<> tail[]{
            {0x0308c5a4, {0x9107c009, 0xb868792a, 0x3100055f, 0x54000040, 0xd65f03c0}, "pockets.speedfix.menu_tail"},
            {0x0308c5b8, {0x528000ea, 0xb828792a, 0xd65f03c0, 0x5298a2e8, 0x72a00028}, "pockets.speedfix.menu_result"},
        };
        for (const auto &check : tail) {
            const auto result = executable.Check(check);
            if (!result) { qol::native::ReportMismatch(result); return false; }
        }
        MenuDelayUpdates = fps == 120 ? 4 : 2;
    }

    qol::core::HookPlan hookPlan{
        qol::core::Site<QuickStoreCommand>("QuickStoreCommand", address::Command, profile::HookWords(address::Command), true),
        qol::core::Site<QuickStorePlay>("QuickStorePlay", address::Play, profile::HookWords(address::Play), true),
        qol::core::Site<EnterPocketMenu>("EnterPocketMenu", address::EnterMenu, profile::HookWords(address::EnterMenu), true),
        qol::core::Site<UpdatePocketMenu>("UpdatePocketMenu", address::UpdateMenu, profile::HookWords(address::UpdateMenu), true),
    };
    if (!hookPlan) return false;
    if (!qol::core::RegisterPageInput<PocketPageInput>(qol::core::InputPriority::Pockets)) {
        Logging.Error("[QoL Enhancements/Pockets] QUICK_STORE_DISABLED shared input unavailable\n");
        return false;
    }
    SetShortcutRules(rules);
    SetMenuPolicies(policies);
    if (!hookPlan.Publish<QuickStoreCommand>(address::Command)) return false;
    if (!hookPlan.Publish<QuickStorePlay>(address::Play)) return false;
    if (!hookPlan.Publish<EnterPocketMenu>(address::EnterMenu)) return false;
    if (!hookPlan.Publish<UpdatePocketMenu>(address::UpdateMenu)) return false;
    Logging.Debug("[QoL Enhancements/Pockets] OPEN_READY single-action A menu bypass\n");
    Logging.Debug("[QoL Enhancements/Pockets] QUICK_STORE_READY Y storage; ZL drop/release; ZR hold/wear/put-away\n");
    return true;
}
} // namespace qol_pockets::commands
