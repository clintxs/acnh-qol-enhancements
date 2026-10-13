#include "program/catalog/features/catalog_features.hpp"
#include "program/core/hook_plan.hpp"
#include "program/catalog/features/catalog_stars.hpp"
#include "program/catalog/features/catalog_star_text.hpp"
#include "program/catalog/game/bindings.hpp"
#include "program/catalog/game/catalog_ownership.hpp"
#include "program/catalog/game/recipe_knowledge.hpp"
#include <array>
#include <cstring>

namespace catalog_aware_shops::features {
namespace {
namespace layout {
constexpr std::size_t ActionEvent = 8, EventPlayer = 0x2a8, ActionItemTarget = 0xd8;
constexpr unsigned VillagerCurrentCraftTarget = 16;
constexpr std::size_t NameType = 8, Text = 0x18, Capacity = 0x20;
constexpr std::size_t StorageStart = 0x28, StorageEnd = 0x1f0, PluralForm = 0x1e0;
constexpr unsigned CapacityUnits = 128;
// Verified native frame sizes: FP points to the saved frame/link registers.
constexpr std::size_t ValueFrameBytes = 0x1b50, UiResultFrameBytes = 0x1860;
constexpr std::size_t ScriptFrameBytes = 0x1bd0;
} // namespace layout

template <class T> T Read(std::uintptr_t object, std::size_t offset = 0) {
    T value;
    std::memcpy(&value, reinterpret_cast<const void *>(object + offset), sizeof(value));
    return value;
}

bool MarkForm(std::uintptr_t name, std::size_t offset, char16_t marker) {
    const auto form = name + offset;
    const auto text = Read<std::uintptr_t>(form, layout::Text);
    const auto capacity = Read<unsigned>(form, layout::Capacity);
    if (capacity != layout::CapacityUnits || text < form + layout::StorageStart ||
        text > form + layout::StorageEnd - layout::CapacityUnits * sizeof(char16_t)) {
        return false;
    }
    // Dialogue nouns commonly follow an article. Preserve native case and noun
    // metadata; only balloons force capitalization. Already starred forms are
    // ignored by PrefixCatalogStar, including repeated use of the same name.
    return PrefixCatalogStar(
        reinterpret_cast<char16_t *>(text), capacity, [](char16_t *) {}, marker);
}

void MarkDialogueItem(exl::hook::InlineCtx *registers, std::uintptr_t name,
                      std::uintptr_t itemAddress, const char *source, bool villagerCraft = false,
                      bool creatureOnly = false) {
    const auto action = registers->X[20];
    if (!action || !name || !itemAddress) {
        return;
    }
    const auto event = Read<std::uintptr_t>(action, layout::ActionEvent);
    if (!event) {
        return;
    }
    const auto player = Read<std::uint8_t>(event, layout::EventPlayer);
    if (player >= 8) {
        return;
    }
    const auto item = Read<game::CatalogItem>(itemAddress);
    const auto decision = DecideItemMarker(player, item,
        villagerCraft ? ItemUse::VillagerCraft : creatureOnly ? ItemUse::CatchDialogue : ItemUse::ItemName,
        Surface::Cabinet, event);
    const auto recipeId = decision.recipeId;
    const auto recipeStatus = decision.recipe;
    const auto status = decision.catalog;
    const auto marker = decision.marker;
    const auto type = Read<unsigned>(name, layout::NameType);
    bool marked = false;
    if (marker && type <= 2) {
        const bool singular = MarkForm(name, 0, marker);
        const bool plural = type ? MarkForm(name, layout::PluralForm, marker) : false;
        marked = type == 2 ? plural : singular;
    }
    static unsigned reports = 0;
    if (reports++ < 96) {
        Logging.Debug("[QoL Enhancements/Catalog] DIALOGUE_ITEM_STAR source=%s item=%u player=%u "
                    "status=%u recipe_status=%u recipe=%u villager_craft=%d marker=0x%x marked=%d type=%u\n",
                    source, item.id, player, static_cast<unsigned>(status),
                    static_cast<unsigned>(recipeStatus), recipeId, villagerCraft, static_cast<unsigned>(marker), marked,
                    type);
    }
}

// Shared event-flow tag setters, independent of bank/label or selected shop.
// At each site x20 is the action and the complete source item is still alive.
// Mark the native temporary after construction and before the game's owned
// substitution copy. No cached pointers, guessed name-to-item mapping, slot-0
// overwrite, or edits to message resources are involved.
HOOK_DEFINE_INLINE(DialogueValueItemStar) {
    static void Callback(exl::hook::InlineCtx *registers) {
        const auto frame = registers->X[29];
        if (!frame) {
            return;
        }
        const auto stack = frame - layout::ValueFrameBytes;
        MarkDialogueItem(registers, stack + 0x400, stack + 0x28, "value");
    }
};
// SetTagFromSystem's cChachFish/Insect/Seafood arms preserve x20 (action)
// and x21 (the complete catch item). They share the value setter's name frame.
HOOK_DEFINE_INLINE(DialogueCatchCreatureStar) {
    static void Callback(exl::hook::InlineCtx *registers) {
        const auto frame = registers->X[29];
        if (frame && registers->X[21]) {
            MarkDialogueItem(registers, frame - layout::ValueFrameBytes + 0x400,
                             registers->X[21], "catch-creature", false, true);
        }
    }
};
HOOK_DEFINE_INLINE(DialogueStoredItemStar) {
    static void Callback(exl::hook::InlineCtx *registers) {
        const auto frame = registers->X[29];
        if (!frame) {
            return;
        }
        MarkDialogueItem(registers, frame - layout::ValueFrameBytes + 0x400, frame - 0xb0,
                         "stored-item");
    }
};
HOOK_DEFINE_INLINE(DialogueInventoryItemStar) {
    static void Callback(exl::hook::InlineCtx *registers) {
        const auto frame = registers->X[29];
        if (!frame) {
            return;
        }
        MarkDialogueItem(registers, frame - layout::ValueFrameBytes + 0x400, frame - 0x10,
                         "inventory-item");
    }
};
// SetTagFromUIResult saves its resolved tag index at FP-4. These are the only
// verified branches reaching the shared constructor at 00c5e134. Its x1 comes
// from three different places; SP+8 is not a universal item buffer.
std::uintptr_t UiResultSourceItem(exl::hook::InlineCtx *registers) {
    const auto frame = registers->X[29];
    const auto tag = Read<unsigned>(frame - 4);
    switch (tag) {
    case 4:  // cItemName
    case 23: // cBridgeSlopeName
    case 49: // cWherearenCafeMenuName
        // 00c5ddac passes x21; the constructor preserves this register.
        return registers->X[21];
    case 19: // cItemNameFromRecipe (DIY and cooking)
        // 00c5e0cc copies the crafted output to FP-68, then builds its name.
        return frame - 0x68;
    case 21: // cBromideNpcName
    case 24: // cBridgeSlopeKitName
        return frame - layout::UiResultFrameBytes + 8;
    default:
        return 0; // Never interpret unrelated scratch storage as an item.
    }
}
HOOK_DEFINE_INLINE(DialogueUiResultItemStar) {
    static void Callback(exl::hook::InlineCtx *registers) {
        const auto frame = registers->X[29];
        if (!frame) {
            return;
        }
        const auto stack = frame - layout::UiResultFrameBytes;
        MarkDialogueItem(registers, stack + 0x158, UiResultSourceItem(registers), "ui-result");
    }
};
HOOK_DEFINE_INLINE(DialogueScriptItemStar) {
    static void Callback(exl::hook::InlineCtx *registers) {
        const auto frame = registers->X[29];
        if (!frame) {
            return;
        }
        // This switch arm is specifically cItemName, immediately before
        // RegisterItemName. The other arms register unrelated tag types.
        // Both NNPC_Reaction_DIY and NNPC_Reaction_Cooking request target 16
        // for the villager's current craft. Other targets (gifts/sales), and
        // the player's own crafting UI-result setter retain catalog semantics.
        MarkDialogueItem(registers, frame - layout::ScriptFrameBytes + 0x17f8,
                         frame - layout::ScriptFrameBytes + 0x17ec, "script-item",
                         registers->X[20] && Read<unsigned>(registers->X[20], layout::ActionItemTarget)
                             == layout::VillagerCurrentCraftTarget);
    }
};
} // namespace

bool InstallDialogueItemStars() {
    struct Check {
        std::uintptr_t offset;
        std::array<std::uint32_t, 5> words;
    };
    constexpr Check checks[]{
        {0x00c57abc, {0xf9400688, 0xb400fd88, 0x910aa100, 0x9460017e, 0xaa0003f5}},
        {0x00c57ad8, {0xf001f2c2, 0x911003e0, 0xf946f842, 0xaa1503e1, 0x943acf7b}},
        {0x00c57ae8, {0x943acf7b, 0xf9400688, 0x911003e1, 0x2a1b03e2, 0xf9414908}},
        {0x00c57b10, {0xf9400688, 0xb400fae8, 0x910aa100, 0x94600169, 0xaa0003f5}},
        {0x00c57b2c, {0xf001f2c2, 0x911003e0, 0xf946f842, 0xaa1503e1, 0x943acf66}},
        {0x00c57b3c, {0x943acf66, 0xf9400688, 0x911003e1, 0x2a1b03e2, 0xf9414908}},
        {0x00c587d0, {0xf9400688, 0xb40094e8, 0x910aa100, 0x945ffe39, 0xaa0003f5}},
        {0x00c587ec, {0xd001f2c2, 0x911003e0, 0xf946f842, 0xaa1503e1, 0x943acc36}},
        {0x00c587fc, {0x943acc36, 0xf9400688, 0x911003e1, 0x2a1b03e2, 0xf9414908}},

        {0x00a46318, {0xb940da81, 0x914007e0, 0x914007e2, 0x911fb000, 0x911fd042}},
        {0x00a53e54, {0xf9400408, 0xaa0003f3, 0xb4000068, 0xf9402515, 0x14000002}},
        {0x00a53e80, {0xf94002a8, 0x90020301, 0xaa1503e0, 0xf9401108, 0xf943e821}},
        {0x00a53ea4, {0xb9450aa0, 0xf9400114, 0x946c20fd, 0xb4000814, 0xaa0003f6}},
        {0x00a53ed8, {0xa940a6c8, 0x8b080128, 0x52862a89, 0x72a00029, 0x78696915}},
        {0x00a53ef0, {0xf94016c8, 0xb4000188, 0xf9400500, 0xb4000140, 0xf9400008}},

        {0x00c5c0ec, {0xb81fc3b5, 0xb9410a93, 0xb85fc3a8, 0x7100f91f, 0x54011dc8}},
        {0x00c5e0cc, {0x7942b3e8, 0x9001f2a2, 0x910563e0, 0x39456be9, 0xd101a3a1}},
        {0x00c5e0ec, {0x39000ac9, 0x39000eca, 0xb819c3ab, 0xf946f842, 0x1400000e}},
        {0x00c5ddac, {0xb001f2a2, 0x910563e0, 0xf946f842, 0xaa1503e1, 0x140000de}},
        {0x00c5e124, {0x9001f2a2, 0x910563e0, 0xf946f842, 0x910023e1, 0x943ab5e8}},
        {0x00c592c4, {0x943ac984, 0xf9400688, 0xf9414908, 0xf9401108, 0xf9400108}},
        {0x00c59bb0, {0x943ac749, 0xf94012a8, 0x17fffdc7, 0x51029109, 0x7100093f}},
        {0x00c59bd8, {0x943ac73f, 0xf9400688, 0x911003e1, 0x2a1b03e2, 0xf9414908}},
        {0x00c5e134, {0x943ab5e8, 0xf9400688, 0x910563e1, 0x2a1303e2, 0xf9414908}},
        {0x00a465d0, {0x911fe021, 0x9441a2a7, 0x140001b7, 0x52800033, 0xa94b2688}},
        {0x00c56ee8, {0x910003fd, 0xa90267fa, 0xa9035ff8, 0xa90457f6, 0xa9054ff4}},
        {0x00c56efc, {0xd14007ff, 0xd12d43ff, 0xaa0003f4, 0x2a1f03f5, 0xd102c3b8}},
        {0x00c5bff8, {0x910003fd, 0xa90267fa, 0xa9035ff8, 0xa90457f6, 0xa9054ff4}},
        {0x00c5c00c, {0xd14007ff, 0xd12183ff, 0xaa0003f4, 0x2a1f03f5, 0xb81fc3bf}},
        {0x00a45ccc, {0xa90267fa, 0xa9035ff8, 0xa90457f6, 0xa9054ff4, 0xd14007ff}},
        {0x00a45cdc, {0xd14007ff, 0xd12f43ff, 0xf9400408, 0x914007f5, 0x911f42b5}},
        {0x00a462ec, {0x394aa108, 0x52801fe9, 0xaa0003f4, 0x914007e0, 0x911fb000}},
        {0x01aaf070, {0xd100c3ff, 0xa9027bfd, 0x910083fd, 0xf001bc48, 0x910f0108}},
    };
    if (!game::ValidateCatalogBindings() || !game::ValidateRecipeBindings()) {
        return false;
    }
    for (const auto &check : checks) {
        if (std::memcmp(reinterpret_cast<const void *>(game::MainAddress(check.offset)),
                        check.words.data(), sizeof(check.words))) {
            Logging.Error("[QoL Enhancements/Catalog] DIALOGUE_ITEM_STARS_DISABLED fingerprint mismatch\n");
            return false;
        }
    }

    qol::core::HookPlan hookPlan{
        qol::core::Site<DialogueCatchCreatureStar>("DialogueCatchCreatureStar", 0x00c57aec, {0xf9400688, 0x911003e1, 0x2a1b03e2, 0xf9414908, 0xf9401108}, true),
        qol::core::Site<DialogueCatchCreatureStar>("DialogueCatchCreatureStar", 0x00c57b40, {0xf9400688, 0x911003e1, 0x2a1b03e2, 0xf9414908, 0xf9401108}, true),
        qol::core::Site<DialogueCatchCreatureStar>("DialogueCatchCreatureStar", 0x00c58800, {0xf9400688, 0x911003e1, 0x2a1b03e2, 0xf9414908, 0xf9401108}, true),
        qol::core::Site<DialogueValueItemStar>("DialogueValueItemStar", 0x00c592c8, {0xf9400688, 0xf9414908, 0xf9401108, 0xf9400108, 0x911003e1}, true),
        qol::core::Site<DialogueStoredItemStar>("DialogueStoredItemStar", 0x00c59bb4, {0xf94012a8, 0x17fffdc7, 0x51029109, 0x7100093f, 0x54000982}, true),
        qol::core::Site<DialogueInventoryItemStar>("DialogueInventoryItemStar", 0x00c59bdc, {0xf9400688, 0x911003e1, 0x2a1b03e2, 0xf9414908, 0xf9401108}, true),
        qol::core::Site<DialogueUiResultItemStar>("DialogueUiResultItemStar", 0x00c5e138, {0xf9400688, 0x910563e1, 0x2a1303e2, 0xf9414908, 0xf9401108}, true),
        qol::core::Site<DialogueScriptItemStar>("DialogueScriptItemStar", 0x00a465d0, {0x911fe021, 0x9441a2a7, 0x140001b7, 0x52800033, 0xa94b2688}, true),
    };
    if (!hookPlan) return false;
    if (!hookPlan.Publish<DialogueCatchCreatureStar>(0x00c57aec)) return false;
    if (!hookPlan.Publish<DialogueCatchCreatureStar>(0x00c57b40)) return false;
    if (!hookPlan.Publish<DialogueCatchCreatureStar>(0x00c58800)) return false;
    if (!hookPlan.Publish<DialogueValueItemStar>(0x00c592c8)) return false;
    if (!hookPlan.Publish<DialogueStoredItemStar>(0x00c59bb4)) return false;
    if (!hookPlan.Publish<DialogueInventoryItemStar>(0x00c59bdc)) return false;
    if (!hookPlan.Publish<DialogueUiResultItemStar>(0x00c5e138)) return false;
    if (!hookPlan.Publish<DialogueScriptItemStar>(0x00a465d0)) return false;
    Logging.Debug("[QoL Enhancements/Catalog] DIALOGUE_ITEM_STARS_READY event item substitutions\n");
    return true;
}
} // namespace catalog_aware_shops::features
