#pragma once
#include "catalog_ownership.hpp"

namespace catalog_aware_shops::game {
// A recipe's learned state is independent of catalog ownership of its result.
enum class RecipeStatus { Unavailable, Learned, Unlearned };
constexpr std::uint16_t RecipeCardId = 5794;
RecipeStatus QueryRecipeCard(std::uint8_t player, const CatalogItem &item);
bool ValidateRecipeBindings();
} // namespace catalog_aware_shops::game
