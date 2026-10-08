#include "catalog_ownership.hpp"
#include "bindings.hpp"
#include <array>
#include <cstring>

namespace catalog_aware_shops::game {
namespace {
// These are offsets from the loaded main module, never absolute addresses.
// GOT entries contain a pointer to a global pointer; direct globals have only
// one indirection. Keeping those two access paths distinct matters under ASLR.
namespace address {
constexpr std::uintptr_t ItemParamGlobalSlot = 0x04ac0428;
constexpr std::uintptr_t PlayerReadinessGlobalSlot = 0x04ab4fb8;
constexpr std::uintptr_t ItemManagerGlobalSlot = 0x04ab43c0;
constexpr std::uintptr_t SaveManagerGlobal = 0x054021a0;
constexpr std::uintptr_t FallbackSaveGlobal = 0x05474040;
constexpr std::uintptr_t ItemParamGlobal = 0x0522a768;
constexpr std::uintptr_t ItemRemakeGlobal = 0x0522a7c8;
constexpr std::uintptr_t ItemAccessorVtable = 0x04936b50;
constexpr std::uintptr_t ResolvePlayerIndex = 0x02482d8c;
constexpr std::uintptr_t ResolvePlayerSave = 0x02483354;
constexpr std::uintptr_t HasCatalogedItem = 0x0254a1e0;
constexpr std::uintptr_t GetBodyVariantCount = 0x019d7260;
constexpr std::uintptr_t GetVariantHistoryIndex = 0x01a00960;
constexpr std::uintptr_t HasCatalogedVariant = 0x0254ccd0;
} // namespace address
namespace layout {
// Runtime lookup table used by both ItemParam and ItemRemake.
constexpr std::size_t FirstRowId = 0x1f8, RowCount = 0x1d8, RowPointers = 0x1e0;
constexpr std::size_t PlayerRecordSize = 0x13f0;
constexpr std::size_t ReadyFlagA = 0x13d4, ReadyFlagB = 0x13d6;
constexpr std::size_t SaveCatalogView = 0x194b0, CatalogViewReady = 8;
constexpr std::size_t FallbackCatalogView = 0x20;
constexpr std::size_t ItemHistory = 0x1070, VariantHistory = 0x10e0;
constexpr std::size_t ItemManagerLookup = 0x57e58;
} // namespace layout
namespace bcsv {
// BCSV field/enum identifiers are CRC32 hashes, not addresses. The file may
// encode enum values using English or Japanese names, so accept both below.
constexpr std::uint32_t RemakeId = 0xcb5eb33f, ItemKind = 0x5270eb75, UniqueId = 0x54706054;
constexpr std::uint32_t FurnitureEnglish = 0x7e0e8482, FurnitureJapanese = 0xbc9d4349;
constexpr std::size_t HeaderV0Bytes = 12, HeaderV1Bytes = 28, FieldDescriptorBytes = 8;
constexpr std::uint32_t MaxRows = 65536, MaxRowBytes = 4096, MaxFields = 512;
constexpr std::uint32_t MaxHeaderDistance = 64 * 1024 * 1024;
} // namespace bcsv
constexpr unsigned PlayerCount = 8, ItemHistoryBitCount = 15000, VariantHistoryBitCount = 16000;
constexpr unsigned BodyVariantsPerRemake = 8, BodyVariantMask = 7;
constexpr unsigned UnsupportedItemFlagsMask = 3;
constexpr unsigned LocalPlayerLookupMode = 1;

template <class T> T Read(std::uintptr_t object, std::size_t offset = 0) {
    T value;
    std::memcpy(&value, reinterpret_cast<const void *>(object + offset), sizeof(value));
    return value;
}
std::uintptr_t ReadIndirectGlobal(std::uintptr_t moduleOffset) {
    const auto pointerSlot = Read<std::uintptr_t>(MainAddress(moduleOffset));
    return pointerSlot ? Read<std::uintptr_t>(pointerSlot) : 0;
}
enum class Surface { Cabinet, Furniture, FittingRoom, Miles };
struct ItemMetadata {
    std::uint32_t kindHash = 0;
    std::int16_t remakeId = -1;
    bool identityMatches = false;
};

// Validate the live BCSV row before interpreting its fields. This describes an
// in-memory resource table, NOT a save-file layout. Unknown/malformed data must
// never be mistaken for an item the player has not cataloged.
bool ReadItemMetadata(const CatalogItem &item, ItemMetadata &metadata) {
    const auto itemTable = ReadIndirectGlobal(address::ItemParamGlobalSlot);
    if (!itemTable) {
        return false;
    }
    const auto firstItemId = Read<std::uint16_t>(itemTable, layout::FirstRowId);
    const auto rowCount = Read<std::uint32_t>(itemTable, layout::RowCount);
    const auto rowPointers = Read<std::uintptr_t>(itemTable, layout::RowPointers);
    if (!rowPointers || rowCount > bcsv::MaxRows || item.id < firstItemId ||
        static_cast<unsigned>(item.id - firstItemId) >= rowCount) {
        return false;
    }
    const auto itemRow =
        Read<std::uintptr_t>(rowPointers, sizeof(std::uintptr_t) * (item.id - firstItemId));
    if (!itemRow) {
        return false;
    }
    const auto headerDistance = Read<std::uint32_t>(itemRow);
    if (headerDistance < bcsv::HeaderV1Bytes || headerDistance > bcsv::MaxHeaderDistance ||
        itemRow < headerDistance) {
        return false;
    }
    const auto header = itemRow - headerDistance;
    const auto entryCount = Read<std::uint32_t>(header);
    const auto rowBytes = Read<std::uint32_t>(header, 4);
    const auto fieldCount = Read<std::uint16_t>(header, 8);
    const auto version = Read<std::uint8_t>(header, 10);
    if (!entryCount || entryCount > bcsv::MaxRows || rowBytes < 4 || rowBytes > bcsv::MaxRowBytes ||
        !fieldCount || fieldCount > bcsv::MaxFields || version > 1) {
        return false;
    }
    const auto fieldDescriptors = header + (version ? bcsv::HeaderV1Bytes : bcsv::HeaderV0Bytes);
    const auto firstRow = fieldDescriptors + bcsv::FieldDescriptorBytes * fieldCount;
    if (itemRow < firstRow || (itemRow - firstRow) % rowBytes ||
        (itemRow - firstRow) / rowBytes >= entryCount) {
        return false;
    }
    for (unsigned fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex) {
        const auto field = fieldDescriptors + bcsv::FieldDescriptorBytes * fieldIndex;
        const auto fieldHash = Read<std::uint32_t>(field);
        const auto fieldOffset = Read<std::uint32_t>(field, 4);
        if (fieldHash == bcsv::RemakeId && fieldOffset <= rowBytes - 2) {
            metadata.remakeId = Read<std::int16_t>(itemRow, fieldOffset);
        }
        if (fieldHash == bcsv::ItemKind && fieldOffset <= rowBytes - 4) {
            metadata.kindHash = Read<std::uint32_t>(itemRow, fieldOffset);
        }
        if (fieldHash == bcsv::UniqueId && fieldOffset <= rowBytes - 2) {
            metadata.identityMatches = Read<std::uint16_t>(itemRow, fieldOffset) == item.id;
        }
    }
    return true;
}
bool IsFurniture(const ItemMetadata &metadata) {
    return metadata.kindHash == bcsv::FurnitureEnglish ||
           metadata.kindHash == bcsv::FurnitureJapanese;
}
bool IsEligibleKind(const ItemMetadata &metadata, Surface surface) {
    // Money covers every coin/bag denomination, including Bells shaken from
    // trees. It is currency, not a collectible. Match both ItemKind spellings.
    constexpr std::uint32_t MoneyEnglish = 0x761e3ce0, MoneyJapanese = 0x3f54ec25;
    if (metadata.kindHash == MoneyEnglish || metadata.kindHash == MoneyJapanese) {
        return false;
    }
    // Nook Miles mixes physical items with unlocks such as hairstyles, pocket
    // upgrades and recipe bundles. These books do not have catalog ownership.
    constexpr std::uint32_t HowtoBook = 0x8d9857c4; // CRC32("DummyHowtoBook")
    if (surface == Surface::Miles && metadata.kindHash == HowtoBook) {
        return false;
    }
    constexpr std::uint32_t recipeKinds[]{
        0xeea2c69e, 0x62291ce3, // DummyRecipe: DIY/cooking books, English/Japanese.
        0x6693e43a, 0x5280a2c3, // DummyDIYRecipe: shop recipe cards.
        0x890e26a5, 0x698b48d4  // DIYRecipe: inventory recipe cards.
    };
    for (const auto recipeKind : recipeKinds) {
        if (metadata.kindHash == recipeKind) {
            return false;
        }
    }
    if (surface == Surface::FittingRoom) {
        constexpr std::uint32_t clothingKinds[]{
            0xc0939284, 0x7544fe0e, // Tops
            0x55067543, 0x83552e45, // OnePiece
            0x0581ce14, 0x34fa4b7e, // Bottoms
            0xdf4042c0, 0x042bd987, // Socks
            0xd50eae93, 0x51a1a70e, // Shoes
            0xa17ec151, 0xc1af5477, // Cap
            0x7d06eb90, 0xd1982e25, // Helmet
            0x2340a7bf, 0x8df84ea5  // Accessory
        };
        for (const auto clothingKind : clothingKinds) {
            if (metadata.kindHash == clothingKind) {
                return metadata.identityMatches;
            }
        }
        return false;
    }
    if (surface == Surface::Furniture) {
        return metadata.identityMatches && IsFurniture(metadata);
    }
    return metadata.identityMatches; // Shared lists also sell ordinary tools, plants, etc.
}

std::uintptr_t FindReadyCatalogView(std::uint8_t player) {
    const auto playerIndex = FunctionAt<unsigned (*)(const std::uint8_t *, unsigned)>(
        address::ResolvePlayerIndex)(&player, LocalPlayerLookupMode);
    const auto readinessRecords = ReadIndirectGlobal(address::PlayerReadinessGlobalSlot);
    if (playerIndex >= PlayerCount || !readinessRecords ||
        !Read<std::uint8_t>(readinessRecords,
                            playerIndex * layout::PlayerRecordSize + layout::ReadyFlagA) ||
        !Read<std::uint8_t>(readinessRecords,
                            playerIndex * layout::PlayerRecordSize + layout::ReadyFlagB)) {
        return 0;
    }
    const auto saveManager = Read<std::uintptr_t>(MainAddress(address::SaveManagerGlobal));
    if (!saveManager) {
        return 0;
    }
    const auto playerSave =
        FunctionAt<std::uintptr_t (*)(std::uintptr_t, const std::uint8_t *, unsigned)>(
            address::ResolvePlayerSave)(saveManager, &player, LocalPlayerLookupMode);
    if (!playerSave) {
        return 0;
    }
    auto catalogView = playerSave + layout::SaveCatalogView;
    if (!Read<std::uintptr_t>(catalogView, layout::CatalogViewReady)) {
        const auto fallbackSave = Read<std::uintptr_t>(MainAddress(address::FallbackSaveGlobal));
        if (!fallbackSave) {
            return 0;
        }
        catalogView = Read<std::uintptr_t>(fallbackSave, layout::FallbackCatalogView);
    }
    return catalogView && Read<std::uintptr_t>(catalogView, layout::ItemHistory) ? catalogView : 0;
}

// The native history query independently dereferences this lookup table. Check
// it before calling the game, even though we already validated ItemParam above.
bool HasNativeItemLookup(std::uint16_t itemId) {
    const auto itemManager = ReadIndirectGlobal(address::ItemManagerGlobalSlot);
    if (!itemManager) {
        return false;
    }
    const auto itemLookup = Read<std::uintptr_t>(itemManager, layout::ItemManagerLookup);
    if (!itemLookup) {
        return false;
    }
    const auto firstItemId = Read<std::uint16_t>(itemLookup, layout::FirstRowId);
    const auto rowPointers = Read<std::uintptr_t>(itemLookup, layout::RowPointers);
    return rowPointers && itemId >= firstItemId &&
           static_cast<unsigned>(itemId - firstItemId) <
               Read<std::uint32_t>(itemLookup, layout::RowCount) &&
           Read<std::uintptr_t>(rowPointers, sizeof(std::uintptr_t) * (itemId - firstItemId));
}

// Called only after the main item is known. Native getters preserve the game's
// body/color interpretation and read-lock callbacks; fabric/custom designs do
// not create additional missing catalog variants.
CatalogStatus QueryFurnitureVariant(std::uint8_t player, const CatalogItem &item,
                                    std::int16_t remakeId, std::uintptr_t catalogView) {
    const auto itemParams = Read<std::uintptr_t>(MainAddress(address::ItemParamGlobal));
    const auto itemRemakes = Read<std::uintptr_t>(MainAddress(address::ItemRemakeGlobal));
    if (!itemParams || itemParams != ReadIndirectGlobal(address::ItemParamGlobalSlot) ||
        !itemRemakes || !Read<std::uintptr_t>(catalogView, layout::VariantHistory)) {
        return CatalogStatus::Unavailable;
    }
    const auto firstRemakeId = Read<std::uint16_t>(itemRemakes, layout::FirstRowId);
    const auto remakeRows = Read<std::uintptr_t>(itemRemakes, layout::RowPointers);
    if (!remakeRows || remakeId < firstRemakeId ||
        static_cast<unsigned>(remakeId - firstRemakeId) >=
            Read<std::uint32_t>(itemRemakes, layout::RowCount)) {
        return CatalogStatus::Unavailable;
    }
    if (!Read<std::uintptr_t>(remakeRows, sizeof(std::uintptr_t) * (remakeId - firstRemakeId))) {
        return CatalogStatus::Unavailable;
    }
    // Native ABI: vtable + the exact 8-byte item + its ItemParam row pointer.
    struct ItemAccessor {
        std::uintptr_t vtable;
        CatalogItem item;
        std::uintptr_t parameterRow;
    };
    const auto parameterRows = Read<std::uintptr_t>(itemParams, layout::RowPointers);
    const auto firstItemId = Read<std::uint16_t>(itemParams, layout::FirstRowId);
    if (!parameterRows || item.id < firstItemId ||
        static_cast<unsigned>(item.id - firstItemId) >=
            Read<std::uint32_t>(itemParams, layout::RowCount)) {
        return CatalogStatus::Unavailable;
    }
    ItemAccessor accessor{
        MainAddress(address::ItemAccessorVtable), item,
        Read<std::uintptr_t>(parameterRows, sizeof(std::uintptr_t) * (item.id - firstItemId))};
    const auto bodyCount =
        FunctionAt<unsigned (*)(const ItemAccessor *)>(address::GetBodyVariantCount)(&accessor);
    if (bodyCount < 2) {
        return CatalogStatus::Cataloged;
    }
    const auto displayedBody = item.extra & BodyVariantMask;
    if (bodyCount > BodyVariantsPerRemake || displayedBody >= bodyCount) {
        return CatalogStatus::Unavailable;
    }
    const auto historyIndex =
        FunctionAt<unsigned (*)(const CatalogItem *)>(address::GetVariantHistoryIndex)(&item);
    if (historyIndex >= VariantHistoryBitCount ||
        historyIndex != static_cast<unsigned>(remakeId) * BodyVariantsPerRemake + displayedBody) {
        return CatalogStatus::Unavailable;
    }
    return FunctionAt<unsigned (*)(const std::uint8_t *, const CatalogItem *)>(
               address::HasCatalogedVariant)(&player, &item)
               ? CatalogStatus::Cataloged
               : CatalogStatus::VariantUncataloged;
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
CatalogStatus QueryCatalog(std::uint8_t player, const CatalogItem &item, Surface surface) {
    ItemMetadata metadata;
    if (player >= PlayerCount || item.id >= ItemHistoryBitCount || IsExcludedReward(item.id) ||
        (item.flags[1] & UnsupportedItemFlagsMask) || !ReadItemMetadata(item, metadata) ||
        !IsEligibleKind(metadata, surface)) {
        return CatalogStatus::Unavailable;
    }
    const auto catalogView = FindReadyCatalogView(player);
    if (!catalogView || !HasNativeItemLookup(item.id)) {
        return CatalogStatus::Unavailable;
    }
    const bool mainItemKnown = FunctionAt<unsigned (*)(const std::uint8_t *, const CatalogItem *)>(
        address::HasCatalogedItem)(&player, &item);
    if (!mainItemKnown) {
        return CatalogStatus::Uncataloged;
    }
    if (!IsFurniture(metadata) || metadata.remakeId < 0) {
        return CatalogStatus::Cataloged;
    }
    return QueryFurnitureVariant(player, item, metadata.remakeId, catalogView);
}
} // namespace

CatalogStatus QueryCabinetCatalog(std::uint8_t player, const CatalogItem &item) {
    return QueryCatalog(player, item, Surface::Cabinet);
}
CatalogStatus QueryMilesCatalog(std::uint8_t player, const CatalogItem &item) {
    return QueryCatalog(player, item, Surface::Miles);
}
CatalogStatus QueryFurnitureCatalog(std::uint8_t player, const CatalogItem &item) {
    return QueryCatalog(player, item, Surface::Furniture);
}
CatalogStatus QueryFittingRoomCatalog(std::uint8_t player, const CatalogItem &item) {
    return QueryCatalog(player, item, Surface::FittingRoom);
}
// Instruction words below are intentional raw ARM fingerprints. They are build
// evidence, not feature logic: changing them requires re-verifying the target
// game's disassembly. A mismatch disables hooks instead of guessing a layout.
bool ValidateCatalogBindings() {
    struct Check {
        std::uintptr_t offset;
        std::array<std::uint32_t, 5> words;
    };
    constexpr Check checks[]{
        {0x0254ccd0, {0xd100c3ff, 0xa9017bfd, 0xa9024ff4, 0x910043fd, 0x39400008}},
        {0x01a00960, {0xd10203ff, 0xa9027bfd, 0xa9036ffc, 0xa90467fa, 0xa9055ff8}},
        {0x019d7260, {0xa9ba7bfd, 0xa9016ffc, 0xa90267fa, 0xa9035ff8, 0xa90457f6}},
        {0x019fd600, {0xf9400808, 0x5280178a, 0xb40000e8, 0xb9400109, 0x7100013f}},

        {0x0254a1e0, {0xa9bd7bfd, 0xf9000bf5, 0xa9024ff4, 0x910003fd, 0x39400008}},
        {0x0267a1f4, {0xa9bc7bfd, 0xf9000bfc, 0x910003fd, 0xa90257f6, 0xa9034ff4}},
        {0x0267d6c4, {0xa9be7bfd, 0xf9000bf3, 0x910003fd, 0xaa0003e8, 0x79400000}},
        {0x02482d8c, {0xa9bc7bfd, 0xf9000bf7, 0xa90257f6, 0xa9034ff4, 0x910003fd}},
        {0x02483354, {0xd10143ff, 0xa9027bfd, 0xa90357f6, 0xa9044ff4, 0x910083fd}}};
    for (const auto &c : checks) {
        if (std::memcmp(reinterpret_cast<const void *>(MainAddress(c.offset)), c.words.data(),
                        sizeof(c.words))) {
            return false;
        }
    }
    return true;
}
} // namespace catalog_aware_shops::game
