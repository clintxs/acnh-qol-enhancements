#include "lib.hpp"
#include "hold_y.hpp"
#include <array>
#include <cstring>

namespace qol::controls {
namespace {
namespace address {
constexpr std::uintptr_t InputUpdate = 0x02461f30;
constexpr std::uintptr_t PickupPressGate = 0x02222a34;
constexpr std::uintptr_t TryPickup = 0x02326e2c;
constexpr std::uintptr_t CheckFurniture = 0x023282d0;
constexpr std::uintptr_t ReservePickup = 0x021baf28;
constexpr std::uintptr_t ItemId = 0x0267d6c4, FieldKind = 0x0113db50;
constexpr std::uintptr_t FieldParameters = 0x04abbc68, ItemParameters = 0x04ac0428;
}
namespace field {
constexpr std::size_t PlayerInput = 0xec0, InputOwner = 0x48;
constexpr std::size_t HeldActions = 8, PressedActions = 0x10;
constexpr std::size_t PlayerHandle = 0x230, Resident = 0x1ee8;
constexpr std::size_t ParameterCount = 0x1d8, ParameterRows = 0x1e0, ParameterFirstId = 0x1f8;
}
// The first input pass stores physical Y as action 38; the later context
// mapper produces pickup action 3. Poll the physical bit during animations,
// before context mapping, and extend only the pickup branch after mapping.
constexpr std::uint64_t HeldY = 1ull << 38;
constexpr unsigned PickupAction = 1u << 3;
enum class Action { None, Item, Crop, Weed, Moss };
struct Hold {
    void *player{};
    unsigned handle{};
    Action action{};
    bool firstPress{};
};
std::array<Hold, 8> Holds{};
std::uintptr_t Main{};
// Only the synchronous native picker may capture/filter reservations. Other
// callers of the shared reservation service retain their normal behavior.
struct PickupScope { Hold *hold{}; bool repeating{}; };
PickupScope Scope{};
template<class T> T Read(const void *object, std::size_t offset = 0) {
    T value;
    std::memcpy(&value, static_cast<const std::byte *>(object) + offset, sizeof(value));
    return value;
}
template<class F> F Native(std::uintptr_t offset) { return reinterpret_cast<F>(Main + offset); }
Hold *ForPlayer(void *player) {
    if (!player) return nullptr;
    const auto resident = Read<unsigned char>(player, field::Resident);
    if (resident >= Holds.size()) return nullptr;
    auto &hold = Holds[resident];
    if (hold.player != player || hold.handle != Read<unsigned>(player, field::PlayerHandle)) return nullptr;
    return &hold;
}
const void *ParameterRow(std::uintptr_t got, unsigned id) {
    // GOT -> resource singleton slot -> loaded parameter table. Never invoke
    // lazy initialization or manufacture a fallback row for an unknown item.
    const auto slot = *Native<const void **>(got);
    const auto table = slot ? Read<const void *>(slot) : nullptr;
    if (!table) return nullptr;
    const auto first = Read<unsigned short>(table, field::ParameterFirstId);
    const auto count = Read<unsigned>(table, field::ParameterCount);
    const auto rows = Read<const void *const *>(table, field::ParameterRows);
    return rows && count <= 65536 && id >= first && id - first < count ? rows[id - first] : nullptr;
}
Action ClassifySource(const void *item) {
    if (!item) return Action::None;
    const auto id = Native<unsigned (*)(const void *)>(address::ItemId)(item);
    if (const auto row = ParameterRow(address::FieldParameters, id)) {
        // FgMainParam's native enum: crops 18..23, seasonal weeds 24..30,
        // and glowing moss (WeedLight) 31. Keep weed and moss holds separate.
        // Flowers, trees, vines, placed ladders/fences and all other field
        // objects are deliberately excluded from the four hold actions.
        const auto kind = Native<unsigned short (*)(const void *, const void *)>(address::FieldKind)(item, row);
        if (kind >= 0x12 && kind <= 0x17) return Action::Crop;
        if (kind >= 0x18 && kind <= 0x1e) return Action::Weed;
        return kind == 0x1f ? Action::Moss : Action::None;
    }
    return ParameterRow(address::ItemParameters, id) ? Action::Item : Action::None;
}
HOOK_DEFINE_TRAMPOLINE(HooverInput) {
    static unsigned long Callback(void *input, const void *request) {
        const auto result = Orig(input, request);
        const auto player = Read<void *>(input, field::InputOwner);
        if (!player || static_cast<std::byte *>(player) + field::PlayerInput != input) return result;
        const auto resident = Read<unsigned char>(player, field::Resident);
        if (resident >= Holds.size()) return result;
        auto &hold = Holds[resident];
        const auto handle = Read<unsigned>(player, field::PlayerHandle);
        const bool held = Read<std::uint64_t>(input, field::HeldActions) & HeldY;
        const bool pressed = Read<std::uint64_t>(input, field::PressedActions) & HeldY;
        if (hold.player != player || hold.handle != handle || !held || pressed)
            hold = {player, handle, Action::None, held && pressed};
        else
            hold.firstPress = false;
        return result;
    }
};
HOOK_DEFINE_INLINE(HooverPressGate) {
    static void Callback(exl::hook::InlineCtx *ctx) {
        const auto player = reinterpret_cast<void *>(ctx->X[19]);
        const auto hold = ForPlayer(player);
        // Preserve the native fresh-press value. Only this Y branch sees a
        // repeated request; no controller snapshot or other action is edited.
        if (hold && hold->action != Action::None && !hold->firstPress &&
            (Read<std::uint64_t>(static_cast<std::byte *>(player) + field::PlayerInput, field::HeldActions) & HeldY))
            ctx->W[8] |= 1;
    }
};
HOOK_DEFINE_TRAMPOLINE(HooverReservation) {
    static unsigned Callback(void *service, void *request, const void *source, const void *resultItem,
                             const void *position, const void *flags) {
        const auto action = Scope.hold ? ClassifySource(source) : Action::None;
        // Reject before the native service reserves a field object, writes a
        // request or performs any inventory work. Recheck the ACTUAL source:
        // the picker's probe and commit can choose different nearby targets.
        if (Scope.repeating && (action == Action::None || action != Scope.hold->action)) return 0;
        const auto result = Orig(service, request, source, resultItem, position, flags);
        if ((result & 1) && Scope.hold && Scope.hold->firstPress && !Scope.repeating)
            Scope.hold->action = action;
        return result;
    }
};
HOOK_DEFINE_TRAMPOLINE(HooverFurniture) {
    static unsigned Callback(void *player, const void *candidate, void *item, void *position, unsigned flags) {
        // This branch handles placed furniture/error dialogs, before the
        // ground-object reservation service. Never enter it for a repeat.
        if (Scope.repeating) return 0;
        return Orig(player, candidate, item, position, flags);
    }
};
HOOK_DEFINE_TRAMPOLINE(HooverPickup) {
    static unsigned Callback(void *player, unsigned probe, void *item, void *position, unsigned flags) {
        auto hold = ForPlayer(player);
        const auto previous = Scope;
        const bool repeat = hold && !hold->firstPress && hold->action != Action::None &&
                            !(Read<unsigned>(static_cast<std::byte *>(player) + field::PlayerInput,
                                             field::PressedActions) & PickupAction);
        if (probe || !hold || (!repeat && !hold->firstPress))
            return Orig(player, probe, item, position, flags);
        Scope = {hold, repeat};
        unsigned result;
        if (repeat) {
            // Native dry-run returns the current source and position without
            // reserving it. The final argument excludes furniture/fence fallback paths.
            std::uint64_t candidate = 0xfffe;
            std::array<float, 3> target{};
            if ((Orig(player, 1, &candidate, target.data(), 1) & 1) &&
                ClassifySource(&candidate) == hold->action)
                Orig(player, 0, nullptr, nullptr, 1);
            // Consume this private repeat even if nothing matches. Native
            // Y fallbacks (such as erasing a ground design) must not run.
            result = 1;
        } else {
            result = Orig(player, probe, item, position, flags);
            hold->firstPress = false; // Only the first attempted Y action can latch.
        }
        Scope = previous;
        return result;
    }
};
}
bool InstallHoldYPickup() {
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    constexpr Fingerprint fingerprints[]{
        {address::InputUpdate, {0xd10343ff,0xfd002bea,0x6d0623e9,0xa9077bfd,0xa9086ffc}},
        {address::PickupPressGate, {0x36001388,0xf9400308,0xb4000248,0xb9415d09,0x34000209}},
        {address::TryPickup, {0xfc180fea,0x6d0123e9,0xa9027bfd,0x910083fd,0xa9036ffc}},
        {address::ReservePickup, {0xd10503ff,0xa90e7bfd,0xa90f6ffc,0xa91067fa,0xa9115ff8}},
        {address::CheckFurniture, {0xa9ba7bfd,0xa9016ffc,0x910003fd,0xa90267fa,0xa9035ff8}},
        {address::ItemId, {0xa9be7bfd,0xf9000bf3,0x910003fd,0xaa0003e8,0x79400000}},
        {address::FieldKind, {0xa9be7bfd,0xf9000bf3,0x910003fd,0xb4000481,0xb9400028}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &f : fingerprints) if (f.offset + sizeof(f.words) > main.m_Text.m_Size ||
        std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + f.offset), f.words, sizeof(f.words))) {
        Logging.Error("[QoL Enhancements/Controls] hold Y: DISABLED; instruction mismatch at main+0x%lx\n", f.offset);
        return false;
    }
    Main = main.m_Text.m_Start;
    HooverInput::InstallAtOffset(address::InputUpdate);
    HooverPressGate::InstallAtOffset(address::PickupPressGate);
    HooverPickup::InstallAtOffset(address::TryPickup);
    HooverReservation::InstallAtOffset(address::ReservePickup);
    HooverFurniture::InstallAtOffset(address::CheckFurniture);
    Logging.Debug("[QoL Enhancements/Controls] hold Y: READY; item/crop/weed/moss lock; native pickup rules\n");
    return true;
}
}
