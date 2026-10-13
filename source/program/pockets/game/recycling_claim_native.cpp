#include "program/core/hook_plan.hpp"
#include "program/pockets/features/recycling_claim.hpp"
#include "program/pockets/game/item_menu.hpp"
#include "program/pockets/settings.hpp"
#include "program/pockets/game/stack_bindings.hpp"
#include <array>
#include <cstring>

namespace qol_pockets::features {
namespace {
game::ChooseMenuEntry chooseClaim{};
namespace address {
constexpr std::uintptr_t EnterMenu = 0x02df1c58, ProcessSelection = 0x02df1d68;
constexpr std::uintptr_t BuildMenu = 0x02defbfc;
constexpr std::uintptr_t ControllerVtable = 0x049df828, BuilderVtable = 0x049dfba8;
constexpr std::uintptr_t DescriptionVtable = 0x049c7208, EmptyCallbackVtable = 0x049c7230;
constexpr std::uintptr_t NameWindowVtable = 0x04a01130, DisappearNameWindow = 0x030127b8;
constexpr std::uintptr_t SetGridState = 0x03dbb160, ChangeGridPauseMode = 0x03764b90;
}
namespace layout {
constexpr std::size_t State = 0x1b0, Queues = 0x1f0, Phase = 0x200;
constexpr std::size_t SelectedSlot = 0x218, Menu = 0x228;
constexpr std::size_t NameWindow = 0x220;
constexpr std::size_t CloseRequested = 0x201;
constexpr std::size_t Page = 0x210, ItemGrid = 0x578;
constexpr std::size_t GridStateMachine = 0x290, GridState = 0x298, GridPauseMode = 0x2e4;
constexpr std::size_t ItemsCount = 0x1c1d8, Items = 0x1c1e0, AvailableSlots = 0x1c1e8;
constexpr std::size_t Builder = 0x218, BuilderOwner = 0x220, BuilderFunction = 0x228, Adjustment = 0x230;
constexpr std::size_t RowCount = 0x26d8, Selection = 0x26c9, FirstAction = 0x3a0;
}
constexpr int SelectingCommand = 4, ClaimAction = 0, SlotLimit = 60;
template <typename T> T &Field(void *object, std::size_t offset) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(object) + offset);
}
struct MenuRow {
    const char *bank{}, *key{};
    int action{};
    unsigned flags{}, extra{}, padding{};
};
struct MenuCallback { std::uintptr_t vtable{}, fields[3]{}; };
struct MenuDescription {
    std::uintptr_t vtable{}, bankVtable{};
    const char *bank{};
    std::byte reserved[0x28]{};
    int error{}, count{};
    MenuRow rows[7]{};
    MenuCallback callbacks[7]{};
};
static_assert(sizeof(MenuDescription) == 0x208 && offsetof(MenuDescription, rows) == 0x48);
struct ClaimRequest {
    void *owner{};
    int slot{};
    std::array<std::byte, 12> item{}; // Native item plus recycling-box record index.
};
ClaimRequest pending{};

void PauseClaimGrid(void *owner) {
    auto *grid = Field<void *>(Field<void *>(owner, layout::Page), layout::ItemGrid);
    // Preserve native CommandSelect entry (02df1c74..02df1cbc), excluding only
    // the popup. Native Claim resumes via 03764fe0, which does NOTHING unless
    // the grid is paused in state 3. Its resume re-establishes cursor focus even
    // when the selected item widget has just become empty/disabled.
    if (Field<int>(grid, layout::GridState) == 3) {
        if (Field<int>(grid, layout::GridPauseMode) != 0)
            game::FunctionAt<void (*)(void *, int)>(address::ChangeGridPauseMode)(grid, 0);
    } else {
        Field<int>(grid, layout::GridPauseMode) = 0;
        game::FunctionAt<void (*)(void *, int)>(address::SetGridState)(
            static_cast<std::byte *>(grid) + layout::GridStateMachine, 3);
    }
}

void DismissClaimedItemName(void *owner, int slot) {
    // Native Claim clears this availability bit only after insertion/removal.
    // Its ordinary popup moves focus away and dismisses the name window; our
    // bypass never opens that popup, so explicitly close the same owned window.
    // Do not close it on a full-pockets or insertion failure.
    if (Field<std::uint8_t>(owner, layout::AvailableSlots + slot / 8) & (1u << (slot % 8))) return;
    auto *nameWindow = Field<void *>(owner, layout::NameWindow);
    if (!nameWindow || Field<std::uintptr_t>(nameWindow, 0) != game::MainAddress(address::NameWindowVtable)) return;
    if (Field<std::uint8_t>(nameWindow, layout::Phase) > 1) return;
    // General CloseController (02b0cb68) also hides the shared cursor and clears
    // its focused pane. Keep only the name window's close flag and native
    // disappearance request (virtual +1c0), leaving the grid's focus untouched.
    Field<std::uint8_t>(nameWindow, layout::CloseRequested) = 1;
    game::FunctionAt<void (*)(void *)>(address::DisappearNameWindow)(nameWindow);
}

bool ProbeClaim(void *owner, ClaimRequest &request, bool entering = false) {
    if (!GetSettings().bypassMenuClaimRecycleBox) return false;
    if (Field<std::uintptr_t>(owner, 0) != game::MainAddress(address::ControllerVtable) ||
        Field<int>(owner, layout::State) != SelectingCommand) return false;
    auto *page = Field<void *>(owner, layout::Page);
    auto *grid = page ? Field<void *>(page, layout::ItemGrid) : nullptr;
    if (!grid || (Field<int>(grid, layout::GridState) != 2 &&
                  Field<int>(grid, layout::GridState) != 3)) return false;
    const unsigned phase = Field<std::uint8_t>(owner, layout::Phase);
    if (phase > 1 || Field<int>(owner, layout::Queues + phase * 4) != -1) return false;
    const int other = Field<int>(owner, layout::Queues + (1 - phase) * 4);
    if (other != -1 && !(entering && other == SelectingCommand)) return false;
    const int slot = Field<int>(owner, layout::SelectedSlot);
    auto *items = Field<std::byte *>(owner, layout::Items);
    if (!items || slot < 0 || slot >= SlotLimit || slot >= Field<int>(owner, layout::ItemsCount) ||
        !(Field<std::uint8_t>(owner, layout::AvailableSlots + slot / 8) & (1u << (slot % 8))) ||
        Field<std::uint16_t>(items + slot * 12, 0) == 0xfffe) return false;
    auto *menu = Field<void *>(owner, layout::Menu);
    if (!menu || (Field<int>(menu, layout::State) != 0 && Field<int>(menu, layout::State) != 1) ||
        Field<int>(menu, layout::Queues) != -1 || Field<int>(menu, layout::Queues + 4) != -1 ||
        Field<std::uintptr_t>(menu, layout::Builder) != game::MainAddress(address::BuilderVtable) ||
        Field<void *>(menu, layout::BuilderOwner) != owner ||
        Field<std::uintptr_t>(menu, layout::BuilderFunction) != game::MainAddress(address::BuildMenu) ||
        Field<std::uintptr_t>(menu, layout::Adjustment) != 0) return false;

    MenuDescription description{};
    description.vtable = game::MainAddress(address::DescriptionVtable);
    description.bankVtable = *reinterpret_cast<const std::uintptr_t *>(
        game::MainAddress(game::stack_offsets::SafeStringVtableGot)) + 16;
    description.bank = "CmnSubWin";
    for (auto &callback : description.callbacks)
        callback.vtable = game::MainAddress(address::EmptyCallbackVtable);
    game::FunctionAt<void (*)(void *, MenuDescription *)>(address::BuildMenu)(owner, &description);
    if (description.count < 1 || description.count > 7 || !chooseClaim) return false;
    std::array<game::MenuEntry, 7> entries{};
    for (int i = 0; i < description.count; ++i) {
        const auto &row = description.rows[i];
        const auto &callback = description.callbacks[i];
        const bool claim = row.action == ClaimAction && row.bank && row.key &&
            std::strcmp(row.bank, "CmnSubWin") == 0 && std::strcmp(row.key, "1602") == 0;
        entries[i] = {claim ? game::ItemAction::Claim : game::ItemAction::Unknown,
            claim && row.flags == 0 && row.extra == 0 &&
            callback.vtable == game::MainAddress(address::EmptyCallbackVtable) &&
            callback.fields[0] == 0 && callback.fields[1] == 0 && callback.fields[2] == 0};
    }
    const auto chosen = chooseClaim({entries.data(), static_cast<std::size_t>(description.count)});
    if (!chosen || *chosen != 0 || !entries[*chosen].executable) return false;
    request.owner = owner;
    request.slot = slot;
    std::memcpy(request.item.data(), items + slot * 12, request.item.size());
    return true;
}

HOOK_DEFINE_TRAMPOLINE(EnterRecyclingMenu) {
    static void Callback(void *owner) {
        pending = {};
        if (!ProbeClaim(owner, pending, true)) Orig(owner);
        else PauseClaimGrid(owner);
        // Never transfer during ENTRY: native transition cleanup would discard
        // the next state. A already supplied the ordinary click feedback.
    }
};
HOOK_DEFINE_TRAMPOLINE(UpdateRecyclingMenu) {
    static void Callback(void *owner) {
        if (pending.owner != owner) { Orig(owner); return; }
        const auto expected = pending;
        pending = {};
        ClaimRequest current{};
        if (!ProbeClaim(owner, current) || current.slot != expected.slot || current.item != expected.item) {
            EnterRecyclingMenu::Orig(owner);
            return;
        }
        auto *menu = Field<void *>(owner, layout::Menu);
        const int oldState = Field<int>(menu, layout::State);
        const int oldCount = Field<int>(menu, layout::RowCount);
        const int oldAction = Field<int>(menu, layout::FirstAction);
        Field<int>(menu, layout::State) = 1;
        Field<int>(menu, layout::RowCount) = 1;
        Field<int>(menu, layout::FirstAction) = ClaimAction;
        Field<std::uint8_t>(menu, layout::Selection) = 0;
        // Native code checks capacity, inserts the item, then removes its box
        // record and refreshes the grid. Failure retains the item and opens the
        // game's pockets-full message. Do not reproduce those operations here.
        Orig(owner);
        DismissClaimedItemName(owner, expected.slot);
        Field<int>(menu, layout::State) = oldState;
        Field<int>(menu, layout::RowCount) = oldCount;
        Field<int>(menu, layout::FirstAction) = oldAction;
        Field<std::uint8_t>(menu, layout::Selection) = 0xff;
    }
};
} // namespace

bool RegisterClaimPolicy(game::ChooseMenuEntry choose) {
    if (!choose || (chooseClaim && chooseClaim != choose)) return false;
    struct Check { std::uintptr_t offset; std::array<std::uint32_t, 5> words; };
    constexpr std::array checks{
        Check{address::EnterMenu, {0xd10103ff, 0xa9027bfd, 0xa9034ff4, 0x910083fd, 0xf9410808}},
        Check{address::ProcessSelection, {0xd10183ff, 0xa9027bfd, 0xf9001bf7, 0xa90457f6, 0xa9054ff4}},
        Check{address::BuildMenu, {0xa9be7bfd, 0xa9014ff4, 0x910003fd, 0xf9400028, 0xaa0103e0}},
        Check{address::SetGridState, {0xa9be7bfd, 0xa9014ff4, 0x910003fd, 0xb9400808, 0x2a0103f3}},
        Check{address::ChangeGridPauseMode, {0xd101c3ff, 0xa9017bfd, 0xf90013fb, 0xa90367fa, 0xa9045ff8}},
        Check{address::DisappearNameWindow, {0x39480008, 0x52800089, 0x8b080808, 0xb901f109, 0xd65f03c0}}};
    for (const auto &check : checks) {
        if (std::memcmp(reinterpret_cast<const void *>(game::MainAddress(check.offset)),
                        check.words.data(), sizeof(check.words))) {
            Logging.Error("[QoL Enhancements/Pockets] CLAIM_DISABLED code mismatch at 0x%lx\n", check.offset);
            return false;
        }
    }

    qol::core::HookPlan hookPlan{
        qol::core::Site<EnterRecyclingMenu>("EnterRecyclingMenu", address::EnterMenu, {0xd10103ff, 0xa9027bfd, 0xa9034ff4, 0x910083fd, 0xf9410808}, true),
        qol::core::Site<UpdateRecyclingMenu>("UpdateRecyclingMenu", address::ProcessSelection, {0xd10183ff, 0xa9027bfd, 0xf9001bf7, 0xa90457f6, 0xa9054ff4}, true),
    };
    if (!hookPlan) return false;
    chooseClaim = choose;
    if (!hookPlan.Publish<EnterRecyclingMenu>(address::EnterMenu)) return false;
    if (!hookPlan.Publish<UpdateRecyclingMenu>(address::ProcessSelection)) return false;
    Logging.Debug("[QoL Enhancements/Pockets] CLAIM_READY single-action recycling box bypass\n");
    return true;
}
} // namespace qol_pockets::features
