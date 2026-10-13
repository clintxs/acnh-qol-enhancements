#include "program/core/hook_plan.hpp"
#include "lib.hpp"
#include "hint_text.hpp"
#include <array>
#include <cstring>

namespace qol::core {
namespace {
constexpr std::uintptr_t InitializeMessage = 0x01aea210;
constexpr std::uintptr_t SetPaneText = 0x03c1b588, SetGroupText = 0x03c47b90;
constexpr std::uintptr_t NotifyTextChanged = 0x03c7dd80;
constexpr std::uintptr_t TextOperationVtableGot = 0x04ac0878;
struct SafeString { std::uintptr_t vtable; const char *text; };
std::array<HintTextProvider, 4> HintProviders{};
template<class T> T &Field(std::uintptr_t object, unsigned offset) {
    return *reinterpret_cast<T *>(object + offset);
}
HOOK_DEFINE_TRAMPOLINE(InitializeHintCaption) {
    static void Callback(std::uintptr_t message, std::uintptr_t binding,
                         const SafeString *bank, const SafeString *key) {
        Orig(message, binding, bank, key);
        if (!message || !key || !key->text) return;
        const char16_t *caption = nullptr;
        for (auto provider : HintProviders) {
            if (provider && (caption = provider(key->text))) break;
        }
        if (!caption) return;
        // Native initialization retains the binding. State 12 skips MSBT
        // resolution; the normal destructor still cleans the message object.
        Field<unsigned>(message, 0xc) = 12;
        const auto adapter = binding ? Field<std::uintptr_t>(binding, 0x48) : 0;
        const auto methods = adapter ? Field<std::uintptr_t>(adapter, 0x18) : 0;
        const auto payload = adapter ? Field<std::uintptr_t>(adapter, 8) : 0;
        const auto setter = methods ? Field<std::uintptr_t>(methods, 0x5e0) : 0;
        const auto base = exl::util::modules::GetTargetStart();
        if (!payload || (setter != base + SetPaneText && setter != base + SetGroupText)) return;
        const auto interface = Field<std::uintptr_t>(adapter, 0);
        if (!interface) return;
        // Match native message publication at 01aeb27c..01aeb2c0: prepare
        // text operation 9, set text, then notify the binding. Setting the pane
        // alone renders letters but leaves the footer background at its old
        // width. The notification schedules its existing dependent bindings;
        // never guess a pixel width or resize the footer globally.
        struct TextOperation { std::uintptr_t vtable; int operation, padding; };
        const TextOperation operation{Field<std::uintptr_t>(base, TextOperationVtableGot) + 16, 9, 0};
        const auto prepare = Field<void (*)(std::uintptr_t, const TextOperation *)>(interface, 0x18);
        if (!prepare) return;
        prepare(adapter, &operation);
        unsigned length = 0;
        while (caption[length]) ++length;
        reinterpret_cast<void (*)(std::uintptr_t, const char16_t *, unsigned)>(setter)(payload, caption, length);
        reinterpret_cast<void (*)(std::uintptr_t)>(base + NotifyTextChanged)(binding);
    }
};
}
bool RegisterHintText(HintTextProvider provider) {
    if (!provider) return false;
    // Reserve callback capacity before publishing the shared hook. Startup is
    // synchronous and Exlaunch never invokes registered callbacks here.
    HintTextProvider *destination = nullptr;
    bool duplicate = false;
    for (auto &slot : HintProviders) if (slot == provider) { destination = &slot; duplicate = true; break; }
    if (!destination) for (auto &slot : HintProviders) if (!slot) { destination = &slot; break; }
    if (!destination) {
        ReportActivationFailure(FeatureState::DependencyUnavailable, "hint provider capacity");
        return false;
    }
    HookPlan plan{Site<InitializeHintCaption>("InitializeHintCaption", InitializeMessage,
        {0xa9bc7bfd, 0xa9015ff8, 0xa90257f6, 0xa9034ff4, 0x910003fd})};
    if (!plan) return false;
    constexpr native::Fingerprint<> notify{NotifyTextChanged,
        {0xd10103ff, 0xa9017bfd, 0xf90013f5, 0xa9034ff4, 0x910043fd}, "caption.NotifyTextChanged"};
    if (const auto result = native::MainExecutable().Check(notify); !result) {
        native::ReportMismatch(result);
        return false;
    }
    if (!plan.Publish<InitializeHintCaption>(InitializeMessage)) return false;
    if (!duplicate) { *destination = provider; RecordMutation(); }
    return true;
}
}
