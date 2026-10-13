#pragma once
#include "registration.hpp"
namespace qol::core {
enum class InputOutcome { PassThrough, Consume };
enum class InputReason { Dispatch, Suppressed };
// Callback-scoped native objects. Domain adapters interpret snapshots; contexts
// and their pointers must not survive the synchronous input pass.
struct PageInputContext {
    void *page, *snapshot;
    unsigned blocked;
    InputReason reason = InputReason::Dispatch;
    bool Cancelled() const { return reason == InputReason::Suppressed; }
};
// Preserve the shipped module order explicitly, including workbench before
// quantity handlers. Equal priorities use stable registration order.
enum class InputPriority : unsigned { Pockets = 10, Preview = 20, Workbench = 30, QuantityWrap = 40, QuantityShortcut = 50, LegacyAdapter = 60 };
using PageInputHandler = bool (*)(void *, void *, unsigned);
using PageInputCallback = void (*)(void *, void *, unsigned);
// Native adapter bridge for the single bounded registry. Features use the
// typed registration templates below.
RegistrationResult RegisterInputAdapter(PageInputHandler handler, InputPriority priority, PageInputHandler cancel);
RegistrationResult RegisterObserverAdapter(PageInputCallback before, PageInputCallback after, InputPriority priority, bool typed);
inline constexpr unsigned SuppressedInput = 1u << 31; // Internal bridge flag; never sent to native dispatch.
inline PageInputContext InputContext(void *page, void *snapshot, unsigned blocked) {
    return {page, snapshot, blocked & ~SuppressedInput,
            blocked & SuppressedInput ? InputReason::Suppressed : InputReason::Dispatch};
}
template<auto Handler> bool TypedPageInput(void *page, void *snapshot, unsigned blocked) {
    return Handler(InputContext(page, snapshot, blocked)) == InputOutcome::Consume;
}
template<auto Handler> RegistrationResult RegisterPageInput(InputPriority priority) {
    return RegisterInputAdapter(TypedPageInput<Handler>, priority, TypedPageInput<Handler>);
}
template<auto Callback> void TypedPageObserver(void *page, void *snapshot, unsigned blocked) {
    Callback(InputContext(page, snapshot, blocked));
}
template<auto Before, auto After> RegistrationResult RegisterPageObserver(InputPriority priority) {
    return RegisterObserverAdapter(TypedPageObserver<Before>, TypedPageObserver<After>, priority, true);
}
}
