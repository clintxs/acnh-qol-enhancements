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
enum class CatalogStatus { Unavailable, Cataloged, Uncataloged, VariantUncataloged, FakeArtUncataloged };
// A validated resource observation, not ownership. Hashes are native ItemKind
// identifiers; missing identity/fields remain distinguishable from an unknown item.
struct ItemMetadata {
    std::uint32_t kindHash = 0;
    std::int16_t remakeId = -1;
    bool identityMatches = false;
};
// Callback-local borrow of a ready live catalog view. Never retain across UI updates.
struct CatalogHistory {
    std::uintptr_t view{};
    std::uint8_t player{};
};
bool ReadCatalogMetadata(const CatalogItem &, ItemMetadata &);
enum class ItemKind { Ordinary, Currency, Recipe, AbilityBook, Furniture, Clothing };
ItemKind ClassifyItemKind(const ItemMetadata &);
bool IsFurnitureKind(const ItemMetadata &);
bool IsCreatureKind(const ItemMetadata &);
bool IsGenuineArtKind(const ItemMetadata &);
bool IsForgedArtKind(const ItemMetadata &);
bool OpenCatalogHistory(std::uint8_t, const CatalogItem &, CatalogHistory &);
// Call these only after OpenCatalogHistory succeeds, within the same callback.
// Native queries own their read locks; this borrow owns no lock or save memory.
bool HasCatalogedItem(const CatalogHistory &, const CatalogItem &);
CatalogStatus ReadFurnitureVariant(const CatalogHistory &, const CatalogItem &, std::int16_t);
CatalogStatus ReadMuseumDonation(const CatalogItem &);
bool ValidateCatalogBindings();
} // namespace catalog_aware_shops::game
