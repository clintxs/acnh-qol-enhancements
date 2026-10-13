#include "program/core/hook_plan.hpp"
#include "lib.hpp"
#include "page_input.hpp"
#include <array>

namespace qol::core {
namespace {
constexpr std::uintptr_t InputAddress = 0x03c58d24;
std::array<PageInputHandler, 5> InputHandlers{};
std::array<PageInputHandler, 5> InputCancellations{};
std::array<InputPriority, 5> InputPriorities{};
struct PageInputObserver { PageInputCallback before{}, after{}; };
std::array<PageInputObserver, 5> InputObservers{};
std::array<InputPriority, 5> ObserverPriorities{};
std::array<bool, 5> TypedObservers{};
HOOK_DEFINE_TRAMPOLINE(SharedPageInput) {
    static void Callback(void *page, void *snapshot, unsigned blocked) {
        for (unsigned i = 0; i < InputHandlers.size(); ++i)
            if (InputHandlers[i] && InputHandlers[i](page, snapshot, blocked)) {
                // Suppressed consumers still get a cancellation sample. They
                // must drain queued per-frame input, never start new operations.
                for (unsigned j = i + 1; j < InputHandlers.size(); ++j)
                    if (InputCancellations[j]) InputCancellations[j](page, snapshot, blocked | 1 | SuppressedInput);
                for (unsigned j = 0; j < InputObservers.size(); ++j)
                    if (InputObservers[j].before)
                        InputObservers[j].before(page, snapshot, blocked | 1 | (TypedObservers[j] ? SuppressedInput : 0));
                return;
            }
        for (const auto &observer : InputObservers) if (observer.before) observer.before(page, snapshot, blocked);
        Orig(page, snapshot, blocked);
        for (const auto &observer : InputObservers) if (observer.after) observer.after(page, snapshot, blocked);
    }
};
bool InstallPageInput() {
    HookPlan plan{Site<SharedPageInput>("SharedPageInput", InputAddress,
        {0xf9410800, 0xb4000060, 0x12000042, 0x17ffcad4, 0xd65f03c0})};
    return plan && plan.Publish<SharedPageInput>(InputAddress);
}
RegistrationResult Failure(RegistrationStatus status, const char *reason) {
    ReportActivationFailure(FeatureState::DependencyUnavailable, reason);
    return {status};
}
}
RegistrationResult RegisterInputAdapter(PageInputHandler handler, InputPriority priority, PageInputHandler cancel) {
    if (!handler) return {RegistrationStatus::Invalid};
    unsigned count = 0;
    while (count < InputHandlers.size() && InputHandlers[count]) {
        if (InputHandlers[count] == handler) {
            if (InputPriorities[count] != priority || InputCancellations[count] != cancel)
                return Failure(RegistrationStatus::Conflict, "incompatible input registration");
            return {InstallPageInput() ? RegistrationStatus::Identical : RegistrationStatus::Unavailable};
        }
        ++count;
    }
    if (count == InputHandlers.size()) return Failure(RegistrationStatus::Capacity, "input registration capacity");
    if (!InstallPageInput()) return {RegistrationStatus::Unavailable};
    unsigned index = count;
    while (index && unsigned(InputPriorities[index - 1]) > unsigned(priority)) {
        InputHandlers[index] = InputHandlers[index - 1];
        InputCancellations[index] = InputCancellations[index - 1];
        InputPriorities[index] = InputPriorities[index - 1];
        --index;
    }
    InputHandlers[index] = handler; InputCancellations[index] = cancel; InputPriorities[index] = priority;
    RecordMutation();
    return {RegistrationStatus::Ready};
}
RegistrationResult RegisterObserverAdapter(PageInputCallback before, PageInputCallback after, InputPriority priority, bool typed) {
    if (!before || !after) return {RegistrationStatus::Invalid};
    unsigned count = 0;
    while (count < InputObservers.size() && InputObservers[count].before) {
        const auto &existing = InputObservers[count];
        if (existing.before == before || existing.after == after) {
            if (existing.before != before || existing.after != after || ObserverPriorities[count] != priority || TypedObservers[count] != typed)
                return Failure(RegistrationStatus::Conflict, "incompatible input observer registration");
            return {InstallPageInput() ? RegistrationStatus::Identical : RegistrationStatus::Unavailable};
        }
        ++count;
    }
    if (count == InputObservers.size()) return Failure(RegistrationStatus::Capacity, "input registration capacity");
    if (!InstallPageInput()) return {RegistrationStatus::Unavailable};
    unsigned index = count;
    while (index && unsigned(ObserverPriorities[index - 1]) > unsigned(priority)) {
        InputObservers[index] = InputObservers[index - 1]; ObserverPriorities[index] = ObserverPriorities[index - 1];
        TypedObservers[index] = TypedObservers[index - 1]; --index;
    }
    InputObservers[index] = {before, after}; ObserverPriorities[index] = priority; TypedObservers[index] = typed;
    RecordMutation();
    return {RegistrationStatus::Ready};
}
} // namespace qol::core
