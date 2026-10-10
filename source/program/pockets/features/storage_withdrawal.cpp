#include "storage_withdrawal.hpp"
#include "storage_policy.hpp"
#include "program/pockets/game/stack_bindings.hpp"
#include <array>
#include <cstring>

namespace qol_pockets::features {
namespace {
namespace address {
constexpr std::uintptr_t EnterCommand = 0x0316dd58;
constexpr std::uintptr_t ProcessSelection = 0x0316deec;
constexpr std::uintptr_t BuildCommands = 0x0316b2dc;
constexpr std::uintptr_t StorageVtable = 0x04a16028;
constexpr std::uintptr_t BuilderVtable = 0x04a164e8;
constexpr std::uintptr_t DescriptionVtable = 0x049c7208;
constexpr std::uintptr_t EmptyCallbackVtable = 0x049c7230;
} // namespace address
// Live controller/menu fields verified for ACNH 3.0.3. We access only the
// observed fields rather than declaring a speculative full game class.
namespace layout {
constexpr std::size_t StorageMode = 0x220;
constexpr std::size_t CurrentState = 0x1b0;
constexpr std::size_t TransitionSlot = 0x200;
constexpr std::size_t SelectedStorageSlot = 0xaa9e9;
constexpr std::size_t StorageMenu = 0xaa9d0;
constexpr std::size_t TransitionA = 0x1f0;
constexpr std::size_t TransitionB = 0x1f4;
constexpr std::size_t BuilderVtable = 0x218;
constexpr std::size_t BuilderOwner = 0x220;
constexpr std::size_t BuilderFunction = 0x228;
constexpr std::size_t BuilderAdjustment = 0x230;
constexpr std::size_t MenuRowCount = 0x26d8;
constexpr std::size_t MenuSelection = 0x26c9;
constexpr std::size_t FirstMenuAction = 0x238 + 0x168;
} // namespace layout
namespace state {
constexpr int EnteringCommand = 5, ProcessingCommand = 6;
constexpr int InitialMenu = 0, IdleMenu = 1, NoQueuedTransition = -1;
} // namespace state
constexpr std::uint8_t NoSelection = 0xff;
template <typename T> T &Field(void *object, std::size_t offset) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(object) + offset);
}

struct MenuRow {
    const char *bank{};
    const char *key{};
    std::int32_t action{};
    std::uint32_t flags{};
    std::uint32_t extra{};
    std::uint32_t padding{};
};
struct MenuCallback {
    std::uintptr_t vtable{};
    std::uintptr_t fields[3]{};
};
// Matches the temporary descriptor built by 0x02cb35a8..0x02cb3694.
// Use the game's capacity/accessor vtable and a native SafeString for the bank.
struct MenuDescription {
    std::uintptr_t vtable{};
    std::uintptr_t bankVtable{};
    const char *bank{};
    std::byte reserved[0x28]{};
    std::int32_t error{};
    std::int32_t count{};
    MenuRow rows[7]{};
    MenuCallback callbacks[7]{};
};
static_assert(sizeof(MenuRow) == 0x20 && offsetof(MenuDescription, count) == 0x44);
static_assert(offsetof(MenuDescription, rows) == 0x48 &&
              offsetof(MenuDescription, callbacks) == 0x128);
static_assert(sizeof(MenuDescription) == 0x208);

bool CanWithdrawDirectly(void *storage) {
    // Home-storage browsing only. Interior decorating and other controllers keep
    // their normal interaction, even if they happen to have a one-row menu.
    if (Field<std::uintptr_t>(storage, 0) != game::MainAddress(address::StorageVtable) ||
        Field<int>(storage, layout::StorageMode) != 0) {
        return false;
    }
    const int controllerState = Field<int>(storage, layout::CurrentState);
    if (controllerState != state::EnteringCommand && controllerState != state::ProcessingCommand) {
        return false;
    }
    const auto transitionSlot = Field<std::uint8_t>(storage, layout::TransitionSlot);
    if (transitionSlot > 1 ||
        Field<int>(storage, layout::TransitionA + transitionSlot * sizeof(int)) != -1 ||
        Field<std::int8_t>(storage, layout::SelectedStorageSlot) < 0) {
        return false;
    }
    auto *menu = Field<void *>(storage, layout::StorageMenu);
    if (!menu) {
        return false;
    }
    auto &menuState = Field<int>(menu, layout::CurrentState);
    // Native IsInactive (0x02cb1d98) accepts BOTH initial state 0 and idle 1,
    // provided neither transition slot is queued. Before its first opening the
    // child may still be in state 0; requiring 1 forced one vanilla popup.
    if ((menuState != state::InitialMenu && menuState != state::IdleMenu) ||
        Field<int>(menu, layout::TransitionA) != -1 ||
        Field<int>(menu, layout::TransitionB) != -1) {
        return false;
    }
    // Only probe the known builder, which just appends rows without side effects.
    // If another mod replaces its binding, fall back instead of invoking an
    // unknown callback an extra time. Entry hooks can still add rows to the probe.
    if (Field<std::uintptr_t>(menu, layout::BuilderVtable) !=
            game::MainAddress(address::BuilderVtable) ||
        Field<void *>(menu, layout::BuilderOwner) != storage ||
        Field<std::uintptr_t>(menu, layout::BuilderFunction) !=
            game::MainAddress(address::BuildCommands) ||
        Field<std::uintptr_t>(menu, layout::BuilderAdjustment) != 0) {
        return false;
    }

    MenuDescription description{};
    description.vtable = game::MainAddress(address::DescriptionVtable);
    description.bankVtable = *reinterpret_cast<const std::uintptr_t *>(
                                 game::MainAddress(game::stack_offsets::SafeStringVtableGot)) +
                             16;
    description.bank = "CmnSubWin";
    for (auto &callback : description.callbacks) {
        callback.vtable = game::MainAddress(address::EmptyCallbackVtable);
    }
    game::FunctionAt<void (*)(void *, MenuDescription *)>(address::BuildCommands)(storage,
                                                                                  &description);
    const auto &row = description.rows[0];
    return IsOnlyMoveToPockets(description.count, row.action, row.bank, row.key, row.flags,
                               row.extra);
}

void WithdrawDirectly(void *storage, void (*processSelection)(void *)) {
    auto *menu = Field<void *>(storage, layout::StorageMenu);
    auto &menuState = Field<int>(menu, layout::CurrentState);

    // Present a confirmed one-row result to the ORIGINAL selection processor.
    // No popup, animation, or second input is started. The selection processor
    // requires state 1, so expose that only during the call, then restore the
    // child's actual lifecycle state (including first-open state 0).
    // Restore cached row/count fields afterward and consume the selection so a
    // same-frame update cannot repeat the transfer. No inventory data is edited here.
    auto &count = Field<int>(menu, layout::MenuRowCount);
    auto &action = Field<int>(menu, layout::FirstMenuAction);
    auto &selection = Field<std::uint8_t>(menu, layout::MenuSelection);
    const auto previousRowCount = count, previousAction = action, previousMenuState = menuState;
    menuState = state::IdleMenu;
    count = 1;
    action = MoveToPockets;
    selection = 0;
    processSelection(storage);
    selection = NoSelection;
    count = previousRowCount;
    action = previousAction;
    menuState = previousMenuState;
    // ProcessSelection checks capacity and queues normal Play or ErrorWindow.
}

// UI callbacks run on one thread. This is an identity token, never dereferenced:
// only the live controller supplied to its next update can consume it. Every
// command entry replaces it, so a later storage session cannot inherit a request.
void *PendingStorage{};

// Entry only records intent; Update consumes it after the native transition
// has finished clearing its queues. Moving the item during Entry loses refresh.
HOOK_DEFINE_TRAMPOLINE(EnterStorageCommand) {
    static void Callback(void *storage) {
        PendingStorage = nullptr;
        if (!CanWithdrawDirectly(storage)) {
            Orig(storage);
            return;
        }
        // The native transition consumer (0x02b0e3e4..0x02b0e3ec) clears BOTH
        // pending slots AFTER entry returns. Never transfer or queue Play here.
        PendingStorage = storage;
    }
};

HOOK_DEFINE_TRAMPOLINE(UpdateStorageCommand) {
    static void Callback(void *storage) {
        if (PendingStorage != storage) {
            Orig(storage);
            return;
        }
        PendingStorage = nullptr; // Consume before calling game code: exactly once.
        if (!CanWithdrawDirectly(storage)) {
            EnterStorageCommand::Orig(storage);
            return;
        }
        WithdrawDirectly(storage, [](void *owner) { Orig(owner); });
        static bool reported = false;
        if (!reported) {
            Logging.Debug("[QoL Enhancements/Pockets] STORAGE_DIRECT single-action menu bypassed\n");
            reported = true;
        }
    }
};
} // namespace

// Instruction fingerprints are exact 3.0.3 ARM words, intentionally left as
// hex. They guard hook locations and native assumptions; do not edit them to
// silence a mismatch without re-checking the game's disassembly.
bool InstallStorageWithdrawal() {
    struct Fingerprint {
        std::uintptr_t offset;
        std::array<std::uint32_t, 5> words;
    };
    constexpr std::array checks{
        Fingerprint{address::EnterCommand,
                    {0xd10143ff, 0xa9027bfd, 0xf9001bf5, 0xa9044ff4, 0x910083fd}},
        Fingerprint{address::ProcessSelection,
                    {0xd10143ff, 0xa9017bfd, 0xf90013f7, 0xa90357f6, 0xa9044ff4}},
        Fingerprint{address::BuildCommands,
                    {0xa9be7bfd, 0xa9014ff4, 0x910003fd, 0xf9400028, 0xaa0103e0}}};
    for (const auto &check : checks) {
        if (std::memcmp(reinterpret_cast<const void *>(game::MainAddress(check.offset)),
                        check.words.data(), sizeof(check.words)) != 0) {
            Logging.Error("[QoL Enhancements/Pockets] STORAGE_DISABLED code mismatch at 0x%lx\n",
                        check.offset);
            return false;
        }
    }
    EnterStorageCommand::InstallAtOffset(address::EnterCommand);
    UpdateStorageCommand::InstallAtOffset(address::ProcessSelection);
    Logging.Debug("[QoL Enhancements/Pockets] STORAGE_READY single Move to Pockets bypass\n");
    return true;
}
} // namespace qol_pockets::features
