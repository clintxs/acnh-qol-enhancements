#include "catalog_features.hpp"
#include "program/catalog/settings.hpp"
#include <array>
#include <cstring>
namespace catalog_aware_shops::features {
namespace {
constexpr unsigned ResidentCount = 8, CatalogItemLimit = 15000;
constexpr unsigned UnsupportedItemFlags = 3;
} // namespace
using game::CatalogStatus;
using game::RecipeStatus;
bool IsEligibleKind(const game::ItemMetadata &metadata, Surface surface) {
    const auto kind = game::ClassifyItemKind(metadata);
    if (kind == game::ItemKind::Currency || kind == game::ItemKind::Recipe ||
        (surface == Surface::Miles && kind == game::ItemKind::AbilityBook)) {
        return false;
    }
    if (!metadata.identityMatches) {
        return false;
    }
    if (surface == Surface::FittingRoom) {
        return kind == game::ItemKind::Clothing;
    }
    if (surface == Surface::Furniture) {
        return kind == game::ItemKind::Furniture;
    }
    return true;
}

// These redemption rewards are currency, travel access or construction unlocks,
// not collectible catalog entries. Apply consistently on every catalog surface.
bool IsExcludedReward(std::uint16_t itemId) {
    constexpr std::array<std::uint16_t, 14> excluded{
        5851, 9964,                               // Nook Miles Ticket, Bell voucher.
        2543, 3075, 3247,                         // Base cliff, path and waterscaping permit items.
        8773, 8774, 8775, 8776, 8777, 8778, 8779, // Built-in path permits.
        8780, 8781                                // Waterscaping and cliff permits sold for Miles.
    };
    constexpr std::uint16_t CustomDesignPathPermit = 9771;
    for (const auto excludedId : excluded) {
        if (itemId == excludedId) {
            return true;
        }
    }
    return itemId == CustomDesignPathPermit;
}
CatalogStatus QueryCatalog(std::uint8_t player, const game::CatalogItem &item, Surface surface) {
    game::ItemMetadata metadata;
    if (player >= ResidentCount || item.id >= CatalogItemLimit || IsExcludedReward(item.id) ||
        (item.flags[1] & UnsupportedItemFlags) || !game::ReadCatalogMetadata(item, metadata) ||
        !IsEligibleKind(metadata, surface)) {
        return CatalogStatus::Unavailable;
    }
    const bool artStars = GetSettings().artDonationStars;
    if (game::IsCreatureKind(metadata) || (artStars && game::IsGenuineArtKind(metadata))) {
        return game::ReadMuseumDonation(item);
    }
    game::CatalogHistory history;
    if (!game::OpenCatalogHistory(player, item, history)) {
        return CatalogStatus::Unavailable;
    }
    if (!game::HasCatalogedItem(history, item)) {
        return artStars && game::IsForgedArtKind(metadata) ? CatalogStatus::FakeArtUncataloged
                                                        : CatalogStatus::Uncataloged;
    }
    if (!game::IsFurnitureKind(metadata) || metadata.remakeId < 0) {
        return CatalogStatus::Cataloged;
    }
    return game::ReadFurnitureVariant(history, item, metadata.remakeId);
}
char16_t CatalogMarker(CatalogStatus status) {
    if (status == CatalogStatus::Uncataloged) {
        return u'★';
    }
    if (status == CatalogStatus::VariantUncataloged || status == CatalogStatus::FakeArtUncataloged) {
        return u'☆';
    }
    return 0;
}
MarkerDecision DecideItemMarker(std::uint8_t player, const game::CatalogItem &item, ItemUse use,
                                Surface surface, std::uintptr_t event) {
    MarkerDecision result;
    // Only Miles row badges interpret a recipe card instead of catalog history.
    // Other name surfaces have explicit recipe semantics; learned state never
    // falls back to ownership of the crafted output, even when unavailable.
    result.isRecipe =
        use == ItemUse::VillagerCraft ||
        (item.id == game::RecipeCardId && (use != ItemUse::CatalogRow || surface == Surface::Miles));
    if (result.isRecipe) {
        result.recipeId = item.id == game::RecipeCardId
                              ? static_cast<std::uint16_t>(item.extra) : 0xffff;
        result.recipe = use == ItemUse::VillagerCraft
                            ? game::QueryVillagerCraftRecipe(player, event, result.recipeId)
                            : game::QueryRecipeCard(player, item);
        result.marker = result.recipe == RecipeStatus::Unlearned ? u'★' : 0;
    } else {
        if (use == ItemUse::InventoryCollectible || use == ItemUse::CatchDialogue) {
            game::ItemMetadata metadata;
            if (game::ReadCatalogMetadata(item, metadata) && game::IsCreatureKind(metadata)) {
                result.catalog = game::ReadMuseumDonation(item);
            }
        } else {
            result.catalog = QueryCatalog(player, item, surface);
        }
        result.marker = CatalogMarker(result.catalog);
    }
    return result;
}
bool ShowRowBadge(const MarkerDecision &decision, bool completeLayout) {
    // Always write this result to every pane that was found, including false.
    // A missing body color earns a label ☆, not a filled family badge.
    return completeLayout && (decision.isRecipe ? decision.recipe == RecipeStatus::Unlearned
                                               : decision.catalog == CatalogStatus::Uncataloged);
}
bool CopyVariationLabel(char16_t (&output)[VariationLabelUnits], const char16_t *text,
                        unsigned length, CatalogStatus status) {
    if (status != CatalogStatus::Uncataloged && status != CatalogStatus::VariantUncataloged) {
        return false;
    }
    if (!text || !length || length + 3ull > VariationLabelUnits) {
        return false;
    }
    if (text[0] == u'☆' || text[0] == u'★') {
        return false;
    }
    for (unsigned i = 0; i < length; ++i) {
        if (text[i] < 0x20 || text[i] == 0x7f) {
            return false;
        }
    }
    if (text[length] != 0) {
        return false;
    }
    output[0] = u'☆';
    output[1] = u' ';
    std::memcpy(output + 2, text, (length + 1) * sizeof(char16_t));
    return true;
}
ShopIndex IdentifyShopMessage(std::string_view bank, std::string_view label) {
    if (bank == "SP_rcm_00_common" && label == "101") {
        return NookShop;
    }
    if (bank == "SYS_Wherearen_OfficeShop" && label == "001") {
        return HhpShop;
    }
    if (bank == "SP_kpg" && label == "151") {
        return HotelShop;
    }
    if (bank == "SP_hgh" && (label == "051" || label == "052")) {
        return AbleShop;
    }
    return UnsupportedShop;
}
} // namespace catalog_aware_shops::features
