#include "program/core/hook_plan.hpp"
#include "stack_splitting.hpp"
#include "stack_menu.hpp"
#include "menu_text.hpp"
#include "program/pockets/game/stack_bindings.hpp"
#include <array>
#include <cstring>

extern "C" int qol_pockets_invoke_split(void *, int, std::uintptr_t);
extern "C" const char qol_pockets_split_return[];

namespace qol_pockets::features {
namespace {
namespace address = game::stack_offsets;
namespace frame_layout {
constexpr std::size_t DispatcherCallbackToken = 0x1c;
constexpr std::size_t SavedReturnAddress = 8;
constexpr std::size_t WrapperQuantity = 0x50;
} // namespace frame_layout
// Pipeline: native menu -> private action ID -> private callback token ->
// wrapper around native Split. Only calls from our wrapper alter the quantity.
constexpr int GrabFiveCallback = static_cast<int>(CallbackId::GrabFive);
constexpr int GrabTenCallback = static_cast<int>(CallbackId::GrabTen);

int SplitFive(void *controller) {
    return qol_pockets_invoke_split(controller, 5, game::MainAddress(address::Split));
}
int SplitTen(void *controller) {
    return qol_pockets_invoke_split(controller, 10, game::MainAddress(address::Split));
}

HOOK_DEFINE_TRAMPOLINE(BuildStackMenu) {
    static void Callback(ActionList *list, game::MenuContext *context) {
        Orig(list, context); // Preserve vanilla eligibility and sorting.
        if (list->count >= 7 || !ContainsGrabOne(*list)) {
            return;
        }
        AddStackActions(*list, game::StackCount(context->selectedItem),
                        game::HasFreePocket(context->inventory));
    }
};
HOOK_DEFINE_TRAMPOLINE(MapStackAction) {
    static int Callback(int action) {
        if (action == static_cast<int>(Action::GrabFive)) {
            return GrabFiveCallback;
        }
        if (action == static_cast<int>(Action::GrabTen)) {
            return GrabTenCallback;
        }
        return Orig(action);
    }
};
HOOK_DEFINE_INLINE(DispatchStackAction) {
    static void Callback(exl::hook::InlineCtx *registers) {
        // The dispatcher's own frame holds the mapped callback at FP+0x1c.
        // Its unsigned <59 table bound selects entry zero for our large tokens.
        // Entry zero has no virtual adjustment; replace it before BLR executes.
        const auto callback = *reinterpret_cast<const int *>(registers->X[29] +
                                                             frame_layout::DispatcherCallbackToken);
        if (callback != GrabFiveCallback && callback != GrabTenCallback) {
            return;
        }
        registers->X[0] = registers->X[19]; // Pocket controller, without member-pointer adjustment.
        registers->X[8] =
            reinterpret_cast<std::uintptr_t>(callback == GrabFiveCallback ? SplitFive : SplitTen);
        // Exlaunch then executes the displaced BLR x8 and original UI cleanup.
    }
};

int QuantityForSplitFrame(std::uintptr_t frame) {
    // Generic Split: FP=SP+0x20, saved caller LR=FP+8; its frame is 0x60.
    // Our wrapper's quantity at caller SP+16 is therefore FP+0x50.
    const auto caller =
        *reinterpret_cast<const std::uintptr_t *>(frame + frame_layout::SavedReturnAddress);
    if (caller != reinterpret_cast<std::uintptr_t>(qol_pockets_split_return)) {
        return 1;
    }
    const int quantity = *reinterpret_cast<const int *>(frame + frame_layout::WrapperQuantity);
    return (quantity == 5 || quantity == 10) ? quantity : 1;
}
HOOK_DEFINE_INLINE(AdjustSourceCount) {
    static void Callback(exl::hook::InlineCtx *registers) {
        // Displaced SUB w1,w0,#1 still executes: pre-adjust its input by q-1.
        registers->X[0] = static_cast<std::uint32_t>(registers->W[0] -
                                                     (QuantityForSplitFrame(registers->X[29]) - 1));
    }
};
HOOK_DEFINE_INLINE(AdjustDestinationCount) {
    static void Callback(exl::hook::InlineCtx *registers) {
        // Vanilla MOV w1,#1 has executed; override just before its setter call.
        registers->X[1] = static_cast<unsigned>(QuantityForSplitFrame(registers->X[29]));
    }
};
HOOK_DEFINE_TRAMPOLINE(ResolveStackCaption) {
    static game::Caption Callback(int action) {
        // Load native metadata first; ReplaceMenuCaption supplies our visible text.
        if (action == static_cast<int>(Action::GrabFive)) {
            action = static_cast<int>(Action::GrabOne);
        }
        if (action == static_cast<int>(Action::GrabTen)) {
            action = static_cast<int>(Action::TurnipGrabTen);
        }
        return Orig(action);
    }
};
HOOK_DEFINE_TRAMPOLINE(ResolveStackPostAction) {
    static int Callback(int action) {
        return Orig(QuantityForAction(action) ? static_cast<int>(Action::GrabOne) : action);
    }
};

struct HookFingerprint {
    std::uintptr_t offset;
    std::array<std::uint32_t, 5> words;
};
// Validate up to 20 bytes: Exlaunch may need a longer jump for distant modules.
constexpr std::array Hooks{
    HookFingerprint{address::BuildMenu,
                    {0xd10203ff, 0xa9027bfd, 0xa9036ffc, 0xa90467fa, 0xa9055ff8}},
    HookFingerprint{address::MapAction,
                    {0xd10083ff, 0xa9017bfd, 0x910043fd, 0xb81fc3a0, 0xd0012c08}},
    HookFingerprint{address::DispatchCall,
                    {0xd63f0100, 0x3607fd80, 0xf9410e68, 0x5284d929, 0x38696909}},
    // Also guard the bounds/fallback path that executes before our dispatch hook.
    HookFingerprint{0x0308c73c, {0xd000cbc9, 0x911d0129, 0x8b08112a, 0x7100ed1f, 0x9a893149}},
    HookFingerprint{0x0308c750, {0xa9402528, 0x8b890660, 0x36000069, 0xf9400009, 0xf8686928}},
    HookFingerprint{address::SubtractOne,
                    {0x51000401, 0x910023e0, 0x97a5f1ce, 0x294227e8, 0x290027e8}},
    HookFingerprint{address::SetDestinationCount,
                    {0x97a5f1b4, 0x294227e8, 0xaa1403e0, 0x290027e8, 0x97d405dc}},
    HookFingerprint{address::ResolveCaption,
                    {0xd10043ff, 0xb001c1a9, 0xb9000fe0, 0xf943f12b, 0xb9800fe9}},
    HookFingerprint{address::PostAction,
                    {0xd10043ff, 0xb001c1a8, 0xb9000fe0, 0xf943f10a, 0xb9800fe8}},
};
} // namespace

bool InstallStackSplitting() {
    if (!ValidateMenuText()) {
        Logging.Error("[QoL Enhancements/Pockets] STACKS_DISABLED text hook mismatch\n");
        return false;
    }
    const auto *fallback =
        reinterpret_cast<const std::uintptr_t *>(game::MainAddress(address::CallbackTable));
    if (fallback[0] != game::MainAddress(address::FallbackCallback) || fallback[1] != 0) {
        Logging.Error("[QoL Enhancements/Pockets] STACKS_DISABLED callback fallback mismatch\n");
        return false;
    }
    const auto &text = exl::util::GetMainModuleInfo().m_Text;
    for (const auto &hook : Hooks) {
        const auto location = game::MainAddress(hook.offset);
        if (!text.InRange(location) || location > text.GetEnd() - sizeof(hook.words) ||
            std::memcmp(reinterpret_cast<const void *>(location), hook.words.data(),
                        sizeof(hook.words)) != 0) {
            Logging.Error("[QoL Enhancements/Pockets] STACKS_DISABLED hook mismatch at 0x%lx\n",
                        hook.offset);
            return false;
        }
    }
    // Install the menu last so no new action can appear before its handlers exist.

    qol::core::HookPlan hookPlan{
        qol::core::Site<AdjustSourceCount>("AdjustSourceCount", address::SubtractOne, {0x51000401, 0x910023e0, 0x97a5f1ce, 0x294227e8, 0x290027e8}, true),
        qol::core::Site<AdjustDestinationCount>("AdjustDestinationCount", address::SetDestinationCount, {0x97a5f1b4, 0x294227e8, 0xaa1403e0, 0x290027e8, 0x97d405dc}, true),
        qol::core::Site<DispatchStackAction>("DispatchStackAction", address::DispatchCall, {0xd63f0100, 0x3607fd80, 0xf9410e68, 0x5284d929, 0x38696909}, true),
        qol::core::Site<MapStackAction>("MapStackAction", address::MapAction, {0xd10083ff, 0xa9017bfd, 0x910043fd, 0xb81fc3a0, 0xd0012c08}, true),
        qol::core::Site<ResolveStackCaption>("ResolveStackCaption", address::ResolveCaption, {0xd10043ff, 0xb001c1a9, 0xb9000fe0, 0xf943f12b, 0xb9800fe9}, true),
        qol::core::Site<ResolveStackPostAction>("ResolveStackPostAction", address::PostAction, {0xd10043ff, 0xb001c1a8, 0xb9000fe0, 0xf943f10a, 0xb9800fe8}, true),
        qol::core::Site<BuildStackMenu>("BuildStackMenu", address::BuildMenu, {0xd10203ff, 0xa9027bfd, 0xa9036ffc, 0xa90467fa, 0xa9055ff8}, true),
    };
    if (!hookPlan) return false;
    if (!InstallMenuText()) return false;
    if (!hookPlan.Publish<AdjustSourceCount>(address::SubtractOne)) return false;
    if (!hookPlan.Publish<AdjustDestinationCount>(address::SetDestinationCount)) return false;
    if (!hookPlan.Publish<DispatchStackAction>(address::DispatchCall)) return false;
    if (!hookPlan.Publish<MapStackAction>(address::MapAction)) return false;
    if (!hookPlan.Publish<ResolveStackCaption>(address::ResolveCaption)) return false;
    if (!hookPlan.Publish<ResolveStackPostAction>(address::PostAction)) return false;
    if (!hookPlan.Publish<BuildStackMenu>(address::BuildMenu)) return false;
    Logging.Debug("[QoL Enhancements/Pockets] STACKS_READY Grab 1 -> Grab 5 -> Grab 10\n");
    Logging.Debug("[QoL Enhancements/Pockets] IDS actions=0x4F550001/0x4F550002 "
                "callbacks=0x4F551001/0x4F551002\n");
    return true;
}
} // namespace qol_pockets::features
