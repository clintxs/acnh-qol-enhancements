#include "storage_hint.hpp"
#include "menu_text.hpp"
#include "program/core/hint_text.hpp"
#include "program/pockets/game/stack_bindings.hpp"
#include <array>
#include <cstring>

namespace qol_pockets::features {
namespace {
namespace address {
constexpr std::uintptr_t BuildPocketFooter = 0x03094fb4;
constexpr std::uintptr_t AppendHint = 0x02de2b00;
constexpr std::uintptr_t ResolveFooterCaptionCall = 0x02de4954;
constexpr std::uintptr_t AppendStorageSelectCall = 0x0316f490;
constexpr std::uintptr_t StorageVtable = 0x04a16028;
constexpr std::uintptr_t OwnHome = 0x02815598;
constexpr std::uintptr_t PocketVtable = 0x04a06e28;
constexpr std::uintptr_t ApplicationGlobalGot = 0x04ab2230;
constexpr std::uintptr_t FooterTypeGot = 0x04ac7278;
constexpr std::uintptr_t FooterTypeGuardGot = 0x04ac7280;
constexpr std::uintptr_t BufferedStringVtableGot = 0x04ab2bc0;
constexpr std::uintptr_t SafeStringVtableGot = 0x04ab2020;
} // namespace address
namespace layout {
constexpr std::size_t ApplicationFooter = 0x58;
constexpr std::size_t Inventory = 0x1f8;
constexpr std::size_t PocketMode = 0x1c4fc;
constexpr std::size_t CanClose = 0x1c51d, SelectSuppressed = 0x1c51e;
constexpr std::size_t HintCount = 0x218, HintCapacity = 0x21c;
constexpr std::size_t HintArray = 0x220, HintFreeList = 0x228;
} // namespace layout
constexpr int NativeY = 7;
// A private string token, not a new shared message ID or an input registration.
constexpr char StorageHintKey[] = "qol_pockets.put_in_storage";
constexpr char WithdrawalHintKey[] = "qol_pockets.move_to_pockets";
constexpr char DropHintKey[] = "qol_pockets.drop_release";
constexpr char EquipHintKey[] = "qol_pockets.hold_wear";

const char16_t *PocketHintText(const char *key) {
    const bool drop = std::strcmp(key, DropHintKey) == 0;
    if (!drop && std::strcmp(key, EquipHintKey) != 0) return nullptr;
    const auto *caption = PocketMessage(key);
    return caption ? caption : drop ? u"Drop / Release" : u"Equip / Unequip";
}

template <typename T> T &Field(void *object, std::size_t offset) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(object) + offset);
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
SafeString StorageBank, StorageCaption, WithdrawalCaption;

HOOK_DEFINE_INLINE(RenameStorageSelectHint) {
    static void Callback(exl::hook::InlineCtx *registers) {
        // Storage footer 0316f388: x20 is the storage controller, x1 the
        // temporary A/Select descriptor. This call site is not shared with
        // donation menus. Mode 0 matches our direct-withdrawal feature.
        auto *storage = reinterpret_cast<void *>(registers->X[20]);
        auto *hint = reinterpret_cast<HintDescription *>(registers->X[1]);
        constexpr std::size_t StorageMode = 0x220;
        if (!storage || !hint ||
            Field<std::uintptr_t>(storage, 0) != game::MainAddress(address::StorageVtable) ||
            Field<int>(storage, StorageMode) != 0 || hint->button != 4 ||
            hint->caption.text != hint->key || hint->capacity != sizeof(hint->key) ||
            std::strcmp(hint->key, "1201") != 0) {
            return;
        }
        // Keep the original buffer pointer: native code reuses this same
        // descriptor for Close/Sort next. AppendHint copies our private token.
        std::memcpy(hint->key, WithdrawalHintKey, sizeof(WithdrawalHintKey));
    }
};

void *PocketFooter() {
    auto **applicationGlobal = *reinterpret_cast<void ***>(
        game::MainAddress(address::ApplicationGlobalGot));
    auto *application = applicationGlobal ? *applicationGlobal : nullptr;
    if (!application) {
        return nullptr;
    }
    auto *footer = Field<void *>(application, layout::ApplicationFooter);
    const auto *guard = *reinterpret_cast<const std::uint8_t **>(
        game::MainAddress(address::FooterTypeGuardGot));
    if (!footer || !guard || !(*guard & 1)) {
        return nullptr;
    }
    // The same native type check as BuildPocketFooter. Do not interpret a
    // different controller's fields if the application changes its footer.
    auto *type = *reinterpret_cast<void **>(game::MainAddress(address::FooterTypeGot));
    auto isType = *Field<unsigned (**)(void *, void *)>(footer, 0);
    return (isType(footer, type) & 1) ? footer : nullptr;
}

void AppendPocketHint(void *pocket, int button, const char *key) {
    auto *footer = PocketFooter();
    if (!footer) {
        return;
    }
    const int count = Field<int>(footer, layout::HintCount);
    const int capacity = Field<int>(footer, layout::HintCapacity);
    auto **entries = Field<HintDescription **>(footer, layout::HintArray);
    // Native KeyGuide has five visible panes and a bounded record pool.
    // Never grow the pool, overwrite a hint, or append twice in one batch.
    if (count < 0 || count >= 5 || count >= capacity || !entries ||
        !Field<void *>(footer, layout::HintFreeList)) {
        return;
    }
    for (int index = 0; index < count; ++index) {
        if (entries[index] && entries[index]->caption.text &&
            std::strcmp(entries[index]->caption.text, key) == 0) {
            return;
        }
    }
    HintDescription hint;
    hint.button = button;
    hint.player = Field<std::uint8_t>(pocket, layout::Inventory);
    if (hint.player > 7) {
        hint.player = 0xff;
    }
    hint.caption.vtable = *reinterpret_cast<const std::uintptr_t *>(
                             game::MainAddress(address::BufferedStringVtableGot)) + 16;
    hint.caption.text = hint.key;
    const auto length = std::strlen(key);
    if (length >= sizeof(hint.key)) return;
    std::memcpy(hint.key, key, length + 1);
    // The game copies the descriptor and sorts hints by its normal button
    // priority before rendering. It also handles sizing and opening/closing.
    game::FunctionAt<void (*)(void *, const HintDescription *, unsigned)>(address::AppendHint)(
        footer, &hint, 1);
}

HOOK_DEFINE_TRAMPOLINE(BuildPocketStorageHint) {
    static void Callback(void *pocket, int kind) {
        Orig(pocket, kind);
        // Kind 0 is the normal Select/Close footer (including opening).
        // Kinds 1/2 are Back-only/hidden; kind 3 serves a special pocket mode.
        // Do not require browsing state 3 here: the opening state builds it.
        if (kind != 0 || !pocket ||
            Field<std::uintptr_t>(pocket, 0) != game::MainAddress(address::PocketVtable) ||
            Field<int>(pocket, layout::PocketMode) != 0 ||
            !Field<std::uint8_t>(pocket, layout::CanClose) ||
            Field<std::uint8_t>(pocket, layout::SelectSuppressed)) {
            return;
        }
        auto *inventory = static_cast<std::byte *>(pocket) + layout::Inventory;
        if (game::FunctionAt<unsigned (*)(void *)>(address::OwnHome)(inventory) & 1) {
            AppendPocketHint(pocket, NativeY, StorageHintKey);
        }
        AppendPocketHint(pocket, 10, DropHintKey);
        AppendPocketHint(pocket, 11, EquipHintKey);
    }
};

HOOK_DEFINE_INLINE(ResolveStorageHintCaption) {
    static void Callback(exl::hook::InlineCtx *registers) {
        // Only the footer's label call: x2 = message-bank SafeString,
        // x3 = key SafeString. Icon and slash rendering use other call sites.
        const auto *key = reinterpret_cast<const SafeString *>(registers->X[3]);
        if (key && key->text && std::strcmp(key->text, StorageHintKey) == 0) {
            registers->X[2] = reinterpret_cast<std::uintptr_t>(&StorageBank);
            registers->X[3] = reinterpret_cast<std::uintptr_t>(&StorageCaption);
        } else if (key && key->text && std::strcmp(key->text, WithdrawalHintKey) == 0) {
            registers->X[2] = reinterpret_cast<std::uintptr_t>(&StorageBank);
            registers->X[3] = reinterpret_cast<std::uintptr_t>(&WithdrawalCaption);
        }
        // The displaced native call now loads CmnSubWin/0121 or /0202 in the active
        // language. No LayoutMsg replacement, numeric-ID allocation or I/O.
    }
};
} // namespace

bool InstallStorageHint() {
    struct Check {
        std::uintptr_t offset;
        std::array<std::uint32_t, 5> words;
    };
    constexpr std::array checks{
        Check{address::BuildPocketFooter,
              {0xd10343ff, 0xa9087bfd, 0xf9004bf9, 0xa90a5ff8, 0xa90b57f6}},
        Check{address::AppendHint,
              {0xb941f008, 0x3100051f, 0xb941f409, 0xb941b00a, 0x3a410920}},
        Check{address::OwnHome,
              {0xf0011988, 0x798fa908, 0xb100051f, 0x540007a0, 0xd100c3ff}},
        Check{address::ResolveFooterCaptionCall,
              {0x97b4162f, 0x910243e0, 0x97b4181b, 0x910243e0, 0x97b41555}},
    };
    for (const auto &check : checks) {
        if (std::memcmp(reinterpret_cast<const void *>(game::MainAddress(check.offset)),
                        check.words.data(), sizeof(check.words)) != 0) {
            Logging.Error("[QoL Enhancements/Pockets] STORAGE_HINT_DISABLED fingerprint at %lx\n",
                        check.offset);
            return false;
        }
    }
    const auto vtable = *reinterpret_cast<const std::uintptr_t *>(
                           game::MainAddress(address::SafeStringVtableGot)) + 16;
    StorageBank = {vtable, "CmnSubWin"};
    StorageCaption = {vtable, "0121"};
    WithdrawalCaption = {vtable, "0202"};
    if (!qol::core::RegisterHintText(PocketHintText)) return false;
    ResolveStorageHintCaption::InstallAtOffset(address::ResolveFooterCaptionCall);
    BuildPocketStorageHint::InstallAtOffset(address::BuildPocketFooter);
    Logging.Debug("[QoL Enhancements/Pockets] STORAGE_HINT_READY native Y / Put in Storage\n");
    return true;
}

bool InstallStorageWithdrawalHint() {
    constexpr std::array<std::uint32_t, 5> expected{
        0x97f1cd9c, 0x360ffe97, 0xf9400ff5, 0x528000a8, 0xb90003e8};
    if (std::memcmp(reinterpret_cast<const void *>(game::MainAddress(address::AppendStorageSelectCall)),
                    expected.data(), sizeof(expected)) != 0) {
        Logging.Error("[QoL Enhancements/Pockets] WITHDRAWAL_HINT_DISABLED fingerprint mismatch\n");
        return false;
    }
    RenameStorageSelectHint::InstallAtOffset(address::AppendStorageSelectCall);
    Logging.Debug("[QoL Enhancements/Pockets] WITHDRAWAL_HINT_READY native A / Move to Pockets\n");
    return true;
}
} // namespace qol_pockets::features
