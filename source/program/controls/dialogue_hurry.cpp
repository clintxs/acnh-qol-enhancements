#include "program/core/hook_plan.hpp"
#include "lib.hpp"
#include "dialogue_hurry.hpp"
#include "program/dialogue/reveal_hooks.hpp"
#include <cstring>

namespace qol::controls {
namespace {
namespace address {
constexpr std::uintptr_t AdvanceInput = 0x01afa530, ActiveController = 0x02435188;
constexpr std::uintptr_t ApplicationGot = 0x04ab3960;
}
namespace layout {
constexpr std::size_t TextManager = 0x208, ManagerText = 0x28, TalkContext = 0x20;
constexpr std::size_t TextContext = 0x210, RevealState = 0x21ac;
constexpr std::size_t ManagerState = 0x48, AdvanceTimer = 0x2464;
constexpr std::size_t ContextPhase = 0x28, MessageController = 8, ChoiceController = 0x18;
constexpr std::size_t ControllerState = 8, ApplicationController = 0xf5b20;
constexpr std::size_t WindowKind = 0x2c, ActiveMessageBank = 0x2488, ActiveMessageLabel = 0x24f8;
constexpr unsigned MessageBankBytes = 64, BalloonWindow = 2;
constexpr std::size_t HeldButtons = 8;
// Native controller format (not platform HID): live held Y is 0010.
constexpr std::uint16_t HurryButton = 0x10;
} // namespace layout
bool Enabled = false;
template <class T> T Read(std::uintptr_t object, std::size_t offset = 0) {
    T result;
    std::memcpy(&result, reinterpret_cast<const void *>(object + offset), sizeof(result));
    return result;
}
std::uintptr_t Main(std::uintptr_t offset) {
    return exl::util::GetMainModuleInfo().m_Text.m_Start + offset;
}
struct HurryInput {
    std::uintptr_t controller{};
    unsigned buttons{};
    bool Held() const { return buttons & layout::HurryButton; }
};
HurryInput ReadHurryInput() {
    // Match Print's active-player getter and application fallback. Read live
    // input in Advance as well: Print's cached B/R byte is stale while waiting.
    auto controller = reinterpret_cast<std::uintptr_t (*)()>(Main(address::ActiveController))();
    if (!controller) {
        const auto slot = Read<std::uintptr_t>(Main(address::ApplicationGot));
        const auto application = slot ? Read<std::uintptr_t>(slot) : 0;
        controller = application ? Read<std::uintptr_t>(application, layout::ApplicationController) : 0;
    }
    return {controller, controller ? Read<std::uint16_t>(controller, layout::HeldButtons) : 0u};
}
// A null reason means eligible. Log rejected frames too, rather than only
// successful hurry frames (the first build's diagnostic blind spot).
const char *HurryContextBlock(std::uintptr_t manager, std::uintptr_t text, unsigned state) {
    if (!manager || !text) return "missing-owner";
    if (Read<std::uintptr_t>(manager, layout::ManagerText) != text) return "manager-text";
    if (Read<std::uintptr_t>(text, layout::TextManager) != manager) return "text-manager";
    if (Read<unsigned>(manager, layout::ManagerState) != state) return "manager-state";
    const auto context = Read<std::uintptr_t>(manager, layout::TalkContext);
    if (!context) return "missing-context";
    if (Read<std::uintptr_t>(text, layout::TextContext) != context) return "text-context";
    if (Read<unsigned>(context, layout::ContextPhase) != 2) return "context-phase";
    if (Read<unsigned>(context, layout::WindowKind) > layout::BalloonWindow) return "window";
    const auto choices = Read<std::uintptr_t>(context, layout::ChoiceController);
    // A choice can open on the final advance, but never advance while its
    // controller is opening, active or closing. No synthetic confirm input.
    if (!choices) return "missing-choices";
    return Read<unsigned>(choices, layout::ControllerState) == 0 ? nullptr : "choice-active";
}
bool CopyMessageId(std::uintptr_t manager, std::size_t field, char *output) {
    output[0] = 0;
    const auto data = Read<std::uintptr_t>(manager, field);
    if (!data) return false;
    for (unsigned i = 0; i < layout::MessageBankBytes; ++i) {
        const auto c = Read<char>(data, i);
        if (!c) { output[i] = 0; return i != 0; }
        if (i + 1 < layout::MessageBankBytes) output[i] = c;
    }
    output[0] = 0;
    return false;
}
bool IsNewCatchAnnouncement(std::uintptr_t manager) {
    // Player_GetDemo's cFirstGet branches open label 001 in these banks.
    // All 14 supplied locale packs share the IDs, even when the localized
    // announcement is not "Yes!". Read the active pair each time so the stop
    // cannot leak into the species message or a later conversation.
    char bank[layout::MessageBankBytes]{}, label[layout::MessageBankBytes]{};
    if (!CopyMessageId(manager, layout::ActiveMessageBank, bank) ||
        !CopyMessageId(manager, layout::ActiveMessageLabel, label) ||
        std::strcmp(label, "001")) return false;
    return !std::strcmp(bank, "SYS_Get_Fish") ||
           !std::strcmp(bank, "SYS_Get_Insect") ||
           !std::strcmp(bank, "SYS_Get_DiveFish");
}
const char *AdvanceHurryBlock(std::uintptr_t manager, std::uintptr_t text) {
    if (const auto block = HurryContextBlock(manager, text, 5)) return block;
    const auto reveal = Read<unsigned>(text, layout::RevealState);
    // Reveal 4 is the explicit input pause (<10:4>), e.g. the Sunday turnip
    // message. Native Advance resumes Print without clearing the page, and
    // Print entry changes 4 back to revealing=1. Held Y can supply that local
    // advance request just as it does for a page break or completed message.
    if (reveal != 2 && reveal != 3 && reveal != 4) return "text-not-ready";
    const auto timer = Read<int>(manager, layout::AdvanceTimer);
    if (timer != -1 && timer != 0) return "scripted-timer";
    const auto context = Read<std::uintptr_t>(manager, layout::TalkContext);
    const auto message = Read<std::uintptr_t>(context, layout::MessageController);
    if (!message) return "missing-message";
    const auto phase = Read<unsigned>(message, layout::ControllerState);
    // The message wrapper does not share the choice controller's animation
    // states: live NPC pages wait for input in phase 1. Allow that state in
    // addition to the existing 0/2 paths; the separate choice guard still
    // blocks opening, active and closing menus.
    if (phase > 2) return "message-transition";
    // Only suppress our synthetic advance request. Native manual input and
    // text speed remain unchanged, including localized pages within label 001.
    return IsNewCatchAnnouncement(manager) ? "new-catch" : nullptr;
}
#if CAS_VERBOSE_HURRY_TRACE
enum class HurryStage { Input, Speed, Advance };
void TraceHurry(HurryStage stage, std::uintptr_t manager, std::uintptr_t text,
                HurryInput input, const char *block, unsigned result) {
    struct Snapshot {
        std::uintptr_t manager{}, text{}, context{}, textContext{}, textManager{}, managerText{}, controller{};
        unsigned buttons{}, state = ~0u, phase = ~0u, window = ~0u, reveal = ~0u;
        unsigned choices = ~0u, message = ~0u, forced{}, disabled{}, result{};
        int timer{};
        const char *block{};
        char bank[layout::MessageBankBytes]{}, label[layout::MessageBankBytes]{};
        bool operator==(const Snapshot &) const = default;
    } current;
    current.manager = manager; current.text = text;
    current.controller = input.controller; current.buttons = input.buttons;
    current.block = block; current.result = result;
    if (manager) {
        current.context = Read<std::uintptr_t>(manager, layout::TalkContext);
        current.managerText = Read<std::uintptr_t>(manager, layout::ManagerText);
        current.state = Read<unsigned>(manager, layout::ManagerState);
        current.timer = Read<int>(manager, layout::AdvanceTimer);
        CopyMessageId(manager, layout::ActiveMessageBank, current.bank);
        CopyMessageId(manager, layout::ActiveMessageLabel, current.label);
    }
    if (text) {
        current.textContext = Read<std::uintptr_t>(text, layout::TextContext);
        current.textManager = Read<std::uintptr_t>(text, layout::TextManager);
        current.reveal = Read<unsigned>(text, layout::RevealState);
        current.forced = Read<std::uint8_t>(text, 0x21b5);
        current.disabled = Read<std::uint8_t>(text, 0x21b6);
    }
    if (current.context) {
        current.phase = Read<unsigned>(current.context, layout::ContextPhase);
        current.window = Read<unsigned>(current.context, layout::WindowKind);
        const auto choices = Read<std::uintptr_t>(current.context, layout::ChoiceController);
        const auto message = Read<std::uintptr_t>(current.context, layout::MessageController);
        if (choices) current.choices = Read<unsigned>(choices, layout::ControllerState);
        if (message) current.message = Read<unsigned>(message, layout::ControllerState);
    }
    const auto index = static_cast<unsigned>(stage);
    static Snapshot previous[3]{};
    // Separate held/unheld budgets: startup UI must not consume Y evidence.
    static unsigned reports[3][2]{};
    auto &count = reports[index][input.Held()];
    if (count >= 96 || (current == previous[index] && count)) return;
    previous[index] = current; ++count;
    constexpr const char *stages[]{"input", "speed", "advance"};
    // This explicit diagnostic build logs bounded evidence even if the user's
    // normal log_debug setting is false. No INI edits or extra hooks required.
    Logging.Log("[QoL Enhancements/Controls] HURRY stage=%s gate=%s buttons=%04x y=%u controller=%u state=%u context_phase=%u window=%u reveal=%u choices=%u message=%u timer=%d owner_match=%u context_match=%u forced=%u disabled=%u result=%u bank=%s label=%s\n",
                stages[index], block ? block : "ready", input.buttons, unsigned(input.Held()), unsigned(input.controller != 0),
                current.state, current.phase, current.window, current.reveal,
                current.choices, current.message, current.timer,
                unsigned(manager && text && current.textManager == manager && current.managerText == text),
                unsigned(current.context && current.textContext == current.context),
                current.forced, current.disabled, result, current.bank, current.label);
}
#endif
HOOK_DEFINE_INLINE(DialogueAdvanceInput) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        const auto manager = registers->X[19];
        const auto text = Read<std::uintptr_t>(manager, layout::ManagerText);
        const auto block = AdvanceHurryBlock(manager, text);
#if CAS_VERBOSE_HURRY_TRACE
        const auto input = ReadHurryInput();
#else
        const auto input = block ? HurryInput{} : ReadHurryInput();
#endif
        if (!block && input.Held()) registers->W[22] = 1;
#if CAS_VERBOSE_HURRY_TRACE
        TraceHurry(HurryStage::Advance, manager, text, input, block, registers->W[22]);
#endif
        // Only the local advance request changes. The native function still
        // clears a page (state 2), opens choices (manager+2475), or closes the
        // message (state 3). Y never writes a persistent latch or controller.
    }
};
} // namespace

unsigned SelectDialogueHurry(std::uintptr_t text, unsigned currentFlag) {
    if (!Enabled) return currentFlag;
    const auto manager = Read<std::uintptr_t>(text, layout::TextManager);
    const auto block = HurryContextBlock(manager, text, 3);
#if CAS_VERBOSE_HURRY_TRACE
    const auto input = ReadHurryInput();
#else
    const auto input = block ? HurryInput{} : ReadHurryInput();
#endif
    const auto flag = !block && input.Held() ? DialogueHurryFlag : currentFlag;
#if CAS_VERBOSE_HURRY_TRACE
    TraceHurry(HurryStage::Speed, manager, text, input, block, flag);
#endif
    return flag;
}
void TraceDialogueInput(std::uintptr_t manager, std::uintptr_t text,
                        std::uintptr_t controller, unsigned held) {
#if CAS_VERBOSE_HURRY_TRACE
    if (Enabled) TraceHurry(HurryStage::Input, manager, text,
                           {controller, controller ? Read<std::uint16_t>(controller, layout::HeldButtons) : 0u},
                           HurryContextBlock(manager, text, 3), held);
#else
    (void)manager; (void)text; (void)controller; (void)held;
#endif
}
bool InstallHoldToAdvanceDialogue() {
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    constexpr Fingerprint fingerprints[]{
        {address::AdvanceInput, {0xb964666b, 0x37f801ab, 0x3400024b, 0x5100096c, 0x7100057f}},
        {address::ActiveController, {0xd10083ff, 0xa9017bfd, 0x910043fd, 0x90013ce8, 0x91276108}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &expected : fingerprints) {
        if (expected.offset + sizeof(expected.words) > main.m_Text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(Main(expected.offset)), expected.words, sizeof(expected.words))) {
            Logging.Error("[QoL Enhancements/Controls] hold dialogue: DISABLED; instruction mismatch at main+0x%lx\n", expected.offset);
            return false;
        }
    }

    qol::core::HookPlan hookPlan{
        qol::core::Site<DialogueAdvanceInput>("DialogueAdvanceInput", address::AdvanceInput, {0xb964666b, 0x37f801ab, 0x3400024b, 0x5100096c, 0x7100057f}, true),
    };
    if (!hookPlan) return false;
    if (!dialogue::InstallRevealHooks()) return false;
    if (!hookPlan.Publish<DialogueAdvanceInput>(address::AdvanceInput)) return false;
    Enabled = true;
#if CAS_VERBOSE_HURRY_TRACE
    Logging.Log("[QoL Enhancements/Controls] DIAGNOSTIC: hurry-v1; raw held input and every hurry gate; log_debug override not required\n");
#endif
    Logging.Debug("[QoL Enhancements/Controls] hold dialogue: READY; hold Y for %ux until choices/new catches/end\n", DialogueHurrySpeed);
    return true;
}
} // namespace qol::controls
