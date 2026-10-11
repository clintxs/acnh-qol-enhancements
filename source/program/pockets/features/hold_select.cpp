#include "hold_select.hpp"
#include "pocket_input.hpp"
#include "program/core/page_input.hpp"
#include "program/pockets/game/stack_bindings.hpp"
#include <array>
#include <cstring>

namespace qol_pockets::features {
namespace {
namespace address {
constexpr std::uintptr_t PocketPageVtable = 0x04a06b00;
constexpr std::uintptr_t ApplicationGlobal = 0x059f83f0;
}
namespace layout {
constexpr std::size_t Page = 0x210, Mode = 0x1c4fc, State = 0x1b0;
constexpr std::size_t TransitionA = 0x1f0, TransitionB = 0x1f4;
constexpr std::size_t SelectionLimit = 0x1c4f8, SelectedSlots = 0x1c4e8;
constexpr std::size_t BlockedSlots = 0x1c4f0, ClickSlot = 0x1c518, LogicalInput = 0x1c51a;
constexpr std::size_t BackHint = 0x570, HintInput = 0xa0, ButtonId = 0x14, InputRow = 0x10;
constexpr std::size_t RowCount = 8, Rows = 0x10, RowBytes = 0x48, Consumed = 0x3c;
constexpr std::size_t InputContext = 0x210, InputBlocked = 0x54, InputMask = 0x58;
constexpr std::size_t GlobalInputMasks = 0x418, GlobalInputMask = 0x10;
constexpr std::size_t ItemWidgets = 0x590, ItemFlags = 0x2c3;
}
constexpr unsigned NativeA = 1u << 4;
constexpr unsigned OtherActionButtons = (1u << 5) | (1u << 6) | (1u << 7) | // B/X/Y
                                        (1u << 8) | (1u << 9) |          // L/R
                                        (1u << 10) | (1u << 11);         // ZL/ZR
// UI rows use the footer's button IDs, not HID bit positions. Live input also
// carries bit 16 alongside ordinary A/navigation; it is not a trigger action.
constexpr std::uint16_t SelectItem = 1u << 2;
constexpr int Browsing = 3, ItemSelectMode = 1, PutInStorageMode = 9;
template <typename T> T &Field(void *object, std::size_t offset) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(object) + offset);
}
struct HoldGesture {
    void *page{}, *owner{};
    int row{-1}, mode{};
    bool released{}, holding{};
};
HoldGesture gesture{};
struct InputCapture {
    void *owner{};
    int row{}, previousSlot{};
};
InputCapture capture{};

// Temporary, bounded diagnostics while verifying the live fix. These use
// the base logger so a user's log_debug=false does not hide the failed gate.
void TraceHoldInput(void *page, void *snapshot, unsigned blocked, bool after) {
    static unsigned records{};
    static std::array<std::uintptr_t, 8> seenPages{};
    if (!page || !snapshot || records >= 160) return;
    const auto vtable = Field<std::uintptr_t>(page, 0);
    if (vtable != game::MainAddress(address::PocketPageVtable)) {
        if (after) return;
        for (auto &seen : seenPages) {
            if (seen == vtable) return;
            if (!seen) {
                seen = vtable;
                Logging.Log("[QoL Enhancements/Pockets] HOLD_SELECT_TRACE dispatch page_vt=%lx\n",
                            vtable - game::MainAddress(0));
                return;
            }
        }
        return;
    }
    auto *owner = pocket_input::FindPocketController(page);
    auto *hint = Field<void *>(page, layout::BackHint);
    auto *unit = hint ? Field<void *>(hint, layout::HintInput) : nullptr;
    const int row = unit ? Field<int>(unit, layout::InputRow) : -1;
    const int count = Field<int>(snapshot, layout::RowCount);
    auto *rows = Field<std::byte *>(snapshot, layout::Rows);
    auto *input = rows && count > 0 && count <= 8 && row >= 0 && row < count ?
        rows + row * layout::RowBytes : nullptr;
    auto *context = Field<void *>(page, layout::InputContext);
    auto *app = *reinterpret_cast<void **>(game::MainAddress(address::ApplicationGlobal));
    auto *masks = app ? Field<void *>(app, layout::GlobalInputMasks) : nullptr;
    const std::array<long, 20> values{
        owner != nullptr, owner ? Field<int>(owner, layout::Mode) : -1,
        owner ? Field<int>(owner, layout::State) : -1,
        owner ? Field<int>(owner, layout::SelectionLimit) : -2,
        owner ? Field<int>(owner, layout::TransitionA) : -2,
        owner ? Field<int>(owner, layout::TransitionB) : -2,
        unit ? Field<int>(unit, layout::ButtonId) : -1, row, count,
        input ? Field<unsigned>(input, 0) : 0,
        input ? Field<unsigned>(input, layout::Consumed) : 0, blocked,
        context ? Field<std::uint8_t>(context, layout::InputBlocked) : -1,
        context ? Field<unsigned>(context, layout::InputMask) : 0,
        masks ? Field<unsigned>(masks, layout::GlobalInputMask) : 0,
        row >= 0 && row < 8 ? pocket_input::FindFocusedItemSlot(page, row) : -1,
        owner ? Field<std::uint16_t>(owner, layout::LogicalInput) : -1,
        owner ? Field<std::int8_t>(owner, layout::ClickSlot) : -1,
        gesture.released, gesture.holding};
    static std::array<std::array<long, 20>, 2> previous{};
    if (values == previous[after]) return;
    previous[after] = values;
    ++records;
    Logging.Log("[QoL Enhancements/Pockets] HOLD_SELECT_TRACE %s owner=%ld mode=%ld state=%ld "
                "limit=%ld queues=%ld,%ld hint=%ld row=%ld/%ld buttons=%lx consumed=%lx "
                "blocked=%ld local=%ld mask=%lx global=%lx focus=%ld logical=%lx click=%ld "
                "released=%ld holding=%ld\n", after ? "after" : "before",
                values[0], values[1], values[2], values[3], values[4], values[5], values[6],
                values[7], values[8], values[9], values[10], values[11], values[12], values[13],
                values[14], values[15], values[16], values[17], values[18], values[19]);
}

bool IsMultiSelection(void *owner) {
    const int mode = Field<int>(owner, layout::Mode);
    const int limit = Field<int>(owner, layout::SelectionLimit);
    // 00d0fd10 -> 02d1e950 -> 033598a0: cItemSelect is mode 1 and
    // cPutInStorage is mode 9. Do not enable ordinary pockets or single picks.
    return (mode == ItemSelectMode || mode == PutInStorageMode) &&
           (limit == -1 || limit > 1) && Field<int>(owner, layout::State) == Browsing &&
           Field<int>(owner, layout::TransitionA) == -1 &&
           Field<int>(owner, layout::TransitionB) == -1;
}

void BeforeHoldSelectInput(void *page, void *snapshot, unsigned blocked) {
    TraceHoldInput(page, snapshot, blocked, false);
    capture = {};
    if (!page || !snapshot ||
        Field<std::uintptr_t>(page, 0) != game::MainAddress(address::PocketPageVtable)) {
        // Other pages also receive input in this frame. Their dispatch must
        // not interrupt a pockets gesture; Play entry handles reopening.
        return;
    }
    auto *owner = pocket_input::FindPocketController(page);
    auto *hint = Field<void *>(page, layout::BackHint);
    auto *unit = hint ? Field<void *>(hint, layout::HintInput) : nullptr;
    if (!owner || !IsMultiSelection(owner) || !unit || Field<int>(unit, layout::ButtonId) != 5) {
        gesture = {};
        return;
    }
    const int row = Field<int>(unit, layout::InputRow);
    const int count = Field<int>(snapshot, layout::RowCount);
    auto *rows = Field<std::byte *>(snapshot, layout::Rows);
    if (!rows || count < 1 || count > 8 || row < 0 || row >= count) {
        gesture = {};
        return;
    }
    const int mode = Field<int>(owner, layout::Mode);
    if (gesture.page != page || gesture.owner != owner || gesture.row != row || gesture.mode != mode)
        gesture = {page, owner, row, mode, false, false};
    auto *input = rows + row * layout::RowBytes;
    const unsigned buttons = Field<unsigned>(input, 0);
    auto *context = Field<void *>(page, layout::InputContext);
    auto *app = *reinterpret_cast<void **>(game::MainAddress(address::ApplicationGlobal));
    auto *masks = app ? Field<void *>(app, layout::GlobalInputMasks) : nullptr;
    if (!context || !masks || (blocked & 1) || Field<std::uint8_t>(context, layout::InputBlocked) ||
        (Field<unsigned>(context, layout::InputMask) & Field<unsigned>(masks, layout::GlobalInputMask)) ||
        (Field<unsigned>(input, layout::Consumed) & NativeA) || (buttons & OtherActionButtons)) {
        gesture.released = gesture.holding = false;
        return;
    }
    if (!(buttons & NativeA)) {
        gesture.released = true;
        gesture.holding = false;
        return;
    }
    // An A press carried into a new/blocked picker must be released first.
    const int slot = pocket_input::FindFocusedItemSlot(page, row);
    if (!gesture.released || slot < 0) return;
    gesture.holding = true;
    capture = {owner, row, slot};
}

void AfterHoldSelectInput(void *page, void *snapshot, unsigned blocked) {
    TraceHoldInput(page, snapshot, blocked, true);
    const auto input = capture;
    capture = {};
    if (!input.owner || !gesture.holding) return;
    auto *owner = pocket_input::FindPocketController(page);
    if (owner != input.owner || !IsMultiSelection(owner) ||
        Field<int>(owner, layout::Mode) != gesture.mode) {
        gesture = {};
        return;
    }
    const int slot = pocket_input::FindFocusedItemSlot(page, input.row);
    if (slot < 0 || slot == input.previousSlot) return;
    const auto bit = std::uint64_t{1} << slot;
    if ((Field<std::uint64_t>(owner, layout::SelectedSlots) & bit) ||
        (Field<std::uint64_t>(owner, layout::BlockedSlots) & bit) ||
        Field<std::uint16_t>(owner, layout::LogicalInput) != 0) return;
    auto *widget = Field<void *>(page, layout::ItemWidgets + sizeof(void *) * slot);
    if (!widget || !(Field<std::uint8_t>(widget, layout::ItemFlags) & 1)) return;
    // Mirror native click callback 030884b0. The browsing update 0308b3f0
    // performs the toggle, selection-limit checks, storage-capacity checks and
    // feedback. Never toggle a selected item or overwrite a real A request.
    Field<std::int8_t>(owner, layout::ClickSlot) = slot;
    Field<std::uint16_t>(owner, layout::LogicalInput) |= SelectItem;
}
}

void ResetHoldSelect(void *owner) {
    if (gesture.owner == owner) gesture = {};
    if (capture.owner == owner) capture = {};
}

bool InstallHoldSelect() {
    // Besides the shared input hook, verify the native request producer and
    // consumer whose field semantics this feature relies on.
    struct Check { std::uintptr_t offset; std::array<std::uint32_t, 5> words; };
    constexpr std::array checks{
        Check{0x030884b0, {0xd100c3ff, 0xa9017bfd, 0xa9024ff4, 0x910043fd, 0xf941080e}},
        Check{0x0308b3f0, {0xd10283ff, 0xfd001be8, 0xa9047bfd, 0xa9056ffc, 0xa90667fa}}};
    for (const auto &check : checks) {
        if (std::memcmp(reinterpret_cast<const void *>(game::MainAddress(check.offset)),
                        check.words.data(), sizeof(check.words)) != 0) {
            Logging.Error("[QoL Enhancements/Pockets] HOLD_SELECT_DISABLED fingerprint at %lx\n", check.offset);
            return false;
        }
    }
    if (!qol::core::RegisterPageInputObserver(BeforeHoldSelectInput, AfterHoldSelectInput)) {
        Logging.Error("[QoL Enhancements/Pockets] HOLD_SELECT_DISABLED shared input unavailable\n");
        return false;
    }
    Logging.Log("[QoL Enhancements/Pockets] HOLD_SELECT_READY test3 corrected UI action mask; selling / put in storage\n");
    return true;
}
}
