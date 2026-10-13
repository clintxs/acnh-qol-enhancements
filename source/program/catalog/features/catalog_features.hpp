#pragma once
#include "program/catalog/game/catalog_ownership.hpp"
#include "program/catalog/game/recipe_knowledge.hpp"
#include <cstddef>
#include <string_view>
namespace catalog_aware_shops::features {
// These scopes retain the supported native UI paths. Cabinet also serves
// ordinary ground/dialogue names; inventory names only mark recipes/creatures.
enum class Surface { Cabinet, Furniture, FittingRoom, Miles };
enum class ItemUse { CatalogRow, InventoryCollectible, ItemName, CatchDialogue, VillagerCraft };
struct MarkerDecision {
    game::CatalogStatus catalog = game::CatalogStatus::Unavailable;
    game::RecipeStatus recipe = game::RecipeStatus::Unavailable;
    std::uint16_t recipeId = 0xffff;
    bool isRecipe = false;
    char16_t marker = 0;
};
bool IsEligibleKind(const game::ItemMetadata &, Surface);
bool IsExcludedReward(std::uint16_t);
game::CatalogStatus QueryCatalog(std::uint8_t, const game::CatalogItem &, Surface);
// Inventory/catch callers validate the borrowed resident before calling.
// The returned decision retains values only, including actual recipe identity.
MarkerDecision DecideItemMarker(std::uint8_t, const game::CatalogItem &, ItemUse,
                                Surface = Surface::Cabinet, std::uintptr_t event = 0);
char16_t CatalogMarker(game::CatalogStatus);
bool ShowRowBadge(const MarkerDecision &, bool completeLayout);
inline constexpr std::size_t VariationLabelUnits = 256;
// Copies into a caller-owned fixed buffer; the native setter copies synchronously.
bool CopyVariationLabel(char16_t (&output)[VariationLabelUnits], const char16_t *, unsigned,
                        game::CatalogStatus);
enum ShopIndex { UnsupportedShop, NookShop, HhpShop, HotelShop, AbleShop };
ShopIndex IdentifyShopMessage(std::string_view bank, std::string_view label);
} // namespace catalog_aware_shops::features
