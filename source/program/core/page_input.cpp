#include "program/core/hook_plan.hpp"
#include "lib.hpp"
#include "page_input.hpp"
#include <array>
#include <cstring>

namespace qol::core {
namespace {
constexpr std::uintptr_t InputAddress = 0x03c58d24;
std::array<PageInputHandler, 5> InputHandlers{};
struct PageInputObserver {
    PageInputCallback before{}, after{};
};
std::array<PageInputObserver, 5> InputObservers{};
HOOK_DEFINE_TRAMPOLINE(SharedPageInput) {
    static void Callback(void *page, void *snapshot, unsigned blocked) {
        for (auto handler : InputHandlers)
            if (handler && handler(page, snapshot, blocked)) {
                for (const auto &observer : InputObservers)
                    if (observer.before) observer.before(page, snapshot, blocked | 1);
                return;
            }
        for (const auto &observer : InputObservers)
            if (observer.before) observer.before(page, snapshot, blocked);
        Orig(page, snapshot, blocked);
        for (const auto &observer : InputObservers)
            if (observer.after) observer.after(page, snapshot, blocked);
    }
};
bool InstallPageInput() {
    HookPlan plan{Site<SharedPageInput>("SharedPageInput", InputAddress,
        {0xf9410800, 0xb4000060, 0x12000042, 0x17ffcad4, 0xd65f03c0})};
    return plan && plan.Publish<SharedPageInput>(InputAddress);
}

}
bool RegisterPageInput(PageInputHandler handler) {
    if (!handler) return false;
    for (auto existing : InputHandlers) if (existing == handler) return InstallPageInput();
    for (auto &slot : InputHandlers) if (!slot) {
        if (!InstallPageInput()) return false;
        slot = handler;
        RecordMutation();
        return true;
    }
    ReportActivationFailure(FeatureState::DependencyUnavailable, "input registration capacity");
    return false;
}
bool RegisterPageInputObserver(PageInputCallback before, PageInputCallback after) {
    if (!before || !after) return false;
    for (const auto &existing : InputObservers)
        if (existing.before == before && existing.after == after) return InstallPageInput();
    for (const auto &existing : InputObservers)
        if (existing.before == before || existing.after == after) {
            ReportActivationFailure(FeatureState::DependencyUnavailable, "incompatible input observer registration");
            return false;
        }
    for (auto &slot : InputObservers) if (!slot.before) {
        if (!InstallPageInput()) return false;
        slot = {before, after};
        RecordMutation();
        return true;
    }
    ReportActivationFailure(FeatureState::DependencyUnavailable, "input registration capacity");
    return false;
}
}
