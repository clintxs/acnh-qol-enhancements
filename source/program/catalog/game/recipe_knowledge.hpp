#pragma once
#include "catalog_ownership.hpp"

namespace catalog_aware_shops::game {
// A recipe's learned state is independent of catalog ownership of its result.
enum class RecipeStatus { Unavailable, Learned, Unlearned };
constexpr std::uint16_t RecipeCardId = 5794;
RecipeStatus QueryRecipeCard(std::uint8_t player, const CatalogItem &item);
// Reads the event-owned villager's current DIY/cooking recipe; never infers it
// from the output item, since different recipes can produce the same item.
RecipeStatus QueryVillagerCraftRecipe(std::uint8_t player, std::uintptr_t event,
                                     std::uint16_t &recipe);
bool ValidateRecipeBindings();
} // namespace catalog_aware_shops::game
