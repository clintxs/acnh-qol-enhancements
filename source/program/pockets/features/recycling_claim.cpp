#include "recycling_claim.hpp"
#include "program/pockets/game/stack_bindings.hpp"
#include <array>
#include <cstring>

namespace qol_pockets::features {
namespace {
namespace address {
constexpr std::uintptr_t EnterMenu = 0x02df1c58, ProcessSelection = 0x02df1d68;
constexpr std::uintptr_t BuildMenu = 0x02defbfc;
constexpr std::uintptr_t ControllerVtable = 0x049df828, BuilderVtable = 0x049dfba8;
constexpr std::uintptr_t DescriptionVtable = 0x049c7208, EmptyCallbackVtable = 0x049c7230;
}
namespace layout {
constexpr std::size_t State = 0x1b0, Queues = 0x1f0, Phase = 0x200;
constexpr std::size_t SelectedSlot = 0x218, Menu = 0x228;
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

bool ProbeClaim(void *owner, ClaimRequest &request, bool entering = false) {
    if (Field<std::uintptr_t>(owner, 0) != game::MainAddress(address::ControllerVtable) ||
        Field<int>(owner, layout::State) != SelectingCommand) return false;
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
    const auto &row = description.rows[0];
    const auto &callback = description.callbacks[0];
    if (description.count != 1 || row.action != ClaimAction || row.flags || row.extra ||
        !row.bank || !row.key || std::strcmp(row.bank, "CmnSubWin") || std::strcmp(row.key, "1602") ||
        callback.vtable != game::MainAddress(address::EmptyCallbackVtable) ||
        callback.fields[0] || callback.fields[1] || callback.fields[2]) return false;
    request.owner = owner;
    request.slot = slot;
    std::memcpy(request.item.data(), items + slot * 12, request.item.size());
    return true;
}

HOOK_DEFINE_TRAMPOLINE(EnterRecyclingMenu) {
    static void Callback(void *owner) {
        pending = {};
        if (!ProbeClaim(owner, pending, true)) Orig(owner);
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
        Field<int>(menu, layout::State) = oldState;
        Field<int>(menu, layout::RowCount) = oldCount;
        Field<int>(menu, layout::FirstAction) = oldAction;
        Field<std::uint8_t>(menu, layout::Selection) = 0xff;
    }
};
} // namespace

bool InstallRecyclingClaim() {
    struct Check { std::uintptr_t offset; std::array<std::uint32_t, 5> words; };
    constexpr std::array checks{
        Check{address::EnterMenu, {0xd10103ff, 0xa9027bfd, 0xa9034ff4, 0x910083fd, 0xf9410808}},
        Check{address::ProcessSelection, {0xd10183ff, 0xa9027bfd, 0xf9001bf7, 0xa90457f6, 0xa9054ff4}},
        Check{address::BuildMenu, {0xa9be7bfd, 0xa9014ff4, 0x910003fd, 0xf9400028, 0xaa0103e0}}};
    for (const auto &check : checks) {
        if (std::memcmp(reinterpret_cast<const void *>(game::MainAddress(check.offset)),
                        check.words.data(), sizeof(check.words))) {
            Logging.Error("[QoL Enhancements/Pockets] CLAIM_DISABLED code mismatch at 0x%lx\n", check.offset);
            return false;
        }
    }
    EnterRecyclingMenu::InstallAtOffset(address::EnterMenu);
    UpdateRecyclingMenu::InstallAtOffset(address::ProcessSelection);
    Logging.Debug("[QoL Enhancements/Pockets] CLAIM_READY single-action recycling box bypass\n");
    return true;
}
} // namespace qol_pockets::features
