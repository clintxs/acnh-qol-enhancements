#include "lib.hpp"
#include "modules.hpp"
#include "release.hpp"
#include "program/dialogue/settings.hpp"
#include "program/catalog/game/build_profile.hpp"
#include "program/catalog/features/catalog_stars.hpp"
#include "program/catalog/features/catalog_badges.hpp"
#include "program/catalog/features/catalog_variation_labels.hpp"
#include "program/pockets/game/build_profile.hpp"
#include "program/pockets/features/stack_splitting.hpp"
#include "program/pockets/features/storage_withdrawal.hpp"
#include "program/pockets/features/quick_store.hpp"
#include "program/pockets/features/storage_hint.hpp"
#include <nn/fs/fs_files.hpp>
#include <atomic>
#include <cstdio>

namespace able_bag { bool Install(); }
namespace qol::amiibo { bool Install(); }

namespace qol::core {
namespace {
bool InstallCatalog() {
    const auto &main = exl::util::GetMainModuleInfo();
    if (!catalog_aware_shops::game::MatchesExpectedCode(
            reinterpret_cast<const std::byte *>(main.m_Text.m_Start), main.m_Text.m_Size)) return false;
    using namespace catalog_aware_shops::features;
    const bool stars = InstallCatalogStars();
    const bool shopDialogue = InstallNookCatalogStars();
    const bool generalDialogue = InstallDialogueItemStars();
    const bool badges = InstallCatalogBadges();
    const bool variations = InstallCatalogVariationLabels();
    return stars && shopDialogue && generalDialogue && badges && variations;
}
bool InstallPockets() {
    const auto &main = exl::util::GetMainModuleInfo();
    if (!qol_pockets::game::MatchesExpectedCode(
            reinterpret_cast<const std::byte *>(main.m_Text.m_Start), main.m_Text.m_Size)) return false;
    using namespace qol_pockets::features;
    const bool stacks = InstallStackSplitting();
    const bool storage = InstallStorageWithdrawal();
    const bool quickStore = InstallQuickStore();
    const bool hint = quickStore && InstallStorageHint();
    const bool withdrawalHint = storage && hint && InstallStorageWithdrawalHint();
    return stacks && storage && quickStore && hint && withdrawalHint;
}

enum class LoadResult { Absent, Invalid, Ready };
LoadResult LoadConfig(const ModuleDescription &module, Config &config) {
    char path[160];
    std::snprintf(path, sizeof(path), "content:/qol-enhancements/config/%s.ini", module.slug);
    nn::fs::FileHandle file{};
    const auto result = nn::fs::OpenFile(&file, path, nn::fs::OpenMode_Read);
    if (result != 0) {
        // Horizon FS PathNotFound (module 2, description 1).
        if (result == 0x202) return LoadResult::Absent;
        Logging.Log("[QoL Enhancements] ERROR: %s config open failed: 0x%x\n", module.name, result);
        return LoadResult::Invalid;
    }
    long size = 0;
    char buffer[MaxConfigBytes];
    bool valid = nn::fs::GetFileSize(&size, file) == 0 && size > 0 &&
                 static_cast<unsigned long>(size) <= sizeof(buffer);
    if (valid) valid = nn::fs::ReadFile(file, 0, buffer, static_cast<unsigned long>(size)) == 0;
    nn::fs::CloseFile(file);
    if (valid) valid = ParseConfig({buffer, static_cast<std::size_t>(size)}, config);
    return valid ? LoadResult::Ready : LoadResult::Invalid;
}
std::atomic<bool> ActivationStarted{false};
} // namespace

// Minimum PACKAGE versions accepted by Core. Raise only when older assets
// stop being compatible; release versions still advance with the whole suite.
const std::array<ModuleDescription, 5> Modules{{
    {"Catalog", "catalog", {0, 1, 0}, InstallCatalog},
    {"Pockets", "pockets", {0, 1, 0}, InstallPockets},
    {"Faster Dialogue", "faster-dialogue", {0, 1, 0}, dialogue::Install},
    {"Fitting Room", "fitting-room", {0, 1, 0}, able_bag::Install},
    {"Amiibo", "amiibo", {0, 1, 0}, amiibo::Install},
}};

void ActivateModules() {
    if (ActivationStarted.exchange(true)) return;
    Logging.Log("[QoL Enhancements] Core: %s; slot=subsdk6; configuration ready\n", ReleaseVersion);
    unsigned installed = 0, errors = 0;
    for (const auto &module : Modules) {
        Config config{};
        const auto loaded = LoadConfig(module, config);
        if (loaded == LoadResult::Absent) {
            Logging.Log("[QoL Enhancements] %s: not installed\n", module.name);
            continue;
        }
        if (loaded == LoadResult::Invalid) {
            Logging.Log("[QoL Enhancements] ERROR: %s config invalid/unreadable; feature code disabled\n", module.name);
            ++errors;
            continue;
        }
        Logging.Log("[QoL Enhancements] %s: %u.%u.%u; enabled=%s; minimum=%u.%u.%u\n",
                    module.name, config.version.major, config.version.minor, config.version.patch,
                    config.enabled ? "true" : "false", module.minimum.major, module.minimum.minor,
                    module.minimum.patch);
        if (!config.enabled) continue;
        const auto compatibility = CheckCompatibility(config.version, module.minimum, CoreVersion);
        if (compatibility == Compatibility::TooOld) {
            Logging.Log("[QoL Enhancements] ERROR: %s package is below minimum; feature code disabled. "
                        "Update the suite or disable the module in Eden and restart.\n", module.name);
            ++errors;
            continue;
        }
        if (compatibility == Compatibility::NewerThanCore) {
            Logging.Log("[QoL Enhancements] WARNING: %s package is newer than Core %s. "
                        "Update Core to the same suite release.\n", module.name, ReleaseVersion);
        }
        const bool complete = module.install();
        Logging.Log("[QoL Enhancements] %s: %s\n", module.name,
                    complete ? "ENABLED" : "ERROR installation failed or partial; inspect feature logs");
        installed += complete;
        errors += !complete;
    }
    Logging.Log("[QoL Enhancements] %s %s; enabled_modules=%u errors=%u\n",
                errors ? "BOOT_PARTIAL" : "BOOT_OK", ReleaseVersion, installed, errors);
}
} // namespace qol::core
