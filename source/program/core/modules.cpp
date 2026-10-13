#include "native/runtime.hpp"
#include "lib.hpp"
#include "modules.hpp"
#include "release.hpp"
#include "program/controls/settings.hpp"
#include "resource_budget.hpp"
#include "settings.hpp"
#include "program/crafting/settings.hpp"
#include "program/shopping/shopping_card.hpp"
#include "program/dialogue/settings.hpp"
#include "program/catalog/game/build_profile.hpp"
#include "program/catalog/settings.hpp"
#include "program/catalog/features/catalog_stars.hpp"
#include "program/catalog/features/catalog_badges.hpp"
#include "program/catalog/features/catalog_variation_labels.hpp"
#include "program/pockets/game/build_profile.hpp"
#include "program/pockets/settings.hpp"
#include "program/pockets/features/stack_splitting.hpp"
#include "program/pockets/features/storage_withdrawal.hpp"
#include "program/pockets/features/recycling_claim.hpp"
#include "program/pockets/features/quick_store.hpp"
#include "program/pockets/features/hold_select.hpp"
#include "program/pockets/features/storage_hint.hpp"
#include <nn/fs/fs_files.hpp>
#include <atomic>
#include <cstdio>

namespace able_bag { bool Install(); }

namespace qol::core {
namespace {
bool ValidateCatalog() {
    const auto result = catalog_aware_shops::game::ValidateExpectedCode(native::MainExecutable());
    if (!result) native::ReportMismatch(result);
    return bool(result);
}
InstallResult InstallCatalog() {
    using namespace catalog_aware_shops::features;
    const FeatureDescription features[]{
        {"catalog.profile", true, ValidateCatalog},
        {"catalog.item_names", true, InstallCatalogStars, {"catalog.profile"}},
        {"catalog.shop_dialogue", true, InstallNookCatalogStars, {"catalog.profile"}},
        {"catalog.item_dialogue", true, InstallDialogueItemStars, {"catalog.profile"}},
        {"catalog.badges", true, InstallCatalogBadges, {"catalog.profile"}},
        {"catalog.variation_labels", true, InstallCatalogVariationLabels, {"catalog.profile"}},
    };
    return ActivateFeatures(features);
}
bool ValidatePockets() {
    const auto result = qol_pockets::game::ValidateExpectedCode(native::MainExecutable());
    if (!result) native::ReportMismatch(result);
    return bool(result);
}
InstallResult InstallPockets() {
    using namespace qol_pockets::features;
    const FeatureDescription features[]{
        {"pockets.profile", true, ValidatePockets},
        {"pockets.stacks", true, InstallStackSplitting, {"pockets.profile"}},
        {"pockets.storage_withdrawal", true, InstallStorageWithdrawal, {"pockets.profile"}},
        {"pockets.quick_store", true, InstallQuickStore, {"pockets.profile"}},
        {"pockets.hold_select", qol_pockets::GetSettings().enableMultiselect, InstallHoldSelect, {"pockets.quick_store"}},
        {"pockets.recycling", true, InstallRecyclingClaim, {"pockets.profile"}},
        {"pockets.hints", true, InstallStorageHint, {"pockets.quick_store"}},
        {"pockets.withdrawal_hint", true, InstallStorageWithdrawalHint, {"pockets.storage_withdrawal", "pockets.hints"}},
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

enum class LoadResult { Absent, Invalid, Ready };
// A null metadata pointer is used only for Core's optional settings files.
// Feature modules still require their regular enabled/version marker.
LoadResult LoadConfig(const ModuleDescription &module, Config *metadata, ConfigLayer layer) {
    const bool custom = layer == ConfigLayer::Custom;
    char path[160];
    std::snprintf(path, sizeof(path), "content:/qol-enhancements/config/%s%s.ini", module.slug,
                  custom ? ".custom" : "");
    nn::fs::FileHandle file{};
    const auto result = nn::fs::OpenFile(&file, path, nn::fs::OpenMode_Read);
    if (result != 0) {
        // Horizon FS PathNotFound (module 2, description 1).
        if (result == 0x202) return LoadResult::Absent;
        if (custom) Logging.Warning("[QoL Enhancements] WARNING: %s custom config open failed: 0x%x\n", module.name, result);
        else Logging.Error("[QoL Enhancements] ERROR: %s config open failed: 0x%x\n", module.name, result);
        return LoadResult::Invalid;
    }
    long size = 0;
    char buffer[MaxConfigBytes];
    bool valid = nn::fs::GetFileSize(&size, file) == 0 && size >= 0 &&
                 static_cast<unsigned long>(size) <= sizeof(buffer);
    if (valid && size) valid = nn::fs::ReadFile(file, 0, buffer, static_cast<unsigned long>(size)) == 0;
    nn::fs::CloseFile(file);
    const std::string_view input{buffer, valid ? static_cast<std::size_t>(size) : 0};
    if (valid) valid = ValidUtf8(input, true);
    if (valid && metadata) valid = ParseConfig(input, *metadata, layer, module.configure != nullptr);
    if (valid && module.configure) {
        const auto warnings = module.configure(input);
        if (warnings) Logging.Warning("[QoL Enhancements] WARNING: %s %sconfig issues=%u; invalid settings retain prior values\n",
                                  module.name, custom ? "custom " : "", warnings);
    }
    return valid ? LoadResult::Ready : LoadResult::Invalid;
}
std::atomic<bool> ActivationStarted{false};
Settings CorePreferences{};
unsigned ConfigureCore(std::string_view input) { return ApplyCoreSettings(input, CorePreferences); }
bool InstallCoreTimeout() { return InstallAmiiboTimeout(CorePreferences.amiiboTimeoutFrames); }
bool ActivateCoreSettings() {
    const ModuleDescription core{"Core", "core", {}, nullptr, ConfigureCore};
    for (const auto layer : {ConfigLayer::Module, ConfigLayer::Custom}) {
        if (LoadConfig(core, nullptr, layer) == LoadResult::Invalid)
            Logging.Warning("[QoL Enhancements/Core] WARNING: %s config invalid/unreadable; retaining prior settings\n",
                        layer == ConfigLayer::Custom ? "custom" : "regular");
    }
    Logging.Configure(CorePreferences.logging);
    Logging.Version("[QoL Enhancements] Core: %s; slot=subsdk6; configuration ready\n", ReleaseVersion);
    Logging.Settings("[QoL Enhancements/Core] settings: amiibo_timeout_frames=%u log_versions=%s log_settings=%s log_debug=%s\n",
                     CorePreferences.amiiboTimeoutFrames, CorePreferences.logging.versions ? "true" : "false",
                     CorePreferences.logging.settings ? "true" : "false", CorePreferences.logging.debug ? "true" : "false");
    return ActivateFeature({"core.amiibo_timeout", CorePreferences.amiiboTimeoutFrames != 0, InstallCoreTimeout}).Complete();
}
} // namespace

// Minimum PACKAGE versions accepted by Core. Raise only when older assets
// stop being compatible; release versions still advance with the whole suite.
const std::array<ModuleDescription, 7> Modules{{
    {"Catalog", "catalog", {0, 1, 0}, InstallCatalog, catalog_aware_shops::Configure, nullptr, catalog_aware_shops::ReportSettings},
    {"Pockets", "pockets", {0, 1, 0}, InstallPockets, qol_pockets::Configure, nullptr, qol_pockets::ReportSettings},
    {"Faster Dialogue", "faster-dialogue", {0, 1, 0}, InstallDialogue, dialogue::Configure, nullptr, dialogue::ReportSettings},
    {"Fitting Room", "fitting-room", {0, 1, 0}, InstallFitting, nullptr, PrepareFittingResources},
    {"Shopping Card", "shopping-card", {0, 3, 2}, InstallShopping, nullptr, shopping::PrepareResources},
    {"Crafting", "crafting", {0, 3, 46}, crafting::Install, crafting::Configure, nullptr, crafting::ReportSettings},
    {"Controls", "controls", {0, 4, 14}, controls::Install, controls::Configure, nullptr, controls::ReportSettings},
}};

void ActivateModules() {
    if (ActivationStarted.exchange(true)) return;
    unsigned installed = 0, errors = !ActivateCoreSettings();
    for (const auto &module : Modules) {
        Config config{};
        const auto loaded = LoadConfig(module, &config, ConfigLayer::Module);
        if (loaded == LoadResult::Absent) {
            ActivationResults.Store(module.slug, {});
            Logging.Debug("[QoL Enhancements] %s: not installed\n", module.name);
            continue;
        }
        const auto beforeResourceMutation = MutationCount();
        const char *resourceId = module.prepareResources == PrepareFittingResources ? "resources.fitting" : "resources.shopping";
        const auto resourceResult = module.prepareResources
            ? ActivateFeature({resourceId, true, module.prepareResources}) : FeatureResult{};
        const bool resourcesReady = resourceResult.Complete();
        const bool resourcesPublished = MutationCount() != beforeResourceMutation;
        ActivationResults.Store(module.slug, {FeatureState::Disabled, resourcesPublished, "package resources only"});
        if (!resourcesReady) {
            ActivationResults.Store(module.slug, {FeatureState::DependencyUnavailable, resourcesPublished, "resource preparation unavailable"});
            Logging.Error("[QoL Enhancements] ERROR: %s resource support unavailable; feature code disabled\n",
                        module.name);
            ++errors;
            continue;
        }
        if (loaded == LoadResult::Invalid) {
            ActivationResults.Store(module.slug, {FeatureState::Failed, resourcesPublished, "invalid package configuration"});
            Logging.Error("[QoL Enhancements] ERROR: %s config invalid/unreadable; feature code disabled\n", module.name);
            ++errors;
            continue;
        }
        // All installed modules use the same overlay path and order, including
        // disabled modules: a personal enabled=true can re-enable their code.
        // An absent/invalid marker never reaches this optional read.
        if (LoadConfig(module, &config, ConfigLayer::Custom) == LoadResult::Invalid)
            Logging.Warning("[QoL Enhancements] WARNING: %s custom config invalid/unreadable; retaining regular config\n",
                        module.name);
        Logging.Version("[QoL Enhancements] %s: %u.%u.%u; enabled=%s; minimum=%u.%u.%u\n",
                    module.name, config.version.major, config.version.minor, config.version.patch,
                    config.enabled ? "true" : "false", module.minimum.major, module.minimum.minor,
                    module.minimum.patch);
        Logging.Settings("[QoL Enhancements/%s] settings: enabled=%s\n", module.name, config.enabled ? "true" : "false");
        if (module.reportSettings) module.reportSettings();
        if (!config.enabled) {
            Logging.Debug("[QoL Enhancements] module=%s state=Disabled published=%s resource_support=%s\n", module.slug, resourcesPublished ? "true" : "false", module.prepareResources && resourcesReady ? "true" : "false");
            continue;
        }
        const auto compatibility = CheckCompatibility(config.version, module.minimum, CoreVersion);
        if (compatibility == Compatibility::TooOld) {
            ActivationResults.Store(module.slug, {FeatureState::Unsupported, resourcesPublished, "package below minimum"});
            Logging.Error("[QoL Enhancements] ERROR: %s package is below minimum; feature code disabled. "
                        "Update the suite or disable the module in Eden and restart.\n", module.name);
            ++errors;
            continue;
        }
        if (compatibility == Compatibility::NewerThanCore) {
            Logging.Warning("[QoL Enhancements] WARNING: %s package is newer than Core %s. "
                        "Update Core to the same suite release.\n", module.name, ReleaseVersion);
        }
        const auto activation = module.install();
        const bool complete = activation.complete;
        ActivationResults.Store(module.slug, {complete ? FeatureState::Ready : FeatureState::Failed,
            resourcesPublished || activation.published, complete ? "features ready" : "one or more features unavailable"});
        if (complete) Logging.Debug("[QoL Enhancements] %s: ENABLED\n", module.name);
        else Logging.Error("[QoL Enhancements] %s: ERROR installation failed or partial; published=%s ready_features=%u unavailable_features=%u; inspect feature results\n", module.name, resourcesPublished || activation.published ? "true" : "false", activation.ready, activation.unavailable);
        installed += complete;
        errors += !complete;
    }
    if (errors) Logging.Error("[QoL Enhancements] BOOT_PARTIAL %s; enabled_modules=%u errors=%u\n", ReleaseVersion, installed, errors);
    else Logging.Debug("[QoL Enhancements] BOOT_OK %s; enabled_modules=%u errors=%u\n", ReleaseVersion, installed, errors);
}
} // namespace qol::core
