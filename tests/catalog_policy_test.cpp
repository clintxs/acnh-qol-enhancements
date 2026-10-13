#include "program/catalog/features/catalog_features.hpp"
#include "program/catalog/settings.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace catalog_aware_shops;
namespace {
Settings settings;
game::ItemMetadata metadata{0, 2, true};
game::ItemKind kind = game::ItemKind::Furniture;
bool metadataReady = true, historyReady = true, known = false;
bool creature = false, genuine = false, forged = false;
game::CatalogStatus donation = game::CatalogStatus::Uncataloged;
game::CatalogStatus variant = game::CatalogStatus::VariantUncataloged;
game::RecipeStatus recipe = game::RecipeStatus::Unlearned;
unsigned opens = 0, ownershipReads = 0, variantReads = 0, donationReads = 0, recipeReads = 0;
void ResetCounts() { opens = ownershipReads = variantReads = donationReads = recipeReads = 0; }
}
namespace catalog_aware_shops {
const Settings &GetSettings() { return settings; }
namespace game {
bool ReadCatalogMetadata(const CatalogItem &, ItemMetadata &out) { out = metadata; return metadataReady; }
ItemKind ClassifyItemKind(const ItemMetadata &) { return kind; }
bool IsFurnitureKind(const ItemMetadata &) { return kind == ItemKind::Furniture; }
bool IsCreatureKind(const ItemMetadata &) { return creature && metadata.identityMatches; }
bool IsGenuineArtKind(const ItemMetadata &) { return genuine; }
bool IsForgedArtKind(const ItemMetadata &) { return forged; }
bool OpenCatalogHistory(std::uint8_t player, const CatalogItem &, CatalogHistory &out) {
    ++opens; out = {123, player}; return historyReady;
}
bool HasCatalogedItem(const CatalogHistory &view, const CatalogItem &) {
    assert(view.view == 123 && view.player == 2); ++ownershipReads; return known;
}
CatalogStatus ReadFurnitureVariant(const CatalogHistory &view, const CatalogItem &, std::int16_t remake) {
    assert(view.view == 123 && remake == 2); ++variantReads; return variant;
}
CatalogStatus ReadMuseumDonation(const CatalogItem &) { ++donationReads; return donation; }
RecipeStatus QueryRecipeCard(std::uint8_t, const CatalogItem &) { ++recipeReads; return recipe; }
RecipeStatus QueryVillagerCraftRecipe(std::uint8_t, std::uintptr_t event, std::uint16_t &id) {
    assert(event == 456); id = 19; ++recipeReads; return recipe;
}
}
}
int main() {
    using namespace features;
    using game::CatalogStatus;
    using game::ItemKind;
    game::CatalogItem item{100, {0, 0}, 3};
    const auto query = [&] { return QueryCatalog(2, item, Surface::Cabinet); };
    for (const auto surface : {Surface::Cabinet, Surface::Miles, Surface::Furniture, Surface::FittingRoom}) {
        for (const auto candidate : {ItemKind::Ordinary, ItemKind::Currency, ItemKind::Recipe,
                                    ItemKind::AbilityBook, ItemKind::Furniture, ItemKind::Clothing}) {
            kind = candidate;
            const bool eligible = candidate != ItemKind::Currency && candidate != ItemKind::Recipe &&
                !(surface == Surface::Miles && candidate == ItemKind::AbilityBook) &&
                (surface != Surface::Furniture || candidate == ItemKind::Furniture) &&
                (surface != Surface::FittingRoom || candidate == ItemKind::Clothing);
            assert(IsEligibleKind(metadata, surface) == eligible);
            metadata.identityMatches = false;
            assert(!IsEligibleKind(metadata, surface)); metadata.identityMatches = true;
        }
    }
    kind = ItemKind::Furniture;
    for (auto excluded : {5851,9964,2543,3075,3247,8773,8774,8775,8776,8777,8778,8779,8780,8781,9771}) {
        item.id = excluded; ResetCounts(); assert(query() == CatalogStatus::Unavailable); assert(opens == 0);
    }
    item.id = 100;
    assert(QueryCatalog(8,item,Surface::Cabinet) == CatalogStatus::Unavailable);
    item.flags[1] = 1; assert(query() == CatalogStatus::Unavailable); item.flags[1] = 0;
    metadataReady = false; assert(query() == CatalogStatus::Unavailable); metadataReady = true;
    historyReady = false; ResetCounts(); assert(query() == CatalogStatus::Unavailable);
    assert(opens == 1 && ownershipReads == 0); historyReady = true;
    ResetCounts(); assert(query() == CatalogStatus::Uncataloged);
    assert(opens == 1 && ownershipReads == 1 && variantReads == 0);
    known = true; ResetCounts(); assert(query() == CatalogStatus::VariantUncataloged);
    assert(opens == 1 && ownershipReads == 1 && variantReads == 1);
    variant = CatalogStatus::Unavailable; assert(query() == CatalogStatus::Unavailable);
    metadata.remakeId = -1; assert(query() == CatalogStatus::Cataloged); metadata.remakeId = 2;
    known = false; forged = true; assert(query() == CatalogStatus::FakeArtUncataloged);
    settings.artDonationStars = false; assert(query() == CatalogStatus::Uncataloged);
    forged = false; genuine = true; assert(query() == CatalogStatus::Uncataloged);
    settings.artDonationStars = true; ResetCounts(); assert(query() == donation);
    assert(donationReads == 1 && opens == 0 && ownershipReads == 0);
    genuine = false; creature = true; settings.artDonationStars = false;
    ResetCounts(); assert(query() == donation); assert(donationReads == 1 && opens == 0);
    creature = false; settings.artDonationStars = true;
    const auto unknown = DecideItemMarker(2,item,ItemUse::InventoryCollectible);
    assert(unknown.marker == 0);
    item.id = game::RecipeCardId;
    for (auto use : {ItemUse::InventoryCollectible,ItemUse::ItemName,ItemUse::CatchDialogue,ItemUse::CatalogRow}) {
        ResetCounts(); auto decision = DecideItemMarker(2,item,use,Surface::Miles);
        assert(decision.isRecipe && decision.marker == u'★' && recipeReads == 1 && opens == 0);
        assert(ShowRowBadge(decision,true) && !ShowRowBadge(decision,false));
    }
    kind = ItemKind::Recipe;
    ResetCounts();
    auto cabinetRecipe = DecideItemMarker(2,item,ItemUse::CatalogRow,Surface::Cabinet);
    assert(!cabinetRecipe.isRecipe && cabinetRecipe.marker == 0 && recipeReads == 0 && opens == 0);
    recipe = game::RecipeStatus::Learned;
    assert(DecideItemMarker(2,item,ItemUse::ItemName).marker == 0);
    recipe = game::RecipeStatus::Unlearned;
    kind = ItemKind::Furniture;
    item.id = 100;
    ResetCounts(); auto decision = DecideItemMarker(2,item,ItemUse::VillagerCraft,Surface::Cabinet,456);
    assert(decision.recipeId == 19 && decision.marker == u'★' && opens == 0);
    recipe = game::RecipeStatus::Unavailable;
    decision = DecideItemMarker(2,item,ItemUse::VillagerCraft,Surface::Cabinet,456);
    assert(decision.marker == 0 && !ShowRowBadge(decision,true) && opens == 0);
    for (auto status : {CatalogStatus::Unavailable,CatalogStatus::Cataloged,CatalogStatus::Uncataloged,
                        CatalogStatus::VariantUncataloged,CatalogStatus::FakeArtUncataloged}) {
        decision = {}; decision.catalog = status;
        assert(ShowRowBadge(decision,true) == (status == CatalogStatus::Uncataloged));
        assert(!ShowRowBadge(decision,false));
        char16_t output[256]{u'x'};
        const bool changed = CopyVariationLabel(output,u"Red",3,status);
        assert(changed == (status == CatalogStatus::Uncataloged || status == CatalogStatus::VariantUncataloged));
        if (changed) assert(std::u16string_view(output) == u"☆ Red"); else assert(output[0] == u'x');
    }
    char16_t output[256]{u'x'};
    for (auto text : {u"",u"★ Red",u"☆ Red",u"\x0eRed",u"R\x7f"}) {
        assert(!CopyVariationLabel(output,text,std::char_traits<char16_t>::length(text),CatalogStatus::Uncataloged));
    }
    assert(!CopyVariationLabel(output,u"Red",2,CatalogStatus::Uncataloged));
    assert(IdentifyShopMessage("SP_rcm_00_common","101") == NookShop);
    assert(IdentifyShopMessage("SYS_Wherearen_OfficeShop","001") == HhpShop);
    assert(IdentifyShopMessage("SP_kpg","151") == HotelShop);
    assert(IdentifyShopMessage("SP_hgh","051") == AbleShop);
    assert(IdentifyShopMessage("SP_hgh","052") == AbleShop);
    assert(IdentifyShopMessage("SP_hgh","053") == UnsupportedShop);
    assert(CatalogMarker(CatalogStatus::Unavailable) == 0);
    assert(CatalogMarker(CatalogStatus::Uncataloged) == u'★');
    assert(CatalogMarker(CatalogStatus::VariantUncataloged) == u'☆');
    assert(CatalogMarker(CatalogStatus::FakeArtUncataloged) == u'☆');
    std::cout << "PASS Catalog policy: surface eligibility, history ordering/frequency, recipe identity, unknown fallback, markers and reused-row visibility\n";
}
