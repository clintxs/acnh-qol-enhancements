#include "program/core/hook_plan.hpp"
#include "lib.hpp"
#include "settings.hpp"
#include "program/core/page_input.hpp"
#include <array>
#include <cstring>

namespace qol::crafting {
namespace {
namespace address {
constexpr std::uintptr_t ResetGrid = 0x03764290, MoveGrid = 0x0375fd8c;
constexpr std::uintptr_t WorkbenchResetReturn = 0x0346b028, KitchenResetReturn = 0x02de8bf8;
constexpr std::uintptr_t WorkbenchQuantityPage = 0x04a3a7d8;
constexpr std::uintptr_t KitchenQuantityPage = 0x049de8f0;
constexpr std::uintptr_t WorkbenchRowBinder = 0x03469b50, KitchenRowBinder = 0x02de7730;
constexpr std::uintptr_t Application = 0x059f83f0;
}
namespace field {
constexpr std::size_t SequencePage = 0x230;
constexpr std::size_t PageInput = 0x210, QuantityGrid = 0x578;
constexpr std::size_t GridState = 0x298, Resident = 0x2e0, Columns = 0x2e8;
constexpr std::size_t VisibleRows = 0x2ec, ItemCount = 0x3cc, GridFlags = 0x3e8;
constexpr std::size_t MoveKind = 0x3ec, MoveDirection = 0x3f0, MoveDelta = 0x3f4;
constexpr std::size_t AlternateMove = 0x3f8;
constexpr std::size_t BindingOwner = 0x338, BindingCallback = 0x340, BindingAdjustment = 0x348;
constexpr std::size_t RowPool = 0x210, ActiveRows = 0x2f8;
constexpr std::size_t LastTopRow = 0x434, SelectedRow = 0x440;
}
std::uintptr_t Main{};
template<class T> T &At(void *p, std::size_t offset = 0) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(p) + offset);
}
template<class T> const T &At(const void *p, std::size_t offset = 0) {
    return *reinterpret_cast<const T *>(static_cast<const std::byte *>(p) + offset);
}
template<class F> F Native(std::uintptr_t offset) { return reinterpret_cast<F>(Main + offset); }
struct InputState {
    void *page{}, *grid{};
    bool armed{}, wrapPaused{};
    std::uint64_t pauseStarted{};
};
constexpr float StickPressThreshold = 0.5f, StickReleaseThreshold = 0.15f;
// Horizon's system timer runs at 19.2 MHz (nn::os::detail::TickManagerImpl).
constexpr std::uint64_t WrapPauseTicks = 19'200'000 * 3 / 10; // 300 ms, independent of UI frame rate.
std::array<InputState, 4> Inputs{};
unsigned NextSlot{};
unsigned WrapReports{};
InputState &ForPage(void *page, void *grid) {
    for (auto &state : Inputs) if (state.page == page && state.grid == grid) return state;
    auto &state = Inputs[NextSlot++ % Inputs.size()];
    state = {page, grid, false}; // Opening with a held direction cannot wrap.
    return state;
}
constexpr unsigned LoopRows = 2;
void BindCyclicQuantityRows(void *sequence, const void *request) {
    auto page = At<void *>(sequence, field::SequencePage);
    auto grid = At<void *>(page, field::QuantityGrid);
    const auto binder = At<std::uintptr_t>(page) == Main + address::KitchenQuantityPage
        ? address::KitchenRowBinder : address::WorkbenchRowBinder;
    auto bind = Native<void (*)(void *, const void *)>(binder);
    const int count = At<int>(grid, field::ItemCount);
    if (!request || At<void *>(request) != grid || count < 2 || count > 10 ||
        !(At<unsigned char>(grid, field::GridFlags) & LoopRows)) {
        bind(sequence, request);
        return;
    }
    // The native binder corrects only one circular lap. A small material limit
    // can be shorter than the padded pool. Bind each pooled button with its
    // normalized source index, retaining native number formatting/material
    // styling. Both native binders use +2f8/+210 only to enumerate buttons;
    // their synchronous row setters do not reenter the grid.
    const unsigned rowCount = At<unsigned>(grid, field::ActiveRows);
    auto rows = At<void **>(grid, field::RowPool);
    const int first = At<int>(request, 0x0c);
    if (first >= 0 && first + static_cast<int>(rowCount) <= count * 2) {
        bind(sequence, request);
        return;
    }
    std::array<std::byte, 0x18> binding;
    std::memcpy(binding.data(), request, binding.size());
    At<unsigned>(grid, field::ActiveRows) = 1;
    for (unsigned i = 0; i < rowCount; ++i) {
        At<void **>(grid, field::RowPool) = rows + i;
        At<int>(binding.data(), 0x0c) = ((first + static_cast<int>(i)) % count + count) % count;
        bind(sequence, binding.data());
    }
    At<void **>(grid, field::RowPool) = rows;
    At<unsigned>(grid, field::ActiveRows) = rowCount;
}
HOOK_DEFINE_TRAMPOLINE(ResetQuantityGrid) {
    static void Callback(void *grid, const void *request) {
        // Native cAppear (0346AEC8 / 02DE8A98) resets the list on each open.
        // Disarm a reused page/grid as well as freshly allocated instances.
        for (auto &state : Inputs) if (state.grid == grid) {
            state.armed = state.wrapPaused = false;
            state.pauseStarted = 0;
        }
        const auto caller = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
        // These two cAppear call sites belong only to the native quantity
        // selectors. Prepare cyclic neighbor binding during their normal open,
        // before any number is displayed; unrelated lists keep their request.
        if (request && (caller == Main + address::WorkbenchResetReturn ||
                        caller == Main + address::KitchenResetReturn) &&
            At<unsigned>(grid, field::Columns) == 1 &&
            At<unsigned>(grid, field::VisibleRows) == 1 &&
            At<unsigned>(request, 8) >= 2 &&
            At<unsigned>(request, 8) <= 10 && At<void *>(grid, field::BindingOwner) &&
            At<std::uintptr_t>(grid, field::BindingAdjustment) == 0 &&
            (At<std::uintptr_t>(grid, field::BindingCallback) == Main + address::WorkbenchRowBinder ||
             At<std::uintptr_t>(grid, field::BindingCallback) == Main + address::KitchenRowBinder ||
             At<std::uintptr_t>(grid, field::BindingCallback) == reinterpret_cast<std::uintptr_t>(BindCyclicQuantityRows))) {
            At<std::uintptr_t>(grid, field::BindingCallback) = reinterpret_cast<std::uintptr_t>(BindCyclicQuantityRows);
            std::array<std::byte, 0x30> cyclicRequest;
            std::memcpy(cyclicRequest.data(), request, cyclicRequest.size());
            cyclicRequest[0x17] = std::byte{1}; // Native GridRequest::loop.
            Orig(grid, cyclicRequest.data());
        } else {
            Orig(grid, request);
        }
    }
};
void AnimateWrap(InputState &state, bool up) {
    auto grid = state.grid;
    // 03762978 queues these fields for an ordinary vertical press. Let the
    // same 0375FD8C path select, bind and animate across the cyclic boundary;
    // it also calls 03765950's native cScroll feedback and direction selector.
    // The quantity layout has one visible row (LBtnListNumInput), so its last
    // top row is count-1. No reset/reopen or nine-row jump is needed.
    At<unsigned>(grid, field::MoveKind) = 2;
    At<unsigned>(grid, field::MoveDirection) = up ? 0 : 1;
    At<int>(grid, field::MoveDelta) = up ? -1 : 1;
    At<unsigned char>(grid, field::AlternateMove) = 0;
    Native<void (*)(void *)>(address::MoveGrid)(grid);
    // Native input/update will still run after this subscriber; do not leave
    // a queued second movement behind. Animation state remains native-owned.
    At<unsigned>(grid, field::MoveKind) = 0;
}
void PauseVerticalInput(void *row) {
    // Present neutral input during the pause. Merely consuming these paths
    // puts 03C4B20C / 03C48C70's repeat timers into a release-required state,
    // preventing a still-held control from restarting when the pause expires.
    // A/B and other buttons remain native. Each update gets a fresh snapshot.
    At<unsigned>(row) &= ~3u;
    At<float>(row, 4) = At<float>(row, 8) = 0;
}
bool QuantityInput(void *page, void *snapshot, unsigned blocked) {
    if (!page || (At<std::uintptr_t>(page) != Main + address::WorkbenchQuantityPage &&
                  At<std::uintptr_t>(page) != Main + address::KitchenQuantityPage)) return false;
    auto grid = At<void *>(page, field::QuantityGrid);
    if (!grid) return false;
    auto &state = ForPage(page, grid);
    const auto resident = At<unsigned>(grid, field::Resident);
    const auto count = At<unsigned>(grid, field::ItemCount);
    if (!snapshot || resident >= 8 || At<unsigned>(snapshot, 8) > 8 ||
        resident >= At<unsigned>(snapshot, 8) || !At<void *>(snapshot, 0x10) ||
        At<unsigned>(grid, field::GridState) != 2 || At<int>(grid, field::Columns) != 1 ||
        count < 2 || count > 10) {
        state.armed = state.wrapPaused = false;
        return false;
    }
    auto row = At<std::byte *>(snapshot, 0x10) + resident * 0x48;
    const auto buttons = At<unsigned>(row), consumed = At<unsigned>(row, 0x3c);
    const float x = At<float>(row, 4), y = At<float>(row, 8);
    const bool validStick = x >= -1 && x <= 1 && y >= -1 && y <= 1;
    const bool stickUp = validStick && y > StickPressThreshold;
    const bool stickDown = validStick && y < -StickPressThreshold;
    const bool up = (buttons & 1) || (stickUp && y*y > x*x);
    const bool down = (buttons & 2) || (stickDown && y*y > x*x);
    auto input = At<void *>(page, field::PageInput);
    bool unavailable = (blocked & 1) || !input || At<unsigned char>(input, 0x54);
    if (!unavailable && At<unsigned>(input, 0x58)) {
        auto app = *Native<void **>(address::Application);
        auto masks = app ? At<void *>(app, 0x418) : nullptr;
        unavailable = !masks || (At<unsigned>(input, 0x58) & At<unsigned>(masks, 0x10));
    }
    const bool fresh = state.armed;
    // Releasing means returning the stick to center, not merely easing below
    // the press threshold. Pressure changes/diagonals must not rearm a hold.
    const bool released = !unavailable && validStick && !(buttons & 3) &&
                          x*x + y*y <= StickReleaseThreshold*StickReleaseThreshold;
    if (released) {
        state.armed = true;
        state.wrapPaused = false;
    } else if (unavailable || !validStick || up || down ||
               x > StickPressThreshold || x < -StickPressThreshold || (buttons & 0x30)) {
        state.armed = false;
    }
    // Preserve an armed latch while traveling from center to the press
    // threshold. Once pressed, only the smaller release zone can rearm it.
    if (state.wrapPaused) {
        if (svcGetSystemTick() - state.pauseStarted < WrapPauseTicks)
            PauseVerticalInput(row);
        else
            state.wrapPaused = false;
        // After 300 ms, hand the still-held direction back to native repeat.
        // Keep the wrap latch disarmed so the next boundary still stops.
        return false;
    }
    const auto selected = At<unsigned>(grid, field::SelectedRow);
    if (up && down) {
        PauseVerticalInput(row);
        return false;
    }
    if (selected >= count || up == down || (up ? selected != 0 : selected != count-1)) return false;
    // Native cyclic binding supplies the transition, while our gesture latch
    // gates endpoint motion. A held input arriving from inside the range must
    // stay clamped, even after a wrap's pause expires or input is unavailable.
    if (!fresh || unavailable || !validStick || (buttons & 0x30) ||
        (consumed & (buttons & 3)) || ((stickUp || stickDown) && (At<unsigned>(row, 0x40) & 1)) ||
        At<unsigned>(grid, field::VisibleRows) != 1 ||
        At<unsigned>(grid, field::LastTopRow) != count-1 ||
        !(At<unsigned char>(grid, field::GridFlags) & LoopRows)) {
        PauseVerticalInput(row);
        return false;
    }
    const unsigned target = up ? count-1 : 0;
    AnimateWrap(state, up);
    state.armed = false;
    state.wrapPaused = true;
    state.pauseStarted = svcGetSystemTick();
    PauseVerticalInput(row);
    if (WrapReports++ < 4)
        Logging.Debug("[QoL Enhancements/Crafting] quantity wrap: selected=%u maximum=%u resident=%u\n",
                    count-target, count, resident);
    return false; // A/B, horizontal input and ordinary repeat remain native.
}
core::InputOutcome QuantityPageInput(const core::PageInputContext &input) {
    return QuantityInput(input.page, input.snapshot, input.blocked) ? core::InputOutcome::Consume : core::InputOutcome::PassThrough;
}

}
bool InstallCraftingQuantityWrap() {
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    constexpr Fingerprint fingerprints[]{
        {address::ResetGrid, {0xd10203ff,0xfd001be8,0xf90027f9,0xa903fbfd,0xa9055ff8}},
        {address::WorkbenchRowBinder, {0xd10283ff,0xfd0013ea,0x6d0323e9,0xa9047bfd,0xa9056ffc}},
        {address::KitchenRowBinder, {0xd10283ff,0xfd0013ea,0x6d0323e9,0xa9047bfd,0xa9056ffc}},
        {address::MoveGrid, {0xd10343ff,0xa9087bfd,0xf9004bf9,0xa90a5ff8,0xa90b57f6}},
        {address::KitchenResetReturn - 4, {0x9425eda7,0xf9411a89,0xf9400128,0xaa0903e0,0x2a1f03e1}},
        {address::WorkbenchResetReturn - 4, {0x940be49b,0xf9411a69,0xf9400128,0xaa0903e0,0x2a1f03e1}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &f : fingerprints) if (f.offset + sizeof(f.words) > main.m_Text.m_Size ||
        std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + f.offset), f.words, sizeof(f.words))) {
        Logging.Error("[QoL Enhancements/Crafting] quantity wrap: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
        return false;
    }
    Main = main.m_Text.m_Start;

    qol::core::HookPlan hookPlan{
        qol::core::Site<ResetQuantityGrid>("ResetQuantityGrid", address::ResetGrid, {0xd10203ff, 0xfd001be8, 0xf90027f9, 0xa903fbfd, 0xa9055ff8}, true),
    };
    if (!hookPlan) return false;
    if (!core::RegisterPageInput<QuantityPageInput>(core::InputPriority::QuantityWrap)) {
        Logging.Error("[QoL Enhancements/Crafting] quantity wrap: DISABLED; shared input unavailable\n");
        return false;
    }
    if (!hookPlan.Publish<ResetQuantityGrid>(address::ResetGrid)) return false;
    Logging.Debug("[QoL Enhancements/Crafting] quantity wrap: READY; native slide/sound; crafting/cooking limits; release before endpoint wrap\n");
    return true;
}
}
