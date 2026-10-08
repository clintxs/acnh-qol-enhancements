#include "lib.hpp"
#include "settings.hpp"
#include <nn/fs/fs_files.hpp>
#include <atomic>
#include <cstring>

namespace qol::dialogue {
namespace {
namespace address {
constexpr std::uintptr_t HeldInput = 0x01afa00c, EligibleFast = 0x01afdec0;
constexpr std::uintptr_t ForwardFast = 0x01afded4;
constexpr std::uintptr_t MainRate = 0x01afdefc, RendererRate = 0x01a969e0;
constexpr std::uintptr_t EntityRegistryGot = 0x04ab27c0;
constexpr std::uintptr_t NpcTypeGot = 0x04ab37d0, NpcTypeGuardGot = 0x04ab37d8;
} // namespace address
namespace layout {
constexpr std::size_t TextManager = 0x208, ManagerText = 0x28, TalkContext = 0x20;
constexpr std::size_t WindowKind = 0x2c, ActiveSpeaker = 0xc0;
constexpr std::size_t SpeakerEntityDelegate = 0x28, DelegateValid = 0x10;
constexpr std::size_t HeldButtons = 8, HeldFast = 0x21b4, EffectiveFast = 0x21b7, RendererFast = 0x205;
constexpr std::size_t EntityTable = 0x1c0, TableEntries = 8;
constexpr std::size_t EntityHandle = 0x230, EntityKind = 0x15c, EntityFlags = 0x248;
constexpr std::size_t EntityResourceName = 0x3e8;
constexpr std::size_t IsNpcType = 0x20;
constexpr unsigned NpcEntity = 10, PlayerEntity = 0x4f, DialogWindow = 1;
constexpr std::uint16_t BoostButtons = 0x4002; // B/R; L is deliberately excluded.
} // namespace layout
Settings Preferences{};
template <class T> T Read(std::uintptr_t object, std::size_t offset = 0) {
    T result;
    std::memcpy(&result, reinterpret_cast<const void *>(object + offset), sizeof(result));
    return result;
}
std::uintptr_t Main(std::uintptr_t offset) {
    return exl::util::GetMainModuleInfo().m_Text.m_Start + offset;
}
bool NativeFunction(std::uintptr_t function) {
    const auto &text = exl::util::GetMainModuleInfo().m_Text;
    return function && function >= text.m_Start && function - text.m_Start < text.m_Size && !(function & 3);
}
struct Speaker {
    MessageKind kind = MessageKind::Unknown;
    char id[NpcIdBytes]{};
};
Speaker CurrentSpeaker(std::uintptr_t text) {
    Speaker result{};
    const auto manager = Read<std::uintptr_t>(text, layout::TextManager);
    if (!manager) return result;
    const auto context = Read<std::uintptr_t>(manager, layout::TalkContext);
    if (!context) return result;
    // Native 01b041a4 selects talk/dialog/balloon from the current message's
    // presentation metadata. A confirmed dialog is UI, even after NPC speech.
    const auto window = Read<unsigned>(context, layout::WindowKind);
    if (window == layout::DialogWindow) { result.kind = MessageKind::Ui; return result; }
    if (window != 0) return result; // Balloon/unknown is not automatically UI.
    const auto source = Read<std::uintptr_t>(manager, layout::ActiveSpeaker);
    if (!source) return result; // Absence alone is NOT proof of a UI message.
    const auto delegate = source + layout::SpeakerEntityDelegate;
    const auto methods = Read<std::uintptr_t>(delegate);
    if (!methods) return result;
    const auto valid = Read<std::uintptr_t>(methods, layout::DelegateValid);
    const auto resolve = Read<std::uintptr_t>(methods);
    if (!NativeFunction(valid) || !NativeFunction(resolve)) return result;
    // Same borrowed delegate, validity check and 12-byte output as the native
    // Animalese update at 01b02e84..01b02ebc. No voice playback is invoked.
    if (!(reinterpret_cast<std::uint64_t (*)(std::uintptr_t)>(valid)(delegate) & 1)) return result;
    std::array<std::uint32_t, 3> handle{};
    reinterpret_cast<void (*)(std::uintptr_t, void *)>(resolve)(delegate, handle.data());
    if (!handle[0]) return result;
    const auto registrySlot = Read<std::uintptr_t>(Main(address::EntityRegistryGot));
    const auto registry = registrySlot ? Read<std::uintptr_t>(registrySlot) : 0;
    const auto table = registry ? Read<std::uintptr_t>(registry, layout::EntityTable) : 0;
    const auto entries = table ? Read<std::uintptr_t>(table, layout::TableEntries) : 0;
    if (!entries) return result;
    // Mirror 01b000fc..01b00178: handles contain a 12-bit table index and
    // generation. This handle comes from a live native speaker delegate.
    const auto entity = Read<std::uintptr_t>(entries, (handle[0] >> 20) * sizeof(std::uintptr_t));
    if (!entity || Read<unsigned>(entity, layout::EntityHandle) != handle[0] ||
        (Read<unsigned>(entity, layout::EntityFlags) & 2)) return result;
    const auto kind = Read<unsigned>(entity, layout::EntityKind);
    if (kind == layout::PlayerEntity) { result.kind = MessageKind::Ui; return result; }
    if (kind != layout::NpcEntity) return result;
    // Match native 01b00164..01b001e8's NPC RTTI check before borrowing the
    // NPC-specific name field. Never initialize RTTI from a text hook: native
    // NPC voice setup initializes it first; an unavailable type uses base.
    const auto type = Read<std::uintptr_t>(Main(address::NpcTypeGot));
    const auto guard = Read<std::uintptr_t>(Main(address::NpcTypeGuardGot));
    if (!type || !guard ||
        !(std::atomic_ref<const std::uint8_t>(*reinterpret_cast<const std::uint8_t *>(guard))
              .load(std::memory_order_acquire) & 1)) return result;
    const auto entityMethods = Read<std::uintptr_t>(entity);
    const auto isNpc = entityMethods ? Read<std::uintptr_t>(entityMethods, layout::IsNpcType) : 0;
    if (!NativeFunction(isNpc) ||
        !(reinterpret_cast<std::uint64_t (*)(std::uintptr_t, std::uintptr_t)>(isNpc)(entity, type) & 1)) return result;
    result.kind = MessageKind::Npc;
    // Native NPC voice setup uses this SafeString's data (01b000c8). Resource
    // IDs distinguish rcm/rct even when the message bank is shared.
    const auto name = Read<std::uintptr_t>(entity, layout::EntityResourceName);
    if (!name) return result;
    for (unsigned i = 0; i < NpcIdBytes; ++i) {
        const auto c = Read<char>(name, i);
        if (!c) return result;
        if (i == NpcIdBytes - 1) break;
        result.id[i] = c;
    }
    result.id[0] = 0; // Never match a truncated resource ID.
    return result;
}
void LoadPreferences() {
    Preferences = {};
    nn::fs::FileHandle file{};
    const auto opened = nn::fs::OpenFile(&file, CustomConfigPath, nn::fs::OpenMode_Read);
    if (opened == 0x202) return;
    if (opened) {
        Logging.Log("[QoL Enhancements/Faster Dialogue] WARNING: custom config open=0x%x; defaults\n", opened);
        return;
    }
    char buffer[core::MaxConfigBytes];
    long size = 0;
    bool readable = nn::fs::GetFileSize(&size, file) == 0 && size >= 0 &&
                    static_cast<unsigned long>(size) <= sizeof(buffer);
    if (readable && size) readable = nn::fs::ReadFile(file, 0, buffer, size) == 0;
    nn::fs::CloseFile(file);
    const auto warnings = readable ? ParseSettings({buffer, static_cast<std::size_t>(size)}, Preferences) : 1;
    if (warnings) Logging.Log("[QoL Enhancements/Faster Dialogue] WARNING: custom config issues=%u; invalid settings use defaults\n", warnings);
}

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
    }
};
HOOK_DEFINE_INLINE(DialogueSelectSpeed) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        const auto text = registers->X[19];
        const auto speaker = CurrentSpeaker(text);
        const auto speed = SelectSpeed(Preferences, speaker.kind, speaker.id,
                                      Read<std::uint8_t>(text, layout::HeldFast) != 0);
        // Reached only AFTER native forced/protected/acceleration-disabled
        // checks. Native stores this flag and forwards it to both renderers.
        // 1 = native fast, 3 = native fast plus extra multiplier (old 0.3 tier).
        registers->W[9] = speed == 4 ? 3 : 1;
#if CAS_VERBOSE_DIALOGUE_TRACE
        static unsigned reports = 0;
        if (reports++ < 64)
            Logging.Log("[QoL Enhancements/Faster Dialogue] SELECT kind=%u npc=%s speed=%u\n",
                        static_cast<unsigned>(speaker.kind), speaker.id, speed);
#endif
    }
};
HOOK_DEFINE_INLINE(DialogueMainRate) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        if (registers->W[20] & 2) registers->S[0] *= 2.0f;
    }
};
HOOK_DEFINE_INLINE(DialogueForwardSpeed) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        // Native DED0 sets w20 to Boolean 1. Forward the selected 1/3 flag
        // after that assignment, before either the rate gate or renderers.
        registers->W[20] = Read<std::uint8_t>(registers->X[19], layout::EffectiveFast);
    }
};
HOOK_DEFINE_INLINE(DialogueRendererRate) {
    static void Callback(exl::hook::InlineFloatCtx *registers) {
        if (Read<std::uint8_t>(registers->X[0], layout::RendererFast) & 2)
            registers->S[0] *= 2.0f;
    }
};
} // namespace

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
        {0x01afdd20, {0xd10183ff, 0xa9027bfd, 0xf9001bf7, 0xa90457f6, 0xa9054ff4}},
        {0x01af9fec, {0x79401008, 0x528c0049, 0x6a09011f, 0x540000a0, 0xf9401668}},
        {0x01afdeac, {0x350002a9, 0x35000768, 0x3974dac8, 0x35000728, 0x3974d2c9}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &expected : fingerprints) {
        if (expected.offset + sizeof(expected.words) > main.m_Text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(Main(expected.offset)), expected.words,
                        sizeof(expected.words))) {
            Logging.Log("[QoL Enhancements/Faster Dialogue] DISABLED: instruction mismatch at main+0x%lx\n", expected.offset);
            return false;
        }
    }
    LoadPreferences();
    DialogueHeldInput::InstallAtOffset(address::HeldInput);
    DialogueSelectSpeed::InstallAtOffset(address::EligibleFast);
    DialogueForwardSpeed::InstallAtOffset(address::ForwardFast);
    DialogueMainRate::InstallAtOffset(address::MainRate);
    DialogueRendererRate::InstallAtOffset(address::RendererRate);
    Logging.Log("[QoL Enhancements/Faster Dialogue] READY: C++ hooks; base=%u boost=%u faster_ui=%s npc_count=%u\n",
                Preferences.baseSpeed, Preferences.boostSpeed, Preferences.fasterUi ? "true" : "false", Preferences.npcCount);
    return true;
}
} // namespace qol::dialogue
