#include "program/core/hook_plan.hpp"
#include "lib.hpp"
#include "settings.hpp"
#include "program/core/page_input.hpp"
#include <array>
#include <cstring>

namespace qol::crafting {
namespace {
namespace address {
constexpr std::uintptr_t EnterPreview = 0x03494b2c, LeavePreview = 0x03494e7c;
constexpr std::uintptr_t BuildFooter = 0x0346ec1c, WorkbenchPage = 0x04a3b3e0;
constexpr std::uintptr_t KitchenBuildFooter = 0x02cbb99c, KitchenPage = 0x049c88a0;
constexpr std::uintptr_t KitchenPageGetter = 0x02cbb710;
constexpr std::uintptr_t AppendHint = 0x02de2b00, ResolveCaption = 0x02de4940;
constexpr std::uintptr_t ApplicationGlobalGot = 0x04ab2230, Application = 0x059f83f0;
constexpr std::uintptr_t FooterTypeGot = 0x04ac7278, FooterTypeGuardGot = 0x04ac7280;
constexpr std::uintptr_t BufferedStringVtableGot = 0x04ab2bc0, SafeStringVtableGot = 0x04ab2020;
}
namespace layout {
constexpr std::size_t ApplicationFooter = 0x58, HintCount = 0x218, HintCapacity = 0x21c;
constexpr std::size_t HintArray = 0x220, HintFreeList = 0x228;
constexpr std::size_t State = 0x1b0, Pending = 0x1f0, PendingSlot = 0x200;
constexpr std::size_t Resident = 0x1f8, Recipe = 0x264, FooterFlags = 0x239;
constexpr std::size_t Result = 0x1c1c8, Action = 0x1c1cc, MaterialStatus = 0x1c1dc;
constexpr std::size_t QuantityChild = 0x1c1e8, SupportsMultiple = 0x1c1f0;
constexpr std::size_t PageGetter = 0x218, ValidRecipe = 0x288, CanCraft = 0x290, MultipleRecipe = 0x2a8;
constexpr std::size_t PageInput = 0x210, Shortcut = 0x570, ShortcutKey = 0xa0;
}
constexpr unsigned PreviewState = 3, QuantityState = 9, XMask = 0x40, YMask = 0x80;
constexpr char QuantityHintKey[] = "qol_crafting.set_quantity";
std::uintptr_t Main{};
std::uintptr_t MainAddress(std::uintptr_t offset) { return Main + offset; }
template<class T> T &Field(void *p, std::size_t offset = 0) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(p) + offset);
}
template<class F> F Virtual(void *object, std::size_t offset) {
    return Field<F>(Field<void *>(object), offset);
}
struct SafeString {
    std::uintptr_t vtable{};
    const char *text{};
};
struct HintDescription {
    int button = -1, secondary = -1, otherButton = -1, otherSecondary = -1;
    SafeString caption{};
    int capacity = 64;
    char key[64]{};
    std::uint32_t padding{};
    std::uint8_t player = 0xff;
    std::uint8_t playerPadding[3]{};
    int appearance = 0;
    std::uint8_t alternate = 0;
    std::uint8_t alternatePadding[3]{};
    int alternateButton = -1, alternateSecondary = -1;
};
static_assert(offsetof(HintDescription, caption) == 0x10);
static_assert(offsetof(HintDescription, key) == 0x24);
static_assert(offsetof(HintDescription, player) == 0x68);
static_assert(offsetof(HintDescription, alternateButton) == 0x74);
static_assert(sizeof(HintDescription) == 0x80);
SafeString QuantityBank, QuantityCaption;
struct PreviewBinding { void *sequence{}, *page{}; bool armed{}, favoriteArmed{}; };
std::array<PreviewBinding, 8> Bindings{};
void *PreviewPage(void *sequence) {
    auto page = Virtual<void *(*)(void *)>(sequence, layout::PageGetter)(sequence);
    if (!page) return nullptr;
    // CookingTableInfoSeq inherits the workbench's Play/MultiCreation state
    // machine and fields, but supplies its own page and footer builder.
    const auto type = Field<std::uintptr_t>(page);
    return type == Main + address::WorkbenchPage || type == Main + address::KitchenPage ? page : nullptr;
}
bool SupportsQuantity(void *sequence) {
    if (!Field<unsigned char>(sequence, layout::SupportsMultiple) ||
        !Field<void *>(sequence, layout::QuantityChild)) return false;
    const int recipe = Field<int>(sequence, layout::Recipe);
    for (const auto method : {layout::ValidRecipe, layout::CanCraft, layout::MultipleRecipe})
        if (!(Virtual<unsigned (*)(void *, int)>(sequence, method)(sequence, recipe) & 1)) return false;
    return true;
}
void *PreviewFooter() {
    auto **applicationGlobal = *reinterpret_cast<void ***>(
        MainAddress(address::ApplicationGlobalGot));
    auto *application = applicationGlobal ? *applicationGlobal : nullptr;
    if (!application) {
        return nullptr;
    }
    auto *footer = Field<void *>(application, layout::ApplicationFooter);
    const auto *guard = *reinterpret_cast<const std::uint8_t **>(
        MainAddress(address::FooterTypeGuardGot));
    if (!footer || !guard || !(*guard & 1)) {
        return nullptr;
    }
    // The same native type check as 0346EC1C. Do not interpret a
    // different controller's fields if the application changes its footer.
    auto *type = *reinterpret_cast<void **>(MainAddress(address::FooterTypeGot));
    auto isType = *Field<unsigned (**)(void *, void *)>(footer, 0);
    return (isType(footer, type) & 1) ? footer : nullptr;
}

void AppendQuantityHint(void *sequence) {
    if (!PreviewPage(sequence) || !SupportsQuantity(sequence)) return;
    auto footer = PreviewFooter();
    if (!footer) return;
    const int count = Field<int>(footer, layout::HintCount);
    auto entries = Field<HintDescription **>(footer, layout::HintArray);
    if (count < 0 || count >= 5 || count >= Field<int>(footer, layout::HintCapacity) ||
        !entries || !Field<void *>(footer, layout::HintFreeList)) return;
    for (int i = 0; i < count; ++i)
        if (entries[i] && entries[i]->caption.text &&
            std::strcmp(entries[i]->caption.text, QuantityHintKey) == 0) return;
    HintDescription hint;
    hint.button = 7; // Native KeyGuide Y icon (input snapshot mask is 0x80).
    hint.player = Field<unsigned char>(sequence, layout::Resident);
    if (hint.player > 7) hint.player = 0xff;
    hint.caption.vtable = *reinterpret_cast<const std::uintptr_t *>(Main + address::BufferedStringVtableGot) + 16;
    hint.caption.text = hint.key;
    std::memcpy(hint.key, QuantityHintKey, sizeof(QuantityHintKey));
    reinterpret_cast<void (*)(void *, const HintDescription *, unsigned)>(Main + address::AppendHint)(footer, &hint, 1);
}
template<class BuildNativeFooter>
void BuildShortcutHints(void *sequence, BuildNativeFooter buildNative) {
    auto page = PreviewPage(sequence);
    auto hint = page ? Field<void *>(page, layout::Shortcut) : nullptr;
    auto key = hint ? Field<void *>(hint, layout::ShortcutKey) : nullptr;
    // Both native footer builders read this key solely for Favorite's
    // descriptor. Use the X icon during that synchronous copy; retain the
    // native key binding afterwards. Our subscriber handles X explicitly.
    const unsigned button = key ? Field<unsigned>(key, 0x14) : 0;
    if (key) Field<unsigned>(key, 0x14) = 6;
    buildNative(sequence);
    if (key) Field<unsigned>(key, 0x14) = button;
    AppendQuantityHint(sequence);
}
HOOK_DEFINE_TRAMPOLINE(BuildQuantityShortcutHint) {
    static void Callback(void *sequence) {
        BuildShortcutHints(sequence, [](void *owner) { Orig(owner); });
    }
};
HOOK_DEFINE_TRAMPOLINE(BuildCookingQuantityShortcutHint) {
    static void Callback(void *sequence) {
        BuildShortcutHints(sequence, [](void *owner) { Orig(owner); });
    }
};
HOOK_DEFINE_INLINE(ResolveQuantityShortcutCaption) {
    static void Callback(exl::hook::InlineCtx *registers) {
        // x2/x3 are already the message bank/key at 02DE4940, before their
        // 02DE4954 lookup. The 20-byte window ends before Pockets' separate
        // caption hook, which leaves this private token untouched.
        auto key = reinterpret_cast<const SafeString *>(registers->X[3]);
        if (key && key->text && std::strcmp(key->text, QuantityHintKey) == 0) {
            registers->X[2] = reinterpret_cast<std::uintptr_t>(&QuantityBank);
            registers->X[3] = reinterpret_cast<std::uintptr_t>(&QuantityCaption);
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(EnterQuantityShortcutPreview) {
    static void Callback(void *sequence) {
        Orig(sequence);
        const auto resident = Field<unsigned char>(sequence, layout::Resident);
        if (resident < Bindings.size()) Bindings[resident] = {sequence, PreviewPage(sequence), false};
    }
};
HOOK_DEFINE_TRAMPOLINE(LeaveQuantityShortcutPreview) {
    static void Callback(void *sequence) {
        for (auto &binding : Bindings) if (binding.sequence == sequence) binding = {};
        Orig(sequence);
    }
};
bool QuantityShortcutInput(void *page, void *snapshot, unsigned blocked) {
    if (!page || !snapshot) return false;
    for (auto &binding : Bindings) {
        if (!binding.sequence || binding.page != page) continue;
        auto sequence = binding.sequence;
        auto input = Field<void *>(page, layout::PageInput);
        auto hint = Field<void *>(page, layout::Shortcut);
        auto key = hint ? Field<void *>(hint, layout::ShortcutKey) : nullptr;
        const unsigned resident = key ? Field<unsigned>(key, 0x10) : 8;
        auto rows = Field<std::byte *>(snapshot, 0x10);
        bool unavailable = (blocked & 1) || !input || Field<unsigned char>(input, 0x54);
        if (!unavailable && Field<unsigned>(input, 0x58)) {
            auto app = *reinterpret_cast<void **>(Main + address::Application);
            auto masks = app ? Field<void *>(app, 0x418) : nullptr;
            unavailable = !masks || (Field<unsigned>(input, 0x58) & Field<unsigned>(masks, 0x10));
        }
        if (unavailable || !rows || resident >= 8 || Field<unsigned>(snapshot, 8) > 8 ||
            resident >= Field<unsigned>(snapshot, 8) || Field<unsigned>(sequence, layout::State) != PreviewState) {
            binding.armed = binding.favoriteArmed = false; return false;
        }
        auto row = rows + resident * 0x48;
        const unsigned buttons = Field<unsigned>(row);
        const unsigned consumed = Field<unsigned>(row, 0x3c);
        const bool fresh = binding.armed, freshFavorite = binding.favoriteArmed;
        binding.armed = !(buttons & YMask);
        binding.favoriteArmed = !(buttons & XMask);
        // Y used to be Favorite here. Consume every Y sample, including held
        // or ineligible presses, so it can never favorite instead of opening
        // quantity. Other pages keep the native controls.
        Field<unsigned>(row, 0x3c) |= buttons & YMask;
        if ((buttons & 0x30) || Field<int>(sequence, layout::Result) != -1) return false;
        if (freshFavorite && (buttons & XMask) && !(buttons & YMask) && !(consumed & XMask) &&
            Field<int>(sequence, layout::Action) == -1) {
            // The native Favorite delegate (03493018) queues action 3 in
            // +1C1CC. Play update runs the existing favorite toggle normally.
            Field<int>(sequence, layout::Action) = 3;
            Field<unsigned>(row, 0x3c) |= XMask;
        }
        if (!fresh || !(buttons & YMask) || (consumed & YMask) ||
            !SupportsQuantity(sequence)) return false;
        const unsigned slot = Field<unsigned char>(sequence, layout::PendingSlot);
        if (slot > 1 || Field<int>(sequence, layout::Pending + slot * 4) != -1) return false;
        auto child = Field<void *>(sequence, layout::QuantityChild);
        if (!(Virtual<unsigned (*)(void *)>(child, 0xf0)(child) & 1)) return false;
        // Native cMultiCreation (03496160) supplies recipe/material limits,
        // attaches the preallocated selector and opens it. Its update handles
        // crafting and B -> cPlay. cTransitToMultiCreation only waits for the
        // confirmation dialog to close; there is no dialog to wait for here.
        Field<int>(sequence, layout::MaterialStatus) = 0;
        Field<int>(sequence, layout::Action) = -1;
        Field<unsigned char>(sequence, layout::FooterFlags) |= 4;
        Field<int>(sequence, layout::Pending + slot * 4) = QuantityState;
        Field<unsigned>(row, 0x3c) |= YMask;
        Logging.Debug("[QoL Enhancements/Crafting] quantity shortcut: open; recipe=%d resident=%u\n",
                    Field<int>(sequence, layout::Recipe), resident);
        return true;
    }
    return false;
}
core::InputOutcome QuantityShortcutPageInput(const core::PageInputContext &input) {
    return QuantityShortcutInput(input.page, input.snapshot, input.blocked) ? core::InputOutcome::Consume : core::InputOutcome::PassThrough;
}

} // namespace
bool InstallCraftingQuantityShortcut() {
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    constexpr Fingerprint checks[]{
        {address::EnterPreview,{0xd10143ff,0xa9027bfd,0xf9001bf5,0xa9044ff4,0x910083fd}},
        {address::LeavePreview,{0xd10103ff,0xa9027bfd,0xa9034ff4,0x910083fd,0xf9400008}},
        {address::BuildFooter,{0xd102c3ff,0xa9087bfd,0xa90957f6,0xa90a4ff4,0x910203fd}},
        {address::KitchenBuildFooter,{0xd102c3ff,0xa9087bfd,0xa90957f6,0xa90a4ff4,0x910203fd}},
        // Kitchen's inherited state machine must keep the same page getter.
        {address::KitchenPageGetter,{0x52983f08,0x72a00028,0xf8686800,0xd65f03c0,0x52983f08}},
        {address::ResolveCaption,{0xf90213e8,0x9000e6e8,0xf945f908,0x91004108,0xf9004be8}},
        {0x03496160,{0xd103c3ff,0xa90d7bfd,0xa90e4ff4,0x910343fd,0x91407008}},
        {address::AppendHint,{0xb941f008,0x3100051f,0xb941f409,0xb941b00a,0x3a410920}},
    };
    Main = exl::util::GetMainModuleInfo().m_Text.m_Start;
    for (const auto &check : checks) if (std::memcmp(reinterpret_cast<const void *>(Main + check.offset), check.words, sizeof(check.words))) {
        Logging.Error("[QoL Enhancements/Crafting] quantity shortcut: DISABLED fingerprint at %lx\n", check.offset);
        return false;
    }

    qol::core::HookPlan hookPlan{
        qol::core::Site<EnterQuantityShortcutPreview>("EnterQuantityShortcutPreview", address::EnterPreview, {0xd10143ff, 0xa9027bfd, 0xf9001bf5, 0xa9044ff4, 0x910083fd}, true),
        qol::core::Site<LeaveQuantityShortcutPreview>("LeaveQuantityShortcutPreview", address::LeavePreview, {0xd10103ff, 0xa9027bfd, 0xa9034ff4, 0x910083fd, 0xf9400008}, true),
        qol::core::Site<BuildQuantityShortcutHint>("BuildQuantityShortcutHint", address::BuildFooter, {0xd102c3ff, 0xa9087bfd, 0xa90957f6, 0xa90a4ff4, 0x910203fd}, true),
        qol::core::Site<BuildCookingQuantityShortcutHint>("BuildCookingQuantityShortcutHint", address::KitchenBuildFooter, {0xd102c3ff, 0xa9087bfd, 0xa90957f6, 0xa90a4ff4, 0x910203fd}, true),
        qol::core::Site<ResolveQuantityShortcutCaption>("ResolveQuantityShortcutCaption", address::ResolveCaption, {0xf90213e8, 0x9000e6e8, 0xf945f908, 0x91004108, 0xf9004be8}, true),
    };
    if (!hookPlan) return false;
    if (!core::RegisterPageInput<QuantityShortcutPageInput>(core::InputPriority::QuantityShortcut)) return false;
    const auto vtable = *reinterpret_cast<const std::uintptr_t *>(Main + address::SafeStringVtableGot) + 16;
    QuantityBank = {vtable, "DIYDialogConfirmNum"}; QuantityCaption = {vtable, "0001"};
    if (!hookPlan.Publish<EnterQuantityShortcutPreview>(address::EnterPreview)) return false;
    if (!hookPlan.Publish<LeaveQuantityShortcutPreview>(address::LeavePreview)) return false;
    if (!hookPlan.Publish<BuildQuantityShortcutHint>(address::BuildFooter)) return false;
    if (!hookPlan.Publish<BuildCookingQuantityShortcutHint>(address::KitchenBuildFooter)) return false;
    if (!hookPlan.Publish<ResolveQuantityShortcutCaption>(address::ResolveCaption)) return false;
    Logging.Debug("[QoL Enhancements/Crafting] quantity shortcut: READY; workbench/cooking preview Y / Set quantity; X / Favorite\n");
    return true;
}
} // namespace qol::crafting
