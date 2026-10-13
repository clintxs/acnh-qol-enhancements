#include "program/core/hook_plan.hpp"
#include "catalog_stars.hpp"
#include "catalog_star_text.hpp"
#include "program/catalog/game/catalog_ownership.hpp"
#include "program/catalog/game/bindings.hpp"
#include <array>
#include <cstring>

namespace catalog_aware_shops::features {
namespace {
// Module addresses identify native routines; layout offsets identify fields
// inside borrowed game objects. These values are verified for ACNH 3.0.3 only.
namespace address {
constexpr std::uintptr_t OpenMessageActionVtable = 0x04833a58;
constexpr std::uintptr_t UiResultGlobalSlot = 0x04ab2230;
constexpr std::uintptr_t NameConstructorContext = 0x04ab2df0;
constexpr std::uintptr_t ConstructItemName = 0x01b0b8d4;
constexpr std::uintptr_t RegisterItemName = 0x01aaf070;
constexpr std::uintptr_t SplitMessageId = 0x009d8430;
constexpr std::uintptr_t DispatchMessage = 0x009d8794;
} // namespace address
namespace layout {
constexpr std::size_t SafeStringData = 8, ActionEvent = 8;
constexpr std::size_t EventPlayer = 0x2a8, EventDialogueHolder = 0x290;
constexpr std::size_t DirectDialogue = 0x10, DialogueReference = 0x20, DialogueFormatter = 8;
constexpr std::size_t UiResult = 0x28, SelectedShopItem = 0x2c;
constexpr std::size_t NativeNameBytes = 0x3d0, NameType = 8;
constexpr std::size_t NameText = 0x18, NameCapacity = 0x20, InlineNameText = 0xf0;
constexpr unsigned NameCapacityUnits = 128;
} // namespace layout
template <class T> T Read(std::uintptr_t object, std::size_t offset = 0) {
    T value;
    std::memcpy(&value, reinterpret_cast<const void *>(object + offset), sizeof(value));
    return value;
}
bool SafeStringEquals(std::uintptr_t safeString, const char *expected) {
    if (!safeString) {
        return false;
    }
    const auto data =
        Read<std::uintptr_t>(safeString, layout::SafeStringData); // native SafeString<char>
    if (!data) {
        return false;
    }
    for (unsigned i = 0; i < 64; ++i) {
        const auto character = Read<char>(data, i);
        if (character != expected[i]) {
            return false;
        }
        if (!character) {
            return true;
        }
    }
    return false;
}
#if CAS_VERBOSE_DIALOGUE_TRACE
template <std::size_t N> void CopyMessageId(std::uintptr_t safeString, char (&out)[N]) {
    const auto data = safeString ? Read<std::uintptr_t>(safeString, layout::SafeStringData) : 0;
    out[0] = 0;
    if (!data) {
        return;
    }
    for (unsigned i = 0; i < N - 1; ++i) {
        const auto character = Read<char>(data, i);
        out[i] = character;
        if (!character) {
            return;
        }
    }
    out[N - 1] = 0;
}
#endif
// The event-flow bank is already resolved to a basename at this hook. Each
// supported flow sets cItemName slot 0 from ShopCheckItemSet before dispatch.
enum ShopIndex { UnsupportedShop, NookShop, HhpShop, HotelShop, AbleShop };
constexpr const char *ShopNames[]{"", "nook", "hhp", "hotel", "able"};
ShopIndex IdentifyShopMessage(std::uintptr_t bank, std::uintptr_t label) {
    if (SafeStringEquals(bank, "SP_rcm_00_common") && SafeStringEquals(label, "101")) {
        return NookShop;
    }
    if (SafeStringEquals(bank, "SYS_Wherearen_OfficeShop") && SafeStringEquals(label, "001")) {
        return HhpShop;
    }
    if (SafeStringEquals(bank, "SP_kpg") && SafeStringEquals(label, "151")) {
        return HotelShop;
    }
    if (SafeStringEquals(bank, "SP_hgh") &&
        (SafeStringEquals(label, "051") || SafeStringEquals(label, "052"))) {
        return AbleShop;
    }
    return UnsupportedShop;
}

std::uintptr_t FindDialogueFormatter(std::uintptr_t event) {
    const auto holder = Read<std::uintptr_t>(event, layout::EventDialogueHolder);
    // Match native 009d886c: prefer the direct NPC dialogue. Only follow the
    // reference when direct is absent; it can be unpopulated in NPC actions.
    auto dialogue = holder ? Read<std::uintptr_t>(holder, layout::DirectDialogue) : 0;
    if (!dialogue && holder) {
        const auto reference = Read<std::uintptr_t>(holder, layout::DialogueReference);
        if (reference) {
            dialogue = Read<std::uintptr_t>(reference);
        }
    }
    return dialogue ? Read<std::uintptr_t>(dialogue, layout::DialogueFormatter) : 0;
}
std::uintptr_t FindShopSelectionResult() {
    const auto pointerSlot = Read<std::uintptr_t>(game::MainAddress(address::UiResultGlobalSlot));
    const auto resultRoot = pointerSlot ? Read<std::uintptr_t>(pointerSlot) : 0;
    return resultRoot ? Read<std::uintptr_t>(resultRoot, layout::UiResult) : 0;
}
bool RegisterMarkedItemName(std::uintptr_t formatter, const game::CatalogItem &item,
                            char16_t marker) {
    alignas(8) std::array<std::byte, layout::NativeNameBytes> nameStorage{};
    game::FunctionAt<void (*)(void *, const game::CatalogItem *, std::uintptr_t)>(
        address::ConstructItemName)(
        nameStorage.data(), &item,
        Read<std::uintptr_t>(game::MainAddress(address::NameConstructorContext)));
    const auto nameObject = reinterpret_cast<std::uintptr_t>(nameStorage.data());
    const auto text = Read<std::uintptr_t>(nameObject, layout::NameText);
    // Only the verified plain-name buffer: ruby/alternate representations stay
    // native. Do not capitalize here; dialogue names occur after an article.
    if (Read<std::uint32_t>(nameObject, layout::NameType) ||
        text != nameObject + layout::InlineNameText ||
        Read<std::uint32_t>(nameObject, layout::NameCapacity) != layout::NameCapacityUnits) {
        return false;
    }
    if (!PrefixCatalogStar(
            reinterpret_cast<char16_t *>(text), layout::NameCapacityUnits, [](char16_t *) {},
            marker)) {
        return false;
    }
    // SetTagFromUIResult's API COPIES the temporary into dialogue-owned storage
    // (01ac0258). Returning is safe even when the message is displayed later.
    // Keeping native noun metadata preserves article selection and text color.
    game::FunctionAt<void (*)(std::uintptr_t, void *, unsigned)>(address::RegisterItemName)(
        formatter, nameStorage.data(), 0); // cItemName substitution slot 0.
    return true;
}

#if CAS_VERBOSE_DIALOGUE_TRACE
// Diagnostic only: establishes whether the OpenMessageWindow path is reached
// even when it never calls the dispatch hook below. No game state is modified.
HOOK_DEFINE_INLINE(NookMessageTrace) {
    static void Callback(exl::hook::InlineCtx *registers) {
        static unsigned reports = 0, nookReports = 0;
        char message[128];
        CopyMessageId(registers->X[2], message);
        const bool nook = std::strstr(message, "SP_rcm_00_common:") != nullptr;
        if (nook ? nookReports >= 64 : reports >= 48) {
            return;
        }
        if (nook) {
            ++nookReports;
        } else {
            ++reports;
        }
        Logging.Debug("[QoL Enhancements/Catalog] NOOK_MESSAGE_TRACE message=%s caller=0x%lx\n", message,
                    registers->X[30] - game::MainAddress(0));
    }
};
#endif
// Register contract at 009d8794: x0 = action, x1 = bank SafeString,
// x2 = label SafeString. The callback never changes these saved registers.
// Historical hook/log names retain "Nook" because all shops share this path.
HOOK_DEFINE_INLINE(NookDisplayCatalogStar) {
    static void Callback(exl::hook::InlineCtx *registers) {
        // OpenMessageWindow has already split and resolved MessageID. This
        // dispatch point runs once, whether the action opened immediately or
        // waited for the NPC. It precedes the asynchronous dialogue display.
        const auto action = registers->X[0];
        const auto vtable = action ? Read<std::uintptr_t>(action) : 0;
#if CAS_VERBOSE_DIALOGUE_TRACE
        // Native dispatch strips directories: the full path is only present
        // before message resolution. Confirmed by the 0.5.12 Eden trace.
        const bool nook = SafeStringEquals(registers->X[1], "SP_rcm_00_common");
        static unsigned dispatchReports = 0, nookReports = 0;
        if (nook ? nookReports < 64 : dispatchReports < 48) {
            if (nook) {
                ++nookReports;
            } else {
                ++dispatchReports;
            }
            char bank[64], label[128];
            CopyMessageId(registers->X[1], bank);
            CopyMessageId(registers->X[2], label);
            Logging.Debug("[QoL Enhancements/Catalog] NOOK_MESSAGE_DISPATCH bank=%s label=%s vtable=0x%lx\n",
                        bank, label, vtable ? vtable - game::MainAddress(0) : 0);
        }
#endif
        const auto shop = IdentifyShopMessage(registers->X[1], registers->X[2]);
        if (shop == UnsupportedShop) {
            return;
        }
        // Report each early exit for scoped messages, independently of generic
        // dialogue trace limits. These were silent in 0.5.11.
        static unsigned rejectReports[5]{};
        const auto reject = [&](const char *reason) {
            if (rejectReports[shop] < 48) {
                ++rejectReports[shop];
                Logging.Debug("[QoL Enhancements/Catalog] NOOK_CATALOG_SKIP reason=%s shop=%s\n", reason,
                            ShopNames[shop]);
            }
        };
        if (!action || vtable != game::MainAddress(address::OpenMessageActionVtable)) {
            reject("action-type");
            return;
        }
        const auto event = Read<std::uintptr_t>(action, layout::ActionEvent);
        if (!event) {
            reject("event");
            return;
        }
        const auto player = Read<std::uint8_t>(event, layout::EventPlayer);
        const auto formatter = FindDialogueFormatter(event);
        const auto result = FindShopSelectionResult();
        if (!formatter || !result) {
            reject(!formatter ? "formatter" : "ui-result");
            return;
        }
        // ShopCheckItemSet writes the full eight-byte selection at the common
        // exit for cMamekichi, cWherearenOffice, cHotelEntrance and cTailor.
        const auto item = Read<game::CatalogItem>(result, layout::SelectedShopItem);
        const auto status = shop == NookShop ? game::QueryFurnitureCatalog(player, item)
                                             : game::QueryCabinetCatalog(player, item);
        const auto marker = game::CatalogMarker(status);
        const bool marked = marker && RegisterMarkedItemName(formatter, item, marker);
        static unsigned reports[5]{};
        if (reports[shop]++ < 48) {
            Logging.Debug(
                "[QoL Enhancements/Catalog] NOOK_CATALOG_STAR item=%u player=%u status=%u marked=%d "
                "shop=%s marker=0x%x\n",
                item.id, player, static_cast<unsigned>(status), marked, ShopNames[shop],
                static_cast<unsigned>(marker));
        }
    }
};
} // namespace
// Instruction fingerprints are exact 3.0.3 ARM words, intentionally left as
// hex. They guard hook locations and native assumptions; do not edit them to
// silence a mismatch without re-checking the game's disassembly.
bool InstallNookCatalogStars() {
    struct Check {
        std::uintptr_t offset;
        std::array<std::uint32_t, 5> words;
    };
    // Exact ARM words are verification data, not values used by the feature.
    constexpr Check checks[]{
#if CAS_VERBOSE_DIALOGUE_TRACE
        {0x009d8430, {0xd10603ff, 0xa9127bfd, 0xa9136ffc, 0xa91467fa, 0xa9155ff8}},
#endif
        {0x009d8794, {0xd105c3ff, 0xa9137bfd, 0xa9145ffc, 0xa91557f6, 0xa9164ff4}},
        {0x009d886c, {0xf9400668, 0xf9414908, 0xf9400915, 0xb50000d5, 0xf9401108}},
        {0x00c5e134, {0x943ab5e8, 0xf9400688, 0x910563e1, 0x2a1303e2, 0xf9414908}},
        {0x01b0b8d4, {0xa9ba7bfd, 0xa9016ffc, 0xa90267fa, 0xa9035ff8, 0xa90457f6}},
        {0x01b0bad4, {0xaa1503e0, 0xaa1403e1, 0xaa1303e2, 0xa9454ff4, 0xa94457f6}},
        {0x01aaf070, {0xd100c3ff, 0xa9027bfd, 0x910083fd, 0xf001bc48, 0x910f0108}},
        {0x01ac0258, {0xa9bb7bfd, 0xa90167fc, 0x910003fd, 0xa9025ff8, 0xa90357f6}}};
    bool valid = game::ValidateCatalogBindings();
    for (const auto &character : checks) {
        valid = valid &&
                !std::memcmp(reinterpret_cast<const void *>(game::MainAddress(character.offset)),
                             character.words.data(), sizeof(character.words));
    }
    if (!valid) {
        Logging.Error("[QoL Enhancements/Catalog] NOOK_CATALOG_STARS_DISABLED fingerprint mismatch\n");
        return false;
    }
    qol::core::HookPlan hookPlan{
#if CAS_VERBOSE_DIALOGUE_TRACE
        qol::core::Site<NookMessageTrace>("NookMessageTrace", address::SplitMessageId, {0xd10603ff, 0xa9127bfd, 0xa9136ffc, 0xa91467fa, 0xa9155ff8}, true),
#endif
        qol::core::Site<NookDisplayCatalogStar>("NookDisplayCatalogStar", address::DispatchMessage, {0xd105c3ff, 0xa9137bfd, 0xa9145ffc, 0xa91557f6, 0xa9164ff4}, true),
    };
    if (!hookPlan) return false;
#if CAS_VERBOSE_DIALOGUE_TRACE
    if (!hookPlan.Publish<NookMessageTrace>(address::SplitMessageId)) return false;
#endif
    if (!hookPlan.Publish<NookDisplayCatalogStar>(address::DispatchMessage)) return false;
    Logging.Debug(
        "[QoL Enhancements/Catalog] NOOK_CATALOG_STARS_READY SP_rcm_00_common:101 direct-context\n");
    Logging.Debug("[QoL Enhancements/Catalog] SHOP_CATALOG_STARS_READY hhp:001 hotel:151 able:051/052\n");
    return true;
}
} // namespace catalog_aware_shops::features
