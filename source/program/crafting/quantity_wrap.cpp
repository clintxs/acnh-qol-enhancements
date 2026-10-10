#include "lib.hpp"
#include "settings.hpp"
#include "program/core/page_input.hpp"
#include <algorithm>
#include <array>
#include <cstring>

namespace qol::crafting {
namespace {
namespace address {
constexpr std::uintptr_t ResetGrid = 0x03764290, ActivateGrid = 0x0335b4a0;
constexpr std::uintptr_t SelectGridRow = 0x03765674, FocusGrid = 0x03763e14;
constexpr std::uintptr_t GridRequestVtable = 0x04a2b020;
constexpr std::uintptr_t WorkbenchQuantityPage = 0x04a3a7d8;
constexpr std::uintptr_t KitchenQuantityPage = 0x049de8f0;
constexpr std::uintptr_t Application = 0x059f83f0;
}
namespace field {
constexpr std::size_t PageInput = 0x210, QuantityGrid = 0x578;
constexpr std::size_t GridState = 0x298, Resident = 0x2e0, Columns = 0x2e8;
constexpr std::size_t VisibleItems = 0x2f4, ItemCount = 0x3cc;
constexpr std::size_t TopRow = 0x430, SelectedRow = 0x440, GridFlags = 0x3e8;
}
std::uintptr_t Main{};
template<class T> T &At(void *p, std::size_t offset = 0) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(p) + offset);
}
template<class F> F Native(std::uintptr_t offset) { return reinterpret_cast<F>(Main + offset); }
struct InputState {
    void *page{}, *grid{};
    bool armed{}, wrapPaused{};
    std::uint64_t pauseStarted{};
};
constexpr float StickPressThreshold = 0.5f, StickReleaseThreshold = 0.15f;
// Horizon's system timer runs at 19.2 MHz (nn::os::detail::TickManagerImpl).
constexpr std::uint64_t WrapPauseTicks = 19'200'000 / 2; // 500 ms, independent of UI frame rate.
std::array<InputState, 4> Inputs{};
unsigned NextSlot{};
unsigned WrapReports{};
InputState &ForPage(void *page, void *grid) {
    for (auto &state : Inputs) if (state.page == page && state.grid == grid) return state;
    auto &state = Inputs[NextSlot++ % Inputs.size()];
    state = {page, grid, false}; // Opening with a held direction cannot wrap.
    return state;
}
HOOK_DEFINE_TRAMPOLINE(ResetQuantityGrid) {
    static void Callback(void *grid, const void *request) {
        // Native cAppear (0346AEC8 / 02DE8A98) resets the list on each open.
        // Disarm a reused page/grid as well as freshly allocated instances.
        for (auto &state : Inputs) if (state.grid == grid) {
            state.armed = state.wrapPaused = false;
            state.pauseStarted = 0;
        }
        Orig(grid, request);
    }
};
// Same native grid request ABI as cAppear. Quantity rows run in reverse:
// row count-1 is quantity 1, row 0 is the largest native selectable quantity.
struct GridRequest {
    std::uintptr_t vtable{};
    int count{}, first{}, selected{};
    unsigned char padMinimum{}, padRow{}, enabled{1}, loop{}, reverse{1}, alternateAxis{1};
    unsigned char cursor{}, animate{}, flags{}, callback{};
    unsigned short padding{};
    int pageRows{}, exitFrames{12}, entryFrames{6};
    unsigned resident{};
};
static_assert(sizeof(GridRequest) == 0x30 && offsetof(GridRequest, resident) == 0x2c);
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
        // After 500 ms, hand the still-held direction back to native repeat.
        // Keep the wrap latch disarmed so the next boundary still stops.
        return false;
    }
    if (!fresh || unavailable || up == down || (buttons & 0x30) ||
        (consumed & (buttons & 3)) || ((stickUp || stickDown) && (At<unsigned>(row, 0x40) & 1))) return false;
    const auto selected = At<unsigned>(grid, field::SelectedRow);
    if (selected >= count || (up ? selected != 0 : selected != count-1)) return false;
    const auto visible = At<int>(grid, field::VisibleItems);
    if (visible < 1 || visible > 64) return false;
    const unsigned target = up ? count-1 : 0;
    GridRequest request;
    request.vtable = Main + address::GridRequestVtable;
    request.count = count; request.selected = target;
    request.first = std::clamp(At<int>(grid, field::TopRow),
                             std::max(0, int(target) - visible + 1), int(target));
    request.resident = resident;
    // Keep native row binding, quantity/material callbacks, focus and cursor.
    // Do not enable the grid's continuous-loop flag: held input stays clamped.
    Native<void (*)(void *, const GridRequest *)>(address::ResetGrid)(grid, &request);
    Native<void (*)(void *, void *)>(address::ActivateGrid)(grid, nullptr);
    Native<void (*)(void *, unsigned)>(address::SelectGridRow)(grid, target);
    Native<void (*)(void *, bool)>(address::FocusGrid)(grid, false);
    state.armed = false;
    state.wrapPaused = true;
    state.pauseStarted = svcGetSystemTick();
    PauseVerticalInput(row);
    if (WrapReports++ < 4)
        Logging.Log("[QoL Enhancements/Crafting] quantity wrap: selected=%u maximum=%u resident=%u\n",
                    count-target, count, resident);
    return false; // A/B, horizontal input and ordinary repeat remain native.
}
}
bool InstallCraftingQuantityWrap() {
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    constexpr Fingerprint fingerprints[]{
        {address::ResetGrid, {0xd10203ff,0xfd001be8,0xf90027f9,0xa903fbfd,0xa9055ff8}},
        {address::ActivateGrid, {0xb40003a0,0xb9429808,0x7100091f,0x54000063,0xb4000341}},
        {address::SelectGridRow, {0xd101c3ff,0xa9027bfd,0xa90367fa,0xa9045ff8,0xa90557f6}},
        {address::FocusGrid, {0xf9416c08,0xf100011f,0xb9429809,0x7a421920,0x54000d61}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &f : fingerprints) if (f.offset + sizeof(f.words) > main.m_Text.m_Size ||
        std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + f.offset), f.words, sizeof(f.words))) {
        Logging.Log("[QoL Enhancements/Crafting] quantity wrap: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
        return false;
    }
    Main = main.m_Text.m_Start;
    if (!core::RegisterPageInput(QuantityInput)) {
        Logging.Log("[QoL Enhancements/Crafting] quantity wrap: DISABLED; shared input unavailable\n");
        return false;
    }
    ResetQuantityGrid::InstallAtOffset(address::ResetGrid);
    Logging.Log("[QoL Enhancements/Crafting] quantity wrap: READY; native crafting/cooking limits; release before endpoint wrap\n");
    return true;
}
}
