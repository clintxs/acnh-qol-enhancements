#pragma once
#include <cstdint>

namespace catalog_aware_shops::game {
// Exact native 8-byte item value. Copy it intact: clothing color is encoded in
// id; furniture body is in extra's low three bits. Other bits are preserved.
struct alignas(4) CatalogItem {
    std::uint16_t id;
    std::uint8_t flags[2];
    std::uint32_t extra;
};
static_assert(sizeof(CatalogItem) == 8);
// Unavailable is a separate result from Uncataloged: missing/unsupported
// runtime data must never produce a misleading star. Values are used in logs.
enum class CatalogStatus { Unavailable, Cataloged, Uncataloged, VariantUncataloged };
constexpr char16_t CatalogMarker(CatalogStatus status) {
    return status == CatalogStatus::Uncataloged          ? u'★'
           : status == CatalogStatus::VariantUncataloged ? u'☆'
                                                         : 0;
}
// Each query reads the active player's historical catalog, not current pockets.
// Cabinet: ordinary item kinds except recipes (also used for new shop dialogue).
// Miles: ordinary items except recipe books and ability/upgrade books.
// Furniture: furniture-only, for the original Nook display message.
// FittingRoom: the eight supported clothing kinds; each color has its own ID.
// No query writes history or claims a variant on behalf of the game.
CatalogStatus QueryCabinetCatalog(std::uint8_t player, const CatalogItem &item);
CatalogStatus QueryMilesCatalog(std::uint8_t player, const CatalogItem &item);
CatalogStatus QueryFurnitureCatalog(std::uint8_t player, const CatalogItem &item);
CatalogStatus QueryFittingRoomCatalog(std::uint8_t player, const CatalogItem &item);
bool ValidateCatalogBindings();
} // namespace catalog_aware_shops::game
