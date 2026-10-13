#include "program/catalog/features/catalog_features.hpp"
#include "program/core/hook_plan.hpp"
#include "program/catalog/features/catalog_stars.hpp"
#include "program/catalog/features/catalog_star_text.hpp"
#include "program/catalog/game/recipe_knowledge.hpp"
#include "program/catalog/game/catalog_ownership.hpp"
#include "program/catalog/game/bindings.hpp"
#include <array>
#include <cstring>

namespace catalog_aware_shops::features {
namespace {
// The name-building hook serves several UIs. Saved return addresses identify
// the verified callers; controller fields then confirm the owning balloon.
namespace address {
constexpr std::uintptr_t CasePairs = 0x040a0fda, GetLocale = 0x037cefc4;
constexpr std::uintptr_t FittingFocusReturn = 0x03114c14, ShopFocusReturn = 0x0310be34;
constexpr std::uintptr_t PocketFocusReturn = 0x030883d4, PocketControllerVtable = 0x04a06e28;
constexpr std::uintptr_t StorageFocusReturn = 0x0316a5a0, StorageControllerVtable = 0x04a16028;
constexpr std::uintptr_t GroundNameReadyHook = 0x02ddc74c;
constexpr std::uintptr_t NameReadyHook = 0x030117c8;
} // namespace address
namespace layout {
constexpr std::size_t SavedReturn = 8, SavedController = 0x58, NameBelowFrame = 0x440;
constexpr std::size_t FittingPage = 0x220, ShopBalloon = 0x360, FittingBalloon = 0x218;
constexpr std::size_t PocketBalloon = 0x258, StorageBalloon = 0xaa9d8, StorageMode = 0x220;
// Ground ItemBalloon builds its name at SP+70 in a frame with FP = SP+b60.
constexpr std::size_t GroundNameBelowFrame = 0xaf0, GroundPlayerBelowFrame = 0xb11;
constexpr std::size_t ControllerPlayer = 0x1f8;
constexpr std::size_t NameType = 8, NameText = 0x18, NameCapacity = 0x20;
constexpr std::size_t NameStorageStart = 0x28, NameStorageEnd = 0x1f0;
} // namespace layout
constexpr unsigned CasePairCount = 188, CasePairBytes = 4, DutchLocale = 11;
constexpr unsigned NameCapacityUnits = 128;
template <class T> T Read(std::uintptr_t object, std::size_t offset = 0) {
    T value;
    std::memcpy(&value, reinterpret_cast<const void *>(object + offset), sizeof(value));
    return value;
}
// Matches native formatter 01ab5740: its sorted 188-pair UTF-16 case table,
// including accented/Cyrillic letters, and locale 11's initial Dutch IJ rule.
// The helper is called only after the complete name and capacity are validated.
void CapitalizeNameInitial(char16_t *text) {
    const auto initialCharacter = text[0];
    unsigned low = 0, high = CasePairCount;
    while (low < high) {
        const auto middle = low + (high - low) / 2;
        const auto casePair = game::MainAddress(address::CasePairs) + CasePairBytes * middle;
        const auto lowercaseCharacter = Read<std::uint16_t>(casePair);
        if (lowercaseCharacter < initialCharacter) {
            low = middle + 1;
        } else if (lowercaseCharacter > initialCharacter) {
            high = middle;
        } else {
            text[0] = Read<std::uint16_t>(casePair, 2);
            break;
        }
    }
    if (text[0] == u'I' && text[1] == u'j' &&
        game::FunctionAt<unsigned (*)()>(address::GetLocale)() == DutchLocale) {
        text[1] = u'J';
    }
}
// Each native name has a singular buffer and, for count-sensitive names,
// a plural buffer. Fences use this second form even when naming their recipe.
// 01b0c050 sets type 1/2 for these names; type 0 has only the singular form.
bool MarkNameForm(std::uintptr_t nameObject, std::size_t formOffset, char16_t marker) {
    const auto form = nameObject + formOffset;
    const auto textAddress = Read<std::uintptr_t>(form, layout::NameText);
    const auto capacity = Read<std::uint32_t>(form, layout::NameCapacity);
    if (capacity == NameCapacityUnits && textAddress >= form + layout::NameStorageStart &&
        textAddress + NameCapacityUnits * sizeof(char16_t) <= form + layout::NameStorageEnd) {
        return PrefixCatalogStar(reinterpret_cast<char16_t *>(textAddress), capacity,
                                 CapitalizeNameInitial, marker);
    }
    return false;
}
bool MarkNameObject(std::uintptr_t nameObject, char16_t marker) {
    constexpr unsigned PlainName = 0, CountedSingular = 1, CountedPlural = 2;
    // Native virtual getters +40/+50 expose forms at object+10 and +1f0.
    // Their corresponding fields therefore differ by 1e0 bytes.
    constexpr std::size_t PluralFormOffset = 0x1e0;
    const auto type = Read<std::uint32_t>(nameObject, layout::NameType);
    if (type > CountedPlural) {
        return false;
    }
    const bool singularMarked = MarkNameForm(nameObject, 0, marker);
    if (type == PlainName) {
        return singularMarked;
    }
    const bool pluralMarked = MarkNameForm(nameObject, PluralFormOffset, marker);
    return type == CountedSingular ? singularMarked : pluralMarked;
}
bool MarkBalloonName(std::uintptr_t nativeFrame, char16_t marker) {
    return MarkNameObject(nativeFrame - layout::NameBelowFrame, marker);
}
void MarkInventoryCollectible(exl::hook::InlineCtx *registers, std::uintptr_t nativeFrame,
                              bool isStorage) {
    const auto controller = Read<std::uintptr_t>(nativeFrame, layout::SavedController);
    if (!controller ||
        Read<std::uintptr_t>(controller) !=
            game::MainAddress(isStorage ? address::StorageControllerVtable
                                        : address::PocketControllerVtable) ||
        Read<std::uintptr_t>(controller, isStorage ? layout::StorageBalloon
                                                   : layout::PocketBalloon) != registers->X[19] ||
        !registers->X[19] || !registers->X[22]) {
        return;
    }
    // Home-storage browsing is mode 0; decorating and other selectors stay native.
    if (isStorage && Read<std::uint32_t>(controller, layout::StorageMode) != 0) {
        return;
    }
    const auto item = Read<game::CatalogItem>(registers->X[22]);
    const auto player = Read<std::uint8_t>(controller, layout::ControllerPlayer);
    if (player >= 8) {
        return;
    }
    const auto decision = DecideItemMarker(player, item, ItemUse::InventoryCollectible);
    const bool recipe = decision.isRecipe;
    const auto status = decision.recipe;
    const auto donation = decision.catalog;
    if (!recipe && donation == game::CatalogStatus::Unavailable) return;
    const bool marked = decision.marker && MarkBalloonName(nativeFrame, decision.marker);
    if (!recipe) {
        static unsigned creatureReports = 0;
        if (creatureReports++ < 64) {
            Logging.Debug("[QoL Enhancements/Catalog] %s_CREATURE_STAR item=%u player=%u status=%u marked=%d\n",
                          isStorage ? "STORAGE" : "POCKET", item.id, player,
                          static_cast<unsigned>(donation), marked);
        }
        return;
    }
    static unsigned reportCounts[2]{};
    auto &reports = reportCounts[isStorage ? 1 : 0];
    if (reports < 64) {
        ++reports;
        Logging.Debug(
            "[QoL Enhancements/Catalog] %s_RECIPE_STAR recipe=%u player=%u status=%u marked=%d type=%u\n",
            isStorage ? "STORAGE" : "POCKET",
            static_cast<unsigned>(static_cast<std::uint16_t>(item.extra)), player,
            static_cast<unsigned>(status), marked,
            Read<std::uint32_t>(nativeFrame - layout::NameBelowFrame, layout::NameType));
    }
}
// Hook register contract: x29 = native frame, x22 = selected 8-byte item,
// x19 = NameWindowCtrl. All pointers are borrowed for this callback only.
HOOK_DEFINE_INLINE(CabinetCatalogStar) {
    static void Callback(exl::hook::InlineCtx *registers) {
        const auto nativeFrame = registers->X[29];
        // Saved LR distinguishes pockets, cabinet and fitting-room focus updates.
        // 03011440 saves the caller's x19 (shop controller) at FP+58.
        if (!nativeFrame) {
            return;
        }
        const auto returnAddress = Read<std::uintptr_t>(nativeFrame, layout::SavedReturn);
        if (returnAddress == game::MainAddress(address::PocketFocusReturn) ||
            returnAddress == game::MainAddress(address::StorageFocusReturn)) {
            MarkInventoryCollectible(registers, nativeFrame,
                                     returnAddress == game::MainAddress(address::StorageFocusReturn));
            return;
        }
        const bool isFittingRoom = returnAddress == game::MainAddress(address::FittingFocusReturn);
        if (!isFittingRoom && returnAddress != game::MainAddress(address::ShopFocusReturn)) {
            return;
        }
        const auto shopController = Read<std::uintptr_t>(nativeFrame, layout::SavedController);
        if (!shopController || !registers->X[22]) {
            return;
        }
        auto nameWindow = Read<std::uintptr_t>(shopController, isFittingRoom ? layout::FittingPage
                                                                             : layout::ShopBalloon);
        if (isFittingRoom && nameWindow) {
            nameWindow = Read<std::uintptr_t>(nameWindow, layout::FittingBalloon);
        }
        if (!nameWindow || nameWindow != registers->X[19]) {
            return;
        }
        const auto item = Read<game::CatalogItem>(registers->X[22]);
        const auto player = Read<std::uint8_t>(shopController, layout::ControllerPlayer);
        const auto status = isFittingRoom ? QueryCatalog(player, item, Surface::FittingRoom)
                                          : QueryCatalog(player, item, Surface::Cabinet);
        static unsigned fittingReports = 0;
        if (isFittingRoom && status != game::CatalogStatus::Uncataloged && fittingReports < 64) {
            ++fittingReports;
            Logging.Debug(
                "[QoL Enhancements/Catalog] FITTING_CATALOG_STAR item=%u player=%u status=%u marked=0\n",
                item.id, player, static_cast<unsigned>(status));
        }
        const auto marker = CatalogMarker(status);
        static unsigned shopReports = 0;
        if (!isFittingRoom && !marker && shopReports < 64) {
            ++shopReports;
            Logging.Debug("[QoL Enhancements/Catalog] CATALOG_STAR item=%u status=%u marker=0 marked=0\n",
                        item.id, static_cast<unsigned>(status));
        }
        if (!marker) {
            return;
        }
        const bool marked = MarkBalloonName(nativeFrame, marker);
        const auto nameObject = nativeFrame - layout::NameBelowFrame;
        const auto capacity = Read<std::uint32_t>(nameObject, layout::NameCapacity);
        if (isFittingRoom) {
            if (fittingReports < 64) {
                ++fittingReports;
                Logging.Debug("[QoL Enhancements/Catalog] FITTING_CATALOG_STAR item=%u player=%u status=%u "
                            "marked=%d type=%u capacity=%u\n",
                            item.id, player, static_cast<unsigned>(status), marked,
                            Read<std::uint32_t>(nameObject, layout::NameType), capacity);
            }
        } else if (shopReports < 64) {
            ++shopReports;
            Logging.Debug(
                "[QoL Enhancements/Catalog] CATALOG_STAR item=%u status=%u marker=0x%x marked=%d type=%u "
                "capacity=%u\n",
                item.id, static_cast<unsigned>(status), static_cast<unsigned>(marker), marked,
                Read<std::uint32_t>(nameObject, layout::NameType), capacity);
        }
    }
};
// Dedicated world ItemBalloon path (02ddbacc), called by the field HUD at
// 02e06cc4. x21 points to the complete item copied into this player's balloon
// slot, and FP-b11 is the validated player byte originally passed as argument 3.
// Do not use the controller's player: this UI owns up to eight player balloons.
HOOK_DEFINE_INLINE(GroundCatalogStar) {
    static void Callback(exl::hook::InlineCtx *registers) {
        const auto nativeFrame = registers->X[29];
        if (!nativeFrame || !registers->X[21]) {
            return;
        }
        const auto player = Read<std::uint8_t>(nativeFrame - layout::GroundPlayerBelowFrame);
        if (player >= 8) {
            return;
        }
        const auto item = Read<game::CatalogItem>(registers->X[21]);
        const auto decision = DecideItemMarker(player, item, ItemUse::ItemName);
        const auto marker = decision.marker;
        const bool isRecipe = decision.isRecipe;
        const auto status = isRecipe ? static_cast<unsigned>(decision.recipe)
                                     : static_cast<unsigned>(decision.catalog);
        const bool marked =
            marker && MarkNameObject(nativeFrame - layout::GroundNameBelowFrame, marker);
        static unsigned reports = 0;
        if (reports < 64) {
            ++reports;
            Logging.Debug("[QoL Enhancements/Catalog] GROUND_STAR item=%u player=%u recipe=%d "
                        "status=%u marker=0x%x marked=%d\n",
                        item.id, player, isRecipe, status, static_cast<unsigned>(marker), marked);
        }
    }
};
} // namespace
// Instruction fingerprints are exact 3.0.3 ARM words, intentionally left as
// hex. They guard hook locations and native assumptions; do not edit them to
// silence a mismatch without re-checking the game's disassembly.
bool InstallCatalogStars() {
    struct Check {
        std::uintptr_t offset;
        std::array<std::uint32_t, 5> words;
    };
    constexpr Check checks[]{
        {0x01b0c320, {0xf9400268, 0xaa1303e0, 0xf9402908, 0xd63f0100, 0xf94002a8}},
        {0x01b0c358, {0xaa1403e0, 0x97fb8fdd, 0xf9400268, 0x7100041f, 0xaa1303e0}},
        {0x01b0c668, {0xb9400800, 0xd65f03c0, 0xb9000801, 0xd65f03c0, 0x91004000}},
        {0x01b0c680, {0x91004000, 0xd65f03c0, 0x9107c000, 0xd65f03c0, 0x9107c000}},
        {0x0316a56c, {0xf9408720, 0x91004108, 0xb90017fa, 0xa907a7e8, 0xf940eac8}},
        {0x0316a584, {0x785e43a8, 0x790013e8, 0x785e63a8, 0x790017e8, 0xb85e83a8}},
        {0x0316a598, {0xb9000fe8, 0x97fa9ba9, 0xb9422268, 0x7100051f, 0x54000301}},
        {0x02ddbacc, {0xfc190fe8, 0xa9017bfd, 0x910043fd, 0xa9026ffc, 0xa90367fa}},
        {0x02ddbb28, {0xf000e6a8, 0xaa0503f4, 0xf9440508, 0x2a0403f6, 0x2a0303f3}},
        {0x02ddbc24, {0xaa1f03fa, 0x91013ff7, 0x14000069, 0x3949000c, 0x52801feb}},
        {0x02ddbdd0, {0x390002e8, 0xf9400328, 0xb4000068, 0x91080118, 0x14000007}},
        {0x02ddc02c, {0x9baa6355, 0x394002ea, 0x7100215f, 0x385e23ac, 0x1a94314a}},
        {0x02ddc56c, {0x9101c3fa, 0xf9467108, 0x5280100a, 0xb90093ea, 0x9101534a}},
        {0x02ddc738, {0xd000e6a2, 0x9101c3e0, 0xf946f842, 0xaa1503e1, 0x97b4bced}},
        {0x02ddc74c, {0xd000e6a8, 0x90009029, 0x912e9129, 0xf9401108, 0x911683e0}},
        {0x02e06ca4, {0xf9410f00, 0x910023e2, 0x910003e5, 0xaa1303e1, 0x7100211f}},
        {0x030883b8, {0x790043f7, 0x39008bf8, 0x39008ff5, 0xb90027f9, 0xf9412e60}},
        {0x030883cc, {0x910063e1, 0x97fe241c, 0xa9544ff4, 0xa95357f6, 0xa9525ff8}},

        {0x01ab5764, {0x3952ec09, 0x340004c9, 0x2a1f03e9, 0x5280176b, 0xf0012f4a}},
        {0x01ab57d8, {0x7101251f, 0x54000581, 0xf9400268, 0xaa1303e0, 0xf9402d08}},
        {0x01ab5800, {0x7101a91f, 0x54000501, 0x3952f26a, 0x340004ca, 0x2a1f03e8}},
        {0x037cefc4, {0xa9be7bfd, 0xf9000bf3, 0x910003fd, 0xb00110c8, 0xf9436108}},
        {0x030117c8, {0xf000d568, 0xd10183ab, 0xf9428908, 0x29412aa9, 0x91002160}},
        {0x03011440, {0xa9ba7bfd, 0xa9016ffc, 0x910003fd, 0xa90267fa, 0xa9035ff8}},
        {0x03011454, {0xa90457f6, 0xa9054ff4, 0xd11143ff, 0xf9410816, 0xaa0103f5}},
        {0x03114b20, {0xf9411268, 0xf9410d15, 0xb4000775, 0xf000cce1, 0x910003f9}},
        {0x03114be4, {0xf8687928, 0x79400109, 0x790013e9, 0x39400909, 0x39002be9}},
        {0x03114c00, {0xb9400508, 0xb9000fe8, 0x910003e1, 0xaa1503e0, 0x97fbf20c}},
        {0x03113fa4, {0xb40000f4, 0x39403a89, 0x7100213f, 0x1a883128, 0x3907e268}},
        {0x0310be20, {0x294026e8, 0x910003e1, 0xf941b260, 0x290127e8, 0x97fc1584}}};
    std::uint32_t caseHash = 2166136261u;
    for (unsigned i = 0; i < CasePairCount * CasePairBytes; ++i) {
        caseHash =
            (caseHash ^ Read<std::uint8_t>(game::MainAddress(address::CasePairs), i)) * 16777619u;
    }
    bool valid = caseHash == 0xdb9674c3u && game::ValidateCatalogBindings() &&
                 game::ValidateRecipeBindings();
    for (const auto &c : checks) {
        valid = valid && !std::memcmp(reinterpret_cast<const void *>(game::MainAddress(c.offset)),
                                      c.words.data(), sizeof(c.words));
    }
    if (!valid) {
        Logging.Error("[QoL Enhancements/Catalog] CATALOG_STARS_DISABLED fingerprint mismatch\n");
        return false;
    }

    qol::core::HookPlan hookPlan{
        qol::core::Site<CabinetCatalogStar>("CabinetCatalogStar", address::NameReadyHook, {0xf000d568, 0xd10183ab, 0xf9428908, 0x29412aa9, 0x91002160}, true),
        qol::core::Site<GroundCatalogStar>("GroundCatalogStar", address::GroundNameReadyHook, {0xd000e6a8, 0x90009029, 0x912e9129, 0xf9401108, 0x911683e0}, true),
    };
    if (!hookPlan) return false;
    if (!hookPlan.Publish<CabinetCatalogStar>(address::NameReadyHook)) return false;
    if (!hookPlan.Publish<GroundCatalogStar>(address::GroundNameReadyHook)) return false;
    Logging.Debug("[QoL Enhancements/Catalog] CATALOG_STARS_READY shared shop list: all ordinary items\n");
    Logging.Debug("[QoL Enhancements/Catalog] FITTING_CATALOG_STARS_READY highlighted clothing balloon\n");
    Logging.Debug("[QoL Enhancements/Catalog] POCKET_RECIPE_STARS_READY learned-recipe balloons\n");
    Logging.Debug("[QoL Enhancements/Catalog] STORAGE_RECIPE_STARS_READY learned-recipe balloons\n");
    Logging.Debug("[QoL Enhancements/Catalog] MUSEUM_CREATURE_STARS_READY undonated fish, bugs and sea creatures\n");
    Logging.Debug("[QoL Enhancements/Catalog] GROUND_STARS_READY recipes and ordinary items\n");
    return true;
}
} // namespace catalog_aware_shops::features
