#include "program/core/hook_plan.hpp"
#include "catalog_badges.hpp"
#include "program/catalog/game/bindings.hpp"
#include "program/catalog/game/catalog_ownership.hpp"
#include "program/catalog/game/recipe_knowledge.hpp"
#include <array>
#include <cstring>

namespace catalog_aware_shops::features {
namespace {
namespace address {
constexpr std::uintptr_t NookItemReady = 0x02b6b3ec;
constexpr std::uintptr_t HhpUpdateRow = 0x02f99110;
constexpr std::uintptr_t MilesUpdateRow = 0x031253d0;
constexpr std::uintptr_t HotelUpdateRow = 0x02d60930;
constexpr std::uintptr_t FindPaneByName = 0x0027d1d0;
constexpr std::uintptr_t SetTextOnPane = 0x03c1b588;
constexpr std::uintptr_t SetTextOnGroup = 0x03c47b90;
} // namespace address
namespace layout {
constexpr std::size_t Player = 0x1f8, HhpPage = 0x220, HotelPage = 0x218;
constexpr std::size_t PageList = 0x578, RowCount = 0x208, Rows = 0x210;
constexpr std::size_t RowBindingCount = 0x2d8, RowNameBindings = 0x2e0;
constexpr std::size_t TextBindingAdapter = 0x48;
constexpr std::size_t AdapterPayload = 8, AdapterMethods = 0x18, SetTextMethod = 0x5e0;
constexpr std::size_t GroupNext = 8, GroupCount = 0x10, GroupNodeOffset = 0x14;
constexpr std::size_t MemberPane = 0x10, MemberMethods = 0x18;
constexpr std::size_t PaneParent = 0x18, PaneFlags = 0x58, PaneName = 0xb0;
constexpr std::size_t PaneSiblingNode = 8, PaneChildren = 0x20, ListNext = 8;
} // namespace layout
enum class CatalogSurface : unsigned { Nook, Hhp, Hotel, Miles };
constexpr std::array<const char *, 4> CatalogNames{"Nook", "HHP", "Hotel", "Miles"};
constexpr unsigned MaximumParentDepth = 12, MaximumPooledRows = 128;
constexpr std::array<const char *, 9> BadgeNames{
    "T_CatalogStar_00", "T_StarOutline_00", "T_StarOutline_01",
    "T_StarOutline_02", "T_StarOutline_03", "T_StarOutline_04",
    "T_StarOutline_05", "T_StarOutline_06", "T_StarOutline_07"};

template <class T> T Read(std::uintptr_t object, std::size_t offset = 0) {
    T value;
    std::memcpy(&value, reinterpret_cast<const void *>(object + offset), sizeof(value));
    return value;
}

// Text bindings use a typed adapter. Its payload is either a native TextBox
// or an intrusive list of (pane, method-table) pairs. Native 03c47b90 iterates
// that list and forwards SetText to each member; 03c1b588 handles one TextBox.
// Inspect the method identity before interpreting the payload. In particular,
// never read PaneName/PaneParent from the group header as 0.2.0 did.
std::uintptr_t ResolveNamePane(std::uintptr_t row, const char *&lookup, CatalogSurface catalog) {
    lookup = "no-row";
    if (!row) {
        return 0;
    }
    const auto bindings = Read<std::uintptr_t>(row, layout::RowNameBindings);
    // Miles' native name writer (03125aa4) selects slot 2 when the row has
    // more than two bindings, otherwise slot 0. Other catalogs use slot 0.
    // Using the first binding unconditionally can resolve an unrelated label
    // or an unbound adapter even when the item-name binding is present.
    const auto nameIndex = catalog == CatalogSurface::Miles &&
                                   Read<unsigned>(row, layout::RowBindingCount) > 2
                               ? 2u
                               : 0u;
    const auto nameBinding =
        bindings ? Read<std::uintptr_t>(bindings, nameIndex * sizeof(std::uintptr_t)) : 0;
    const auto adapter =
        nameBinding ? Read<std::uintptr_t>(nameBinding, layout::TextBindingAdapter) : 0;
    lookup = "no-name-adapter";
    if (!adapter) {
        return 0;
    }
    const auto payload = Read<std::uintptr_t>(adapter, layout::AdapterPayload);
    const auto methods = Read<std::uintptr_t>(adapter, layout::AdapterMethods);
    lookup = "unbound-name-adapter";
    if (!payload || !methods) {
        return 0;
    }
    const auto setText = Read<std::uintptr_t>(methods, layout::SetTextMethod);
    if (setText == game::MainAddress(address::SetTextOnPane)) {
        lookup = "direct-name";
        return payload;
    }
    lookup = "unsupported-name-adapter";
    if (setText != game::MainAddress(address::SetTextOnGroup)) {
        return 0;
    }
    const auto count = Read<unsigned>(payload, layout::GroupCount);
    const auto nodeOffset = Read<int>(payload, layout::GroupNodeOffset);
    lookup = "invalid-name-group";
    if (count > 16 || nodeOffset < 0 || nodeOffset > 0x80) {
        return 0;
    }
    auto node = Read<std::uintptr_t>(payload, layout::GroupNext);
    lookup = "empty-name-group";
    for (unsigned i = 0; i < count && node && node != payload; ++i) {
        const auto member = node - nodeOffset;
        const auto memberMethods = Read<std::uintptr_t>(member, layout::MemberMethods);
        const auto pane = Read<std::uintptr_t>(member, layout::MemberPane);
        if (pane && memberMethods &&
            Read<std::uintptr_t>(memberMethods, layout::SetTextMethod) ==
                game::MainAddress(address::SetTextOnPane)) {
            lookup = "group-name";
            return pane;
        }
        node = Read<std::uintptr_t>(node, layout::GroupNext);
    }
    return 0;
}

bool PaneHasName(std::uintptr_t pane, const char *name) {
    return std::strncmp(reinterpret_cast<const char *>(pane + layout::PaneName), name, 24) == 0;
}

// Unlike recursive native lookup, this never crosses a component boundary.
// The parent check and iteration limit also reject malformed child lists.
std::uintptr_t FindDirectChild(std::uintptr_t parent, const char *name) {
    const auto sentinel = parent + layout::PaneChildren;
    auto node = Read<std::uintptr_t>(sentinel, layout::ListNext);
    for (unsigned count = 0; node && node != sentinel && count < 128; ++count) {
        const auto child = node - layout::PaneSiblingNode;
        if (Read<std::uintptr_t>(child, layout::PaneParent) != parent) {
            return 0;
        }
        if (PaneHasName(child, name)) {
            return child;
        }
        node = Read<std::uintptr_t>(node, layout::ListNext);
    }
    return 0;
}

// Embedded BFLYT roots are renamed (live Nook: L_Btn_00, L_Btn_01, ...).
// Searching upward for "RootPane" reaches the entire catalog screen. Instead,
// recognize the row by its direct icon container and button hitbox, present in
// all four verified row layouts (Miles names its icon container N_Btn_02).
// Start the badge search inside that icon container, so native component lookup
// cannot skip the row or find a sibling.
std::uintptr_t FindRowIconContainer(std::uintptr_t row, const char *&lookup,
                                    CatalogSurface catalog) {
    auto pane = ResolveNamePane(row, lookup, catalog);
    if (!pane) {
        return 0;
    }
    const auto name = reinterpret_cast<const char *>(pane + layout::PaneName);
    if (std::memcmp(name, "T_Name_00", sizeof("T_Name_00")) &&
        std::memcmp(name, "T_Name_01", sizeof("T_Name_01"))) {
        lookup = "unexpected-name-pane";
        return 0;
    }
    for (unsigned depth = 0; pane && depth < MaximumParentDepth; ++depth) {
        const auto icon =
            FindDirectChild(pane, catalog == CatalogSurface::Miles ? "N_Btn_02" : "N_Icon_00");
        if (icon && FindDirectChild(pane, "B_Btn_00")) {
            return icon;
        }
        // A known row boundary with a missing icon must fail locally. Never
        // climb into the list and accidentally use another row's badge.
        const auto paneName = reinterpret_cast<const char *>(pane + layout::PaneName);
        if (!std::strncmp(paneName, "L_Btn_", 6) || PaneHasName(pane, "RootPane")) {
            break;
        }
        pane = Read<std::uintptr_t>(pane, layout::PaneParent);
    }
    lookup = "no-row-icon";
    return 0;
}

// One bounded snapshot per catalog when badge lookup fails. Walk the native
// child links directly, independently of FindPaneByName's virtual dispatch.
// 0027bb94 initializes the sentinel at +20; 0027d1d0/0027e6c0 walk next at +8
// and subtract 8 to recover a child Pane. Only internal pane names are logged.
// This diagnoses the loaded resource, not just the layout in our release ZIP.
void TracePaneTree(std::uintptr_t pane, unsigned depth, unsigned &remaining, const char *catalog) {
    if (!pane || !remaining || depth >= 16) {
        return;
    }
    --remaining;
    char name[25]{};
    std::memcpy(name, reinterpret_cast<const void *>(pane + layout::PaneName), 24);
    Logging.Debug("[QoL Enhancements/Catalog] CATALOG_TREE shop=%s depth=%u name=%s flags=%u\n", catalog,
                depth, name, Read<std::uint8_t>(pane, layout::PaneFlags));
    const auto sentinel = pane + layout::PaneChildren;
    auto node = Read<std::uintptr_t>(sentinel, layout::ListNext);
    while (remaining && node && node != sentinel) {
        const auto child = node - layout::PaneSiblingNode;
        if (Read<std::uintptr_t>(child, layout::PaneParent) != pane) {
            Logging.Debug("[QoL Enhancements/Catalog] CATALOG_TREE shop=%s stopped=parent-mismatch\n",
                        catalog);
            return;
        }
        if (depth + 1 >= 16) {
            Logging.Debug("[QoL Enhancements/Catalog] CATALOG_TREE shop=%s stopped=depth-limit\n", catalog);
            return;
        }
        TracePaneTree(child, depth + 1, remaining, catalog);
        node = Read<std::uintptr_t>(node, layout::ListNext);
    }
}

// No retained pointers or ownership cache: recycled rows and newly registered
// purchases are evaluated every time the game rebuilds the row. Resolve all
// layers first; a partial/missing layout must not leave an isolated outline.
void UpdateRowBadge(std::uintptr_t row, std::uint8_t player, const game::CatalogItem &item,
                    CatalogSurface catalog) {
    const char *lookup = "not-started";
    const auto iconContainer = FindRowIconContainer(row, lookup, catalog);
    std::array<std::uintptr_t, BadgeNames.size()> panes{};
    unsigned found = 0;
    if (iconContainer) {
        const auto findPane =
            game::FunctionAt<std::uintptr_t (*)(std::uintptr_t, const char *, bool)>(
                address::FindPaneByName);
        for (unsigned i = 0; i < panes.size(); ++i) {
            panes[i] = findPane(iconContainer, BadgeNames[i], true);
            found += panes[i] != 0;
        }
    }
    // Diagnose ownership separately from pane discovery. Earlier releases
    // reported Unavailable for every missing layout, hiding the actual result.
    const bool isMiles = catalog == CatalogSurface::Miles;
    const bool isRecipe = isMiles && item.id == game::RecipeCardId;
    const auto recipeStatus =
        isRecipe ? game::QueryRecipeCard(player, item) : game::RecipeStatus::Unavailable;
    const auto status = isRecipe  ? game::CatalogStatus::Unavailable
                        : isMiles ? game::QueryMilesCatalog(player, item)
                                  : game::QueryCabinetCatalog(player, item);
    // Row icons represent the item family. A missing body variant alone does
    // not earn a filled star; catalog_variation_labels marks that color with ☆.
    const bool visible =
        found == panes.size() && (isRecipe ? recipeStatus == game::RecipeStatus::Unlearned
                                           : status == game::CatalogStatus::Uncataloged);
    for (const auto pane : panes) {
        if (!pane) {
            continue;
        }
        auto *flags = reinterpret_cast<std::uint8_t *>(pane + layout::PaneFlags);
        *flags = (*flags & ~1u) | static_cast<unsigned>(visible);
    }
    static std::array<unsigned, CatalogNames.size()> reports{};
    static std::array<bool, CatalogNames.size()> traced{};
    const auto catalogIndex = static_cast<unsigned>(catalog);
    if (iconContainer && found != panes.size() && !traced[catalogIndex]) {
        traced[catalogIndex] = true;
        unsigned remaining = 128;
        TracePaneTree(iconContainer, 0, remaining, CatalogNames[catalogIndex]);
        Logging.Debug("[QoL Enhancements/Catalog] CATALOG_TREE_END shop=%s visited=%u limit=%d\n",
                    CatalogNames[catalogIndex], 128 - remaining, remaining == 0);
    }
    if (reports[catalogIndex] < 48) {
        ++reports[catalogIndex];
        Logging.Debug("[QoL Enhancements/Catalog] CATALOG_BADGE shop=%s item=%u player=%u status=%u "
                    "panes=%u visible=%d lookup=%s recipe_id=%u recipe_status=%u\n",
                    CatalogNames[catalogIndex], item.id, player, static_cast<unsigned>(status),
                    found, visible, lookup,
                    isRecipe ? static_cast<unsigned>(static_cast<std::uint16_t>(item.extra)) : 0,
                    static_cast<unsigned>(recipeStatus));
    }
}

std::uintptr_t CatalogRow(std::uintptr_t controller, std::size_t pageOffset, int index,
                          std::size_t listOffset = layout::PageList) {
    if (!controller || index < 0) {
        return 0;
    }
    const auto page = Read<std::uintptr_t>(controller, pageOffset);
    const auto list = page ? Read<std::uintptr_t>(page, listOffset) : 0;
    if (!list) {
        return 0;
    }
    const auto count = Read<unsigned>(list, layout::RowCount);
    if (count > MaximumPooledRows || static_cast<unsigned>(index) >= count) {
        return 0;
    }
    const auto rows = Read<std::uintptr_t>(list, layout::Rows);
    return rows ? Read<std::uintptr_t>(rows, index * sizeof(std::uintptr_t)) : 0;
}

// Nook updates several pooled rows in one loop. This site precedes its empty
// item check: x28 = controller, x23 = row, x11 = complete 8-byte item. The native
// ldrh/cmp after this site establish flags, so the callback need not preserve NZCV.
HOOK_DEFINE_INLINE(NookCatalogBadge) {
    static void Callback(exl::hook::InlineCtx *registers) {
        if (!registers->X[28] || !registers->X[11]) {
            return;
        }
        UpdateRowBadge(registers->X[23], Read<std::uint8_t>(registers->X[28], layout::Player),
                       Read<game::CatalogItem>(registers->X[11]), CatalogSurface::Nook);
    }
};

// HHP and Hotel receive (controller, item, pooled row index). Let native text,
// icon and row state updates finish before toggling our independent panes.
HOOK_DEFINE_TRAMPOLINE(HhpCatalogBadge) {
    static void Callback(std::uintptr_t controller, const game::CatalogItem *item, int index) {
        Orig(controller, item, index);
        if (controller && item) {
            UpdateRowBadge(CatalogRow(controller, layout::HhpPage, index),
                           Read<std::uint8_t>(controller, layout::Player), *item,
                           CatalogSurface::Hhp);
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(HotelCatalogBadge) {
    static void Callback(std::uintptr_t controller, const game::CatalogItem *item, int index) {
        Orig(controller, item, index);
        if (controller && item) {
            UpdateRowBadge(CatalogRow(controller, layout::HotelPage, index),
                           Read<std::uint8_t>(controller, layout::Player), *item,
                           CatalogSurface::Hotel);
        }
    }
};
// Nook Miles uses the same row ABI but a different list field. Run after
// native rendering, including empty rows, so recycled badges always reset.
HOOK_DEFINE_TRAMPOLINE(MilesCatalogBadge) {
    static void Callback(std::uintptr_t controller, const game::CatalogItem *item, int index) {
        Orig(controller, item, index);
        constexpr std::size_t MilesPage = 0x210, MilesList = 0x588;
        if (controller && item) {
            UpdateRowBadge(CatalogRow(controller, MilesPage, index, MilesList),
                           Read<std::uint8_t>(controller, layout::Player), *item,
                           CatalogSurface::Miles);
        }
    }
};
} // namespace

bool InstallCatalogBadges() {
    struct Check {
        std::uintptr_t offset;
        std::array<std::uint32_t, 5> words;
    };
    constexpr Check checks[]{
        {0x03125aa4, {0xb942da68, 0x7100091f, 0xf9417269, 0x1a9f97e8, 0xd37c7d08}},
        {0x031253d0, {0xfc190fe8, 0xa9017bfd, 0x910043fd, 0xa9026ffc, 0xa90367fa}},
        {0x03125748, {0xf90207e8, 0xf90203f7, 0x941aaec8, 0xf9410a88, 0xf942c508}},
        // Exact words from the local, hash-verified ACNH 3.0.3 executable.
        {0x03c47b90, {0xf9400408, 0xeb00011f, 0x54000380, 0xa9bc7bfd, 0xa9015ff8}},
        {0x03c47bd0, {0xa9412300, 0xaa1403e1, 0x2a1303e2, 0xf942f108, 0xd63f0100}},
        {0x03c1b588, {0xa9be7bfd, 0xf9000bf3, 0x910003fd, 0xf9400008, 0xaa0003f3}},
        {0x03c18314, {0xd503201f, 0x1017c3c8, 0xf902f268, 0xd503201f, 0x1017c768}},
        {0x02b6b3ec, {0x79400168, 0x529fffcc, 0x6b0c011f, 0x39400969, 0x39400d6a}},
        {0x02f99110, {0x6db923e9, 0xa9017bfd, 0x910043fd, 0xa9026ffc, 0xa90367fa}},
        {0x02d60930, {0xfc190fe8, 0xa9017bfd, 0x910043fd, 0xa9026ffc, 0xa90367fa}},
        {0x0027d1d0, {0xa9bd7bfd, 0xf9000bf5, 0xa9024ff4, 0x910003fd, 0xaa0003f4}},
        {0x0027bb94, {0xf001ec28, 0x91008009, 0x3dc08100, 0xa9022409, 0xf001ec29}},
        {0x02f99494, {0xf9411288, 0xf942bd08, 0xb4003908, 0xaa1f03f4, 0x37f800f5}},
        {0x02d60cb4, {0xf9410e88, 0xf942bd08, 0xb4003348, 0xaa1f03f5, 0x37f800f6}},
        {0x02b6b3e0, {0xf940dbe9, 0xf9400529, 0xf868592b, 0x79400168, 0x529fffcc}},
    };
    for (const auto &check : checks) {
        if (std::memcmp(reinterpret_cast<const void *>(game::MainAddress(check.offset)),
                        check.words.data(), sizeof(check.words))) {
            Logging.Error("[QoL Enhancements/Catalog] CATALOG_BADGES_DISABLED fingerprint mismatch\n");
            return false;
        }
    }
    if (!game::ValidateCatalogBindings() || !game::ValidateRecipeBindings()) {
        return false;
    }

    qol::core::HookPlan hookPlan{
        qol::core::Site<MilesCatalogBadge>("MilesCatalogBadge", address::MilesUpdateRow, {0xfc190fe8, 0xa9017bfd, 0x910043fd, 0xa9026ffc, 0xa90367fa}, true),
        qol::core::Site<NookCatalogBadge>("NookCatalogBadge", address::NookItemReady, {0x79400168, 0x529fffcc, 0x6b0c011f, 0x39400969, 0x39400d6a}, true),
        qol::core::Site<HhpCatalogBadge>("HhpCatalogBadge", address::HhpUpdateRow, {0x6db923e9, 0xa9017bfd, 0x910043fd, 0xa9026ffc, 0xa90367fa}, true),
        qol::core::Site<HotelCatalogBadge>("HotelCatalogBadge", address::HotelUpdateRow, {0xfc190fe8, 0xa9017bfd, 0x910043fd, 0xa9026ffc, 0xa90367fa}, true),
    };
    if (!hookPlan) return false;
    if (!hookPlan.Publish<MilesCatalogBadge>(address::MilesUpdateRow)) return false;
    if (!hookPlan.Publish<NookCatalogBadge>(address::NookItemReady)) return false;
    if (!hookPlan.Publish<HhpCatalogBadge>(address::HhpUpdateRow)) return false;
    if (!hookPlan.Publish<HotelCatalogBadge>(address::HotelUpdateRow)) return false;
    Logging.Debug("[QoL Enhancements/Catalog] CATALOG_BADGES_READY Nook/HHP/Hotel/Miles item-family and "
                "recipe stars\n");
    return true;
}
} // namespace catalog_aware_shops::features
