#include "lib.hpp"
#include "settings.hpp"
#include <atomic>
#include <cstring>

namespace qol::dialogue {
namespace {
namespace address {
constexpr std::uintptr_t HeldInput = 0x01afa00c, EligibleFast = 0x01afdec0;
constexpr std::uintptr_t ForwardFast = 0x01afded4;
constexpr std::uintptr_t MainRate = 0x01afdefc, RendererRate = 0x01a969e0;
constexpr std::uintptr_t NameBegin = 0x01b04824, NameReady = 0x01b04b80;
constexpr std::uintptr_t AdvanceInput = 0x01afa530, ActiveController = 0x02435188;
constexpr std::uintptr_t ApplicationGot = 0x04ab3960;
constexpr std::uintptr_t EntityRegistryGot = 0x04ab27c0;
constexpr std::uintptr_t NpcTypeGot = 0x04ab37d0, NpcTypeGuardGot = 0x04ab37d8;
} // namespace address
namespace layout {
constexpr std::size_t TextManager = 0x208, ManagerText = 0x28, TalkContext = 0x20;
constexpr std::size_t TextContext = 0x210, RevealState = 0x21ac;
constexpr std::size_t ManagerState = 0x48, AdvanceTimer = 0x2464;
constexpr std::size_t ContextPhase = 0x28, MessageController = 8, ChoiceController = 0x18;
constexpr std::size_t ControllerState = 8, ApplicationController = 0xf5b20;
constexpr std::size_t WindowKind = 0x2c, ActiveSpeaker = 0xc0;
constexpr std::size_t ActiveMessageBank = 0x2488;
constexpr std::size_t ActiveMessageLabel = 0x24f8;
constexpr unsigned MessageBankBytes = 64;
constexpr std::size_t SpeakerEntityHandle = 8;
constexpr std::size_t HeldButtons = 8, HeldFast = 0x21b4, EffectiveFast = 0x21b7, RendererFast = 0x205;
constexpr std::size_t EntityTable = 0x1c0, TableEntries = 8;
constexpr std::size_t EntityHandle = 0x230, EntityKind = 0x15c, EntityFlags = 0x248;
constexpr std::size_t EntityResourceName = 0x3e8;
constexpr std::size_t IsNpcType = 0x20;
constexpr unsigned NpcEntity = 10, PlayerEntity = 0x4f, DialogWindow = 1, BalloonWindow = 2;
constexpr std::uint16_t BoostButtons = 0x4002; // B/R; L is deliberately excluded.
constexpr std::uint16_t HurryButton = 0x8; // Y, sampled without changing controller memory.
} // namespace layout
Settings Preferences{};
constexpr unsigned HurryFlag = 5, HurrySpeed = 30;
// Odd flags retain native fast mode. Bits 1/2 select configured boost / hurry;
// native renderer gates test nonzero and forward the whole byte. Nominal 2x
// is the native fast rate, so scale both text paths by the desired tier / 2.
float RateScale(unsigned flag) {
    if (flag & 4) return float(HurrySpeed) * 0.5f;
    return float(flag & 2 ? Preferences.boostSpeed : Preferences.baseSpeed) * 0.5f;
}
enum class NameTag { Unknown, Named, Unnamed };
struct ResolvedName {
    std::uintptr_t manager{}, source{};
    unsigned handle{};
    char name[NpcNameBytes]{};
    std::uintptr_t context{};
    char bank[layout::MessageBankBytes]{}, label[layout::MessageBankBytes]{};
    bool messageKnown{};
    NameTag tag = NameTag::Unknown;
};
ResolvedName Name{};
template <class T> T Read(std::uintptr_t object, std::size_t offset = 0) {
    T result;
    std::memcpy(&result, reinterpret_cast<const void *>(object + offset), sizeof(result));
    return result;
}
std::uintptr_t Main(std::uintptr_t offset) {
    return exl::util::GetMainModuleInfo().m_Text.m_Start + offset;
}
bool HurryHeld() {
    // Match Print's active-player getter and application fallback. Read live
    // input in Advance as well: Print's cached B/R byte is stale while waiting.
    auto controller = reinterpret_cast<std::uintptr_t (*)()>(Main(address::ActiveController))();
    if (!controller) {
        const auto slot = Read<std::uintptr_t>(Main(address::ApplicationGot));
        const auto application = slot ? Read<std::uintptr_t>(slot) : 0;
        controller = application ? Read<std::uintptr_t>(application, layout::ApplicationController) : 0;
    }
    return controller && (Read<std::uint16_t>(controller, layout::HeldButtons) & layout::HurryButton);
}
bool HurryContext(std::uintptr_t manager, std::uintptr_t text, unsigned state) {
    if (!manager || !text || Read<std::uintptr_t>(manager, layout::ManagerText) != text ||
        Read<std::uintptr_t>(text, layout::TextManager) != manager ||
        Read<unsigned>(manager, layout::ManagerState) != state) return false;
    const auto context = Read<std::uintptr_t>(manager, layout::TalkContext);
    if (!context || Read<std::uintptr_t>(text, layout::TextContext) != context ||
        Read<unsigned>(context, layout::ContextPhase) != 2 ||
        Read<unsigned>(context, layout::WindowKind) > layout::BalloonWindow) return false;
    const auto choices = Read<std::uintptr_t>(context, layout::ChoiceController);
    // A choice can open on the final advance, but never advance while its
    // controller is opening, active or closing. No synthetic confirm input.
    return choices && Read<unsigned>(choices, layout::ControllerState) == 0;
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
bool NameOwnerMatches(std::uintptr_t manager, std::uintptr_t context, std::uintptr_t source) {
    return Name.manager == manager && Name.context == context && Name.source == source &&
           (!source || Name.handle == Read<unsigned>(source, layout::SpeakerEntityHandle));
}
bool NameMessageMatches(std::uintptr_t manager) {
    char bank[layout::MessageBankBytes]{}, label[layout::MessageBankBytes]{};
    return Name.messageKnown && CopyMessageId(manager, layout::ActiveMessageBank, bank) &&
           CopyMessageId(manager, layout::ActiveMessageLabel, label) &&
           !std::strcmp(Name.bank, bank) && !std::strcmp(Name.label, label);
}
bool NativeFunction(std::uintptr_t function) {
    const auto &text = exl::util::GetMainModuleInfo().m_Text;
    return function && function >= text.m_Start && function - text.m_Start < text.m_Size && !(function & 3);
}
enum class SpeakerLookup {
    Manager, Context, Window, Source,
    EmptyHandle, Registry, Entity, EntityKind, NpcType, NpcMethod, NpcMismatch,
    NpcName, NpcNameTooLong, Npc, UiDialog, PlayerEntity, PlayerBank, UnnamedTag
};
struct Speaker {
    SpeakerLookup lookup = SpeakerLookup::Manager;
    unsigned window = ~0u, entityKind = ~0u;
    MessageKind kind = MessageKind::Unknown;
    NameTag tag = NameTag::Unknown;
    unsigned handle{};
    std::uintptr_t entity{};
    unsigned entityHandle = ~0u, entityFlags = ~0u;
    char id[NpcIdBytes]{};
    char localizedName[NpcNameBytes]{};
    char bank[layout::MessageBankBytes]{};
};
Speaker CurrentSpeaker(std::uintptr_t text) {
    Speaker result{};
    const auto manager = Read<std::uintptr_t>(text, layout::TextManager);
    if (!manager) return result;
    result.lookup = SpeakerLookup::Context;
    const auto context = Read<std::uintptr_t>(manager, layout::TalkContext);
    if (!context) return result;
    // Native 01b041a4 selects talk/dialog/balloon from the current message's
    // presentation metadata. A confirmed dialog is UI, even after NPC speech.
    result.lookup = SpeakerLookup::Window;
    const auto window = result.window = Read<unsigned>(context, layout::WindowKind);
    if (window > layout::BalloonWindow) return result;
    // Native 01af89a0..01af8a84 copies the pending MessageID bank into
    // manager+2480's SafeString (data +2488) when starting the current message.
    // Read that owned active bank each update, never a pending ID or last-bank
    // cache. Live DIY/catch messages use window 2 (balloon), so recognize known
    // self banks remain a fallback until native name preparation is observed.
    CopyMessageId(manager, layout::ActiveMessageBank, result.bank);
    const auto source = Read<std::uintptr_t>(manager, layout::ActiveSpeaker);
    const auto tag = result.tag = NameOwnerMatches(manager, context, source) && NameMessageMatches(manager)
                   ? Name.tag : NameTag::Unknown;
    if (window != layout::DialogWindow && tag == NameTag::Unnamed) {
        result.lookup = SpeakerLookup::UnnamedTag;
        result.kind = MessageKind::Unnamed;
        return result;
    }
    // A confirmed name always beats a legacy self-bank guess. Preserve the
    // previous five-bank fallback (including its UI priority) when unknown.
    if (tag == NameTag::Unknown && IsUnnamedMessageBank(result.bank)) {
        result.lookup = SpeakerLookup::PlayerBank;
        result.kind = MessageKind::Unnamed;
        return result;
    }
    if (window == layout::DialogWindow) {
        result.lookup = SpeakerLookup::UiDialog; result.kind = MessageKind::Ui; return result;
    }
    if (window != 0) return result; // Balloon/unknown is not automatically UI.
    result.lookup = SpeakerLookup::Source;
    if (!source) return result; // Absence alone is NOT proof of a UI message.
    // The NPC component's constructor at 01d9e224..01d9e254 copies the entity's
    // generation handle (+230) into this TalkSource's +8 field. In that block,
    // x19=component, source=x19+7f60 and x20=x19+799c: x20+5cc == source+8.
    // source+28 is a POSITION delegate for spatial voice playback, not identity;
    // calling it returns three floats. Read the owned handle without invoking
    // that callback, then validate it against the live registry on every update.
    const auto handle = result.handle = Read<unsigned>(source, layout::SpeakerEntityHandle);
    result.lookup = SpeakerLookup::EmptyHandle;
    if (!handle) return result;
    result.lookup = SpeakerLookup::Registry;
    const auto registrySlot = Read<std::uintptr_t>(Main(address::EntityRegistryGot));
    const auto registry = registrySlot ? Read<std::uintptr_t>(registrySlot) : 0;
    const auto table = registry ? Read<std::uintptr_t>(registry, layout::EntityTable) : 0;
    const auto entries = table ? Read<std::uintptr_t>(table, layout::TableEntries) : 0;
    if (!entries) return result;
    // Mirror 01b000fc..01b00178: handles contain a 12-bit table index and
    // generation. Never use a message bank or cached actor to guess identity.
    result.lookup = SpeakerLookup::Entity;
    const auto entity = result.entity = Read<std::uintptr_t>(entries, (handle >> 20) * sizeof(std::uintptr_t));
    if (!entity) return result;
    result.entityHandle = Read<unsigned>(entity, layout::EntityHandle);
    if (result.entityHandle != handle) return result;
    result.entityFlags = Read<unsigned>(entity, layout::EntityFlags);
    if (result.entityFlags & 2) return result;
    result.lookup = SpeakerLookup::EntityKind;
    const auto kind = result.entityKind = Read<unsigned>(entity, layout::EntityKind);
    if (kind == layout::PlayerEntity && tag != NameTag::Named) {
        result.lookup = SpeakerLookup::PlayerEntity; result.kind = MessageKind::Unnamed; return result;
    }
    if (kind != layout::NpcEntity) return result;
    // Match native 01b00164..01b001e8's NPC RTTI check before borrowing the
    // NPC-specific name field. Never initialize RTTI from a text hook: native
    // NPC voice setup initializes it first; an unavailable type uses base.
    result.lookup = SpeakerLookup::NpcType;
    const auto type = Read<std::uintptr_t>(Main(address::NpcTypeGot));
    const auto guard = Read<std::uintptr_t>(Main(address::NpcTypeGuardGot));
    if (!type || !guard ||
        !(std::atomic_ref<const std::uint8_t>(*reinterpret_cast<const std::uint8_t *>(guard))
              .load(std::memory_order_acquire) & 1)) return result;
    const auto entityMethods = Read<std::uintptr_t>(entity);
    const auto isNpc = entityMethods ? Read<std::uintptr_t>(entityMethods, layout::IsNpcType) : 0;
    result.lookup = SpeakerLookup::NpcMethod;
    if (!NativeFunction(isNpc)) return result;
    result.lookup = SpeakerLookup::NpcMismatch;
    if (!(reinterpret_cast<std::uint64_t (*)(std::uintptr_t, std::uintptr_t)>(isNpc)(entity, type) & 1)) return result;
    result.kind = MessageKind::Npc;
    // Native NPC voice setup uses this SafeString's data (01b000c8). Resource
    // IDs distinguish rcm/rct even when the message bank is shared.
    result.lookup = SpeakerLookup::NpcName;
    const auto name = Read<std::uintptr_t>(entity, layout::EntityResourceName);
    if (!name) return result;
    for (unsigned i = 0; i < NpcIdBytes; ++i) {
        const auto c = Read<char>(name, i);
        if (!c) {
            result.lookup = SpeakerLookup::Npc;
            if (Name.manager == manager && Name.source == source && Name.handle == handle)
                std::memcpy(result.localizedName, Name.name, sizeof(result.localizedName));
            return result;
        }
        if (i == NpcIdBytes - 1) break;
        result.id[i] = c;
    }
    result.lookup = SpeakerLookup::NpcNameTooLong;
    result.id[0] = 0; // Never match a truncated resource ID.
    return result;
}

#if CAS_VERBOSE_DIALOGUE_TRACE
// Observe the message even when native reveal gates never reach SelectSpeed.
// These are the manager's owned SafeStrings, not calls into pending MessageID
// delegates. The cache is diagnostic evidence only; it cannot select a speed.
void TraceMessage(std::uintptr_t manager, std::uintptr_t text) {
    constexpr std::size_t CachedBank = 0x25c0, ActiveLabel = 0x24f8;
    constexpr std::size_t TextContext = 0x210, RevealState = 0x21ac;
    constexpr std::size_t ForcedInstant = 0x21b5, AccelerationDisabled = 0x21b6;
    struct Snapshot {
        std::uintptr_t manager{}, source{};
        unsigned window = ~0u, state{}, forced{}, disabled{};
        bool contextMatches{};
        char bank[64]{}, cachedBank[64]{}, label[64]{};
    } current;
    const auto copy = [](std::uintptr_t pointer, auto &buffer) {
        if (!pointer) { std::strcpy(buffer, "<null>"); return; }
        for (unsigned i = 0; i < sizeof(buffer); ++i) {
            const char c = Read<char>(pointer, i);
            if (!c) { buffer[i] = 0; return; }
            if (i + 1 < sizeof(buffer)) buffer[i] = c;
        }
        std::strcpy(buffer, "<unterminated>");
    };
    current.manager = manager;
    current.source = Read<std::uintptr_t>(manager, layout::ActiveSpeaker);
    const auto context = Read<std::uintptr_t>(manager, layout::TalkContext);
    if (context) current.window = Read<unsigned>(context, layout::WindowKind);
    current.contextMatches = context == Read<std::uintptr_t>(text, TextContext);
    current.state = Read<unsigned>(text, RevealState);
    current.forced = Read<std::uint8_t>(text, ForcedInstant);
    current.disabled = Read<std::uint8_t>(text, AccelerationDisabled);
    copy(Read<std::uintptr_t>(manager, layout::ActiveMessageBank), current.bank);
    copy(Read<std::uintptr_t>(manager, CachedBank), current.cachedBank);
    copy(Read<std::uintptr_t>(manager, ActiveLabel), current.label);
    static Snapshot previous;
    static unsigned reports = 0;
    const bool changed = !reports || previous.manager != current.manager ||
        previous.source != current.source || previous.window != current.window ||
        previous.state != current.state || previous.forced != current.forced ||
        previous.disabled != current.disabled || previous.contextMatches != current.contextMatches ||
        std::strcmp(previous.bank, current.bank) || std::strcmp(previous.cachedBank, current.cachedBank) ||
        std::strcmp(previous.label, current.label);
    if (!changed || reports >= 256) return;
    previous = current;
    ++reports;
    Logging.Debug("[QoL Enhancements/Faster Dialogue] MESSAGE manager=0x%lx source=0x%lx window=%u state=%u forced=%u disabled=%u context_match=%u bank=%s cached_bank=%s label=%s\n",
                current.manager, current.source, current.window, current.state, current.forced,
                current.disabled, unsigned(current.contextMatches), current.bank, current.cachedBank, current.label);
    if (reports == 256)
        Logging.Debug("[QoL Enhancements/Faster Dialogue] MESSAGE trace limit reached; restart for a fresh trace\n");
}
#endif

HOOK_DEFINE_INLINE(DialogueNameBegin) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        // Native 01b04808 prepares the name tag. Clear on EVERY preparation,
        // including absent names and failed delegates, before retaining owner
        // identity. x0 is the presentation context; context+0 is its manager.
        Name = {};
        if (!registers->X[0]) return;
        Name.context = registers->X[0];
        Name.manager = Read<std::uintptr_t>(registers->X[0]);
        if (!Name.manager) return;
        Name.source = Read<std::uintptr_t>(Name.manager, layout::ActiveSpeaker);
        if (Name.source) Name.handle = Read<unsigned>(Name.source, layout::SpeakerEntityHandle);
        Name.messageKnown = CopyMessageId(Name.manager, layout::ActiveMessageBank, Name.bank) &&
                            CopyMessageId(Name.manager, layout::ActiveMessageLabel, Name.label);
    }
};
HOOK_DEFINE_INLINE(DialogueNameReady) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        // 01b048ec..01b04914 invokes the SAME source+128 name delegate as the
        // label. Its holder's virtual +18/+20 return UTF-16 data/capacity in
        // x19/w0 at 01b04b80 (verified getters 005f5b28/005f5b30).
        // Borrow only for this copy: the holder resides in the native frame.
        const auto data = registers->X[19];
        const auto units = registers->W[0];
        if (!Name.manager ||
            !NameOwnerMatches(Name.manager, Read<std::uintptr_t>(Name.manager, layout::TalkContext), registers->X[24]) ||
            Read<std::uintptr_t>(Name.manager, layout::ActiveSpeaker) != Name.source) return;
        // Exactly mirror native 01b04b80..01b04b8c: null data, zero capacity
        // or an initial NUL suppresses the name tag. A failed UTF-8 conversion
        // of a nonempty native name remains Named, never inferred Unnamed.
        if (NameMessageMatches(Name.manager))
            Name.tag = data && units && Read<char16_t>(data) ? NameTag::Named : NameTag::Unnamed;
        if (!Name.source || !Name.handle || !data || !units || units > MaxLabelUnits) return;
        std::array<char16_t, MaxLabelUnits> buffer{};
        std::memcpy(buffer.data(), reinterpret_cast<const void *>(data), units * sizeof(char16_t));
        CopyLabelName({buffer.data(), units}, Name.name);
    }
};
HOOK_DEFINE_INLINE(DialogueHeldInput) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        // Immediately after native active-player/fallback selection; x0 is
        // still the controller on every branch, including the null fallback.
        const auto controller = registers->X[0];
        const auto text = Read<std::uintptr_t>(registers->X[19], layout::ManagerText);
        if (!text) return;
        const std::uint8_t held = controller &&
            (Read<std::uint16_t>(controller, layout::HeldButtons) & layout::BoostButtons);
        std::memcpy(reinterpret_cast<void *>(text + layout::HeldFast), &held, sizeof(held));
#if CAS_VERBOSE_DIALOGUE_TRACE
        TraceMessage(registers->X[19], text);
#endif
    }
};
HOOK_DEFINE_INLINE(DialogueSelectSpeed) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        const auto text = registers->X[19];
        const auto speaker = CurrentSpeaker(text);
        auto speed = SelectSpeed(Preferences, speaker.kind, speaker.id,
                                      Read<std::uint8_t>(text, layout::HeldFast) != 0, speaker.localizedName);
        // Reached only AFTER native forced/protected/acceleration-disabled
        // checks. Native stores this flag and forwards it to both renderers.
        // Store the selected tier, not the numeric multiplier. All flags
        // retain native fast mode; the two rate hooks apply its actual speed.
        registers->W[9] = speed > Preferences.baseSpeed ? 3 : 1;
        if (HurryContext(Read<std::uintptr_t>(text, layout::TextManager), text, 3) && HurryHeld()) {
            speed = HurrySpeed;
            registers->W[9] = HurryFlag;
        }
#if CAS_VERBOSE_DIALOGUE_TRACE
        // Log transitions, not the first frames: title/save dialogs must not
        // consume the entire trace before the user reaches an NPC.
        constexpr const char *reasons[]{
            "no-manager", "no-context", "unsupported-window", "no-source", "empty-handle", "no-registry",
            "invalid-entity", "other-entity-kind", "npc-type-unready", "foreign-npc-method",
            "npc-type-mismatch", "no-npc-name", "npc-name-too-long", "npc", "ui-dialog", "player-entity", "player-bank", "unnamed-tag"
        };
        static Speaker previous{};
        static unsigned previousSpeed = 0, reports = 0;
        const bool changed = !reports || previous.lookup != speaker.lookup ||
            previous.tag != speaker.tag ||
            previous.window != speaker.window || previous.entityKind != speaker.entityKind ||
            previous.handle != speaker.handle || previous.entity != speaker.entity ||
            previous.entityHandle != speaker.entityHandle || previous.entityFlags != speaker.entityFlags ||
            previousSpeed != speed || std::strcmp(previous.id, speaker.id) ||
            std::strcmp(previous.bank, speaker.bank) || std::strcmp(previous.localizedName, speaker.localizedName);
        if (changed && reports < 128) {
            ++reports; previous = speaker; previousSpeed = speed;
            Logging.Debug("[QoL Enhancements/Faster Dialogue] SELECT reason=%s window=%u entity_kind=%u kind=%u npc=%s speed=%u bank=%s handle=%08x entity=0x%lx entity_handle=%08x entity_flags=%08x name=%s name_tag=%u\n",
                        reasons[static_cast<unsigned>(speaker.lookup)], speaker.window, speaker.entityKind,
                        static_cast<unsigned>(speaker.kind), speaker.id, speed, speaker.bank,
                        speaker.handle, speaker.entity, speaker.entityHandle, speaker.entityFlags, speaker.localizedName,
                        static_cast<unsigned>(speaker.tag));
        }
#endif
    }
};
HOOK_DEFINE_INLINE(DialogueAdvanceInput) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        const auto manager = registers->X[19];
        const auto text = Read<std::uintptr_t>(manager, layout::ManagerText);
        if (!HurryContext(manager, text, 5)) return;
        const auto reveal = Read<unsigned>(text, layout::RevealState);
        if (reveal != 2 && reveal != 3) return; // No revealing or embedded-pause bypass.
        const auto timer = Read<int>(manager, layout::AdvanceTimer);
        if (timer != -1 && timer != 0) return; // Preserve authored/countdown waits.
        const auto context = Read<std::uintptr_t>(manager, layout::TalkContext);
        const auto message = Read<std::uintptr_t>(context, layout::MessageController);
        if (!message) return;
        const auto phase = Read<unsigned>(message, layout::ControllerState);
        if (phase != 0 && phase != 2) return; // Idle or ready; never opening/closing.
        if (HurryHeld()) registers->W[22] = 1;
        // Only the local advance request changes. The native function still
        // clears a page (state 2), opens choices (manager+2475), or closes the
        // message (state 3). Y never writes a persistent latch or controller.
    }
};
HOOK_DEFINE_INLINE(DialogueMainRate) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        if (registers->W[20]) registers->S[0] *= RateScale(registers->W[20]);
    }
};
HOOK_DEFINE_INLINE(DialogueForwardSpeed) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        // Native DED0 sets w20 to Boolean 1. Forward the selected 1/3/5 flag
        // after that assignment, before either the rate gate or renderers.
        registers->W[20] = Read<std::uint8_t>(registers->X[19], layout::EffectiveFast);
    }
};
HOOK_DEFINE_INLINE(DialogueRendererRate) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        const auto flag = Read<std::uint8_t>(registers->X[0], layout::RendererFast);
        if (flag) registers->S[0] *= RateScale(flag);
    }
};
} // namespace

unsigned Configure(std::string_view input) {
    return ApplySettings(input, Preferences);
}

void ReportSettings() {
    if (NormalizeSettings(Preferences))
        Logging.Warning("[QoL Enhancements/Faster Dialogue] WARNING: boost_speed below base_speed; using base speed for both\n");
    Logging.Settings("[QoL Enhancements/Faster Dialogue] settings: base=%u boost=%u faster_ui=%s faster_unnamed=%s npc_count=%u\n",
                Preferences.baseSpeed, Preferences.boostSpeed, Preferences.fasterUi ? "true" : "false",
                Preferences.fasterUnnamed ? "true" : "false", Preferences.npcCount);
    for (unsigned i = 0; i < Preferences.npcCount; ++i)
        Logging.Settings("[QoL Enhancements/Faster Dialogue] faster_npc_list[%u]=%s\n", i, Preferences.npcs[i].data());
}

bool Install() {
    // Original instructions are fingerprints only, never replacement payloads.
    // Check all sites/dependencies before installing the first C++ hook. Also
    // reject earlier Faster Interactions releases that change these paths.
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    constexpr Fingerprint fingerprints[]{
        {address::HeldInput, {0xa9422269, 0x8b140275, 0x911be114, 0xf9401128, 0xb4000308}},
        {address::EligibleFast, {0x52843508, 0x8b080268, 0x3934dec9, 0x340006e9, 0x52800034}},
        {address::ForwardFast, {0xb900011f, 0x394832c8, 0x340007c8, 0x3961f2c8, 0x340008c8}},
        {address::MainRate, {0x14000054, 0x52829108, 0x52a7f017, 0x3934dedf, 0x8b080274}},
        {address::RendererRate, {0x14000002, 0xbd421400, 0xbd420801, 0xbd420c02, 0x5e21d821}},
        {address::NameBegin, {0xf9400008, 0x9105e3f4, 0xf9406118, 0xf9401008, 0xf9410d16}},
        {address::NameReady, {0xb4001153, 0x34001120, 0x79400268, 0x340010e8, 0xf94186c8}},
        {address::AdvanceInput, {0xb964666b, 0x37f801ab, 0x3400024b, 0x5100096c, 0x7100057f}},
        {address::ActiveController, {0xd10083ff, 0xa9017bfd, 0x910043fd, 0x90013ce8, 0x91276108}},
        {0x01b048ec, {0xf9409708, 0x9104a313, 0xaa1303e0, 0xf9400908, 0xd63f0100}},
        {0x01b04b5c, {0xf94007e8, 0x910023e0, 0xf9400d08, 0xd63f0100, 0xf94007e8}},
        {0x005f5b28, {0xf9400400, 0xd65f03c0, 0xb9401000, 0xd65f03c0, 0xa9bd7bfd}},
        {0x01af89d0, {0xaa1803e0, 0xf9524676, 0xf9400d08, 0xd63f0100, 0xf9400717}},
        {0x01d9dc3c, {0x528f3388, 0xf941d829, 0x8b080008, 0xf81d83a8, 0x528f0d88}},
        {0x01d9e11c, {0xa97de7b4, 0x910042ea, 0xeb08029f, 0xf93cca74, 0x3900fe9f}},
        {0x01d9e224, {0x528fec08, 0x8b08026a, 0xb94232c8, 0xf9461d29, 0x39174295}},
        {0x01d9e244, {0xb905ce88, 0x91004128, 0xb00168a9, 0x52800814, 0xf93fb268}},
        {0x01afdd20, {0xd10183ff, 0xa9027bfd, 0xf9001bf7, 0xa90457f6, 0xa9054ff4}},
        {0x01af9fec, {0x79401008, 0x528c0049, 0x6a09011f, 0x540000a0, 0xf9401668}},
        {0x01afdeac, {0x350002a9, 0x35000768, 0x3974dac8, 0x35000728, 0x3974d2c9}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &expected : fingerprints) {
        if (expected.offset + sizeof(expected.words) > main.m_Text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(Main(expected.offset)), expected.words,
                        sizeof(expected.words))) {
            Logging.Error("[QoL Enhancements/Faster Dialogue] DISABLED: instruction mismatch at main+0x%lx\n", expected.offset);
            return false;
        }
    }
#if CAS_VERBOSE_DIALOGUE_TRACE
    Logging.Debug("[QoL Enhancements/Faster Dialogue] DIAGNOSTIC: unnamed-v1; current-message native name-tag classification\n");
#endif
    DialogueNameBegin::InstallAtOffset(address::NameBegin);
    DialogueNameReady::InstallAtOffset(address::NameReady);
    DialogueHeldInput::InstallAtOffset(address::HeldInput);
    DialogueSelectSpeed::InstallAtOffset(address::EligibleFast);
    DialogueForwardSpeed::InstallAtOffset(address::ForwardFast);
    DialogueMainRate::InstallAtOffset(address::MainRate);
    DialogueRendererRate::InstallAtOffset(address::RendererRate);
    DialogueAdvanceInput::InstallAtOffset(address::AdvanceInput);
    Logging.Debug("[QoL Enhancements/Faster Dialogue] READY: C++ hooks; hold Y to hurry at %ux until choices/end\n", HurrySpeed);
    return true;
}
} // namespace qol::dialogue
