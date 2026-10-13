#include "core_features.hpp"
#include "program/core/modules.hpp"
#include "program/core/native/core_operations.hpp"
#include "program/core/resource_budget.hpp"
#include "program/controls/settings.hpp"
#include "program/crafting/settings.hpp"
#include "program/shopping/shopping_card.hpp"
#include "program/dialogue/settings.hpp"
#include "program/catalog/settings.hpp"
#include "program/catalog/features/catalog_stars.hpp"
#include "program/catalog/features/catalog_badges.hpp"
#include "program/catalog/features/catalog_variation_labels.hpp"
#include "program/pockets/features/pockets.hpp"
#include "program/pockets/settings.hpp"

namespace able_bag { bool Install(); }

namespace qol::core {
namespace {
InstallResult InstallCatalog() {
    using namespace catalog_aware_shops::features;
    const FeatureDescription features[]{
        {"catalog.profile", true, game::ValidateCatalogProfile},
        {"catalog.item_names", true, InstallCatalogStars, {"catalog.profile"}},
        {"catalog.shop_dialogue", true, InstallNookCatalogStars, {"catalog.profile"}},
        {"catalog.item_dialogue", true, InstallDialogueItemStars, {"catalog.profile"}},
        {"catalog.badges", true, InstallCatalogBadges, {"catalog.profile"}},
        {"catalog.variation_labels", true, InstallCatalogVariationLabels, {"catalog.profile"}},
    };
    return ActivateFeatures(features);
}
InstallResult InstallDialogue() {
    const FeatureDescription features[]{{"dialogue.speed", true, dialogue::Install}};
    return ActivateFeatures(features);
}
InstallResult InstallFitting() {
    const FeatureDescription features[]{{"fitting.bag", true, able_bag::Install}};
    return ActivateFeatures(features);
}
InstallResult InstallShopping() {
    const FeatureDescription features[]{{"shopping.card", true, shopping::Install}};
    return ActivateFeatures(features);
}

// Activation runs once after both optional Core settings layers are loaded.
// Copy only the configured value for the synchronous installer callback; the
// native adapter owns the installed scan hook's process-lifetime value.
unsigned RequestedScanTimeout{};
bool InstallConfiguredScanTimeout() { return game::InstallAmiiboTimeout(RequestedScanTimeout); }
} // namespace

FeatureResult ActivateCoreFeatures(const Settings &settings) {
    RequestedScanTimeout = settings.amiiboTimeoutFrames;
    // Zero leaves the game's initialization, countdown and expiry untouched.
    // The default is 9,000 updates; settings validate the unsigned 16-bit range.
    return ActivateFeature({"core.amiibo_timeout", RequestedScanTimeout != 0, InstallConfiguredScanTimeout});
}

// Minimum PACKAGE versions accepted by Core. Raise only when older assets
// stop being compatible; release versions still advance with the whole suite.
const std::array<ModuleDescription, 7> Modules{{
    {"Catalog", "catalog", {0, 1, 0}, InstallCatalog, catalog_aware_shops::Configure, nullptr, catalog_aware_shops::ReportSettings},
    {"Pockets", "pockets", {0, 1, 0}, qol_pockets::features::InstallPockets, qol_pockets::Configure, nullptr, qol_pockets::ReportSettings},
    {"Faster Dialogue", "faster-dialogue", {0, 1, 0}, InstallDialogue, dialogue::Configure, nullptr, dialogue::ReportSettings},
    {"Fitting Room", "fitting-room", {0, 1, 0}, InstallFitting, nullptr, PrepareFittingResources, nullptr, "resources.fitting"},
    {"Shopping Card", "shopping-card", {0, 3, 2}, InstallShopping, nullptr, shopping::PrepareResources, nullptr, "resources.shopping"},
    {"Crafting", "crafting", {0, 3, 46}, crafting::Install, crafting::Configure, nullptr, crafting::ReportSettings},
    {"Controls", "controls", {0, 4, 14}, controls::Install, controls::Configure, nullptr, controls::ReportSettings},
}};

} // namespace qol::core
