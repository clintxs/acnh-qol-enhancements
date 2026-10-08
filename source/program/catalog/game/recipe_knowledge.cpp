#include "recipe_knowledge.hpp"
#include "bindings.hpp"
#include <array>
#include <cstring>

namespace catalog_aware_shops::game {
namespace {
namespace address {
constexpr std::uintptr_t RecipeManagerSlot = 0x04ab46f0;
constexpr std::uintptr_t PlayerReadinessSlot = 0x04ab4fb8;
constexpr std::uintptr_t SaveManager = 0x054021a0, FallbackSave = 0x05474040;
constexpr std::uintptr_t ResolvePlayer = 0x02482d8c, ResolveSave = 0x024830c0;
constexpr std::uintptr_t HasLearnedDiyRecipe = 0x0251e658;
constexpr std::uintptr_t HasLearnedCookingRecipe = 0x0251288c;
} // namespace address
namespace layout {
constexpr std::size_t ValidDiyRecipeBits = 0xe98e0, ValidCookingRecipeBits = 0xe99e0;
constexpr std::size_t PlayerRecordSize = 0x13f0, ReadyA = 0x13d4, ReadyB = 0x13d6;
constexpr std::size_t SaveView = 0x194b0, ViewReady = 8, FallbackView = 0x20;
constexpr std::size_t RecipeBits = 0x8690;
} // namespace layout
constexpr unsigned PlayerCount = 8, RecipeCount = 2048, LocalPlayerLookup = 1;
template <class T> T Read(std::uintptr_t object, std::size_t offset = 0) {
    T value;
    std::memcpy(&value, reinterpret_cast<const void *>(object + offset), sizeof(value));
    return value;
}
std::uintptr_t ReadIndirectGlobal(std::uintptr_t offset) {
    const auto slot = Read<std::uintptr_t>(MainAddress(offset));
    return slot ? Read<std::uintptr_t>(slot) : 0;
}
// The native bool getter returns false both for unlearned and unavailable data.
// Validate its save path first so missing data never produces a false star.
bool HasReadyRecipeHistory(std::uint8_t player) {
    const auto index = FunctionAt<unsigned (*)(const std::uint8_t *, unsigned)>(
        address::ResolvePlayer)(&player, LocalPlayerLookup);
    const auto records = ReadIndirectGlobal(address::PlayerReadinessSlot);
    if (index >= PlayerCount || !records ||
        !Read<std::uint8_t>(records, index * layout::PlayerRecordSize + layout::ReadyA) ||
        !Read<std::uint8_t>(records, index * layout::PlayerRecordSize + layout::ReadyB)) {
        return false;
    }
    const auto manager = Read<std::uintptr_t>(MainAddress(address::SaveManager));
    if (!manager) {
        return false;
    }
    // Use exactly the resolver used by both learned-recipe getters, including local-player
    // remapping. This retrieves the live save view; it does not learn a recipe.
    const auto save =
        FunctionAt<std::uintptr_t (*)(std::uintptr_t, const std::uint8_t *, unsigned)>(
            address::ResolveSave)(manager, &player, LocalPlayerLookup);
    if (!save) {
        return false;
    }
    auto view = save + layout::SaveView;
    if (!Read<std::uintptr_t>(view, layout::ViewReady)) {
        const auto fallback = Read<std::uintptr_t>(MainAddress(address::FallbackSave));
        view = fallback ? Read<std::uintptr_t>(fallback, layout::FallbackView) : 0;
    }
    return view && Read<std::uintptr_t>(view, layout::RecipeBits);
}
} // namespace

RecipeStatus QueryRecipeCard(std::uint8_t player, const CatalogItem &item) {
    // Native card getter 01a0c3c8 identifies item 16a2 and uses extra's low
    // 16 bits as the recipe number. Wrapped/special item forms are excluded.
    const auto recipe = static_cast<std::uint16_t>(item.extra);
    if (player >= PlayerCount || item.id != RecipeCardId || (item.flags[1] & 3) ||
        recipe >= RecipeCount) {
        return RecipeStatus::Unavailable;
    }
    const auto manager = ReadIndirectGlobal(address::RecipeManagerSlot);
    if (!manager) {
        return RecipeStatus::Unavailable;
    }
    const auto mask = 1u << (recipe % 8);
    const bool isDiy = Read<std::uint8_t>(manager, layout::ValidDiyRecipeBits + recipe / 8) & mask;
    const bool isCooking =
        Read<std::uint8_t>(manager, layout::ValidCookingRecipeBits + recipe / 8) & mask;
    // These native validity tables are disjoint. Unknown or ambiguous recipe
    // categories must not be interpreted as an unlearned recipe.
    if (isDiy == isCooking || !HasReadyRecipeHistory(player)) {
        return RecipeStatus::Unavailable;
    }
    // Pockets uses 0251e658 for DIY (0307ab08) and 0251288c for cooking
    // (0307aa14). Both preserve native read locks and read the first learned
    // bitset. The adjacent +100 history is unrelated to this distinction.
    // Their ABIs differ: cooking takes pointers to the recipe and player.
    const bool learned = isCooking
                             ? FunctionAt<bool (*)(const std::uint16_t *, const std::uint8_t *)>(
                                   address::HasLearnedCookingRecipe)(&recipe, &player)
                             : FunctionAt<bool (*)(std::uintptr_t, unsigned, const std::uint8_t *)>(
                                   address::HasLearnedDiyRecipe)(manager, recipe, &player);
    return learned ? RecipeStatus::Learned : RecipeStatus::Unlearned;
}

bool ValidateRecipeBindings() {
    struct Check {
        std::uintptr_t offset;
        std::array<std::uint32_t, 5> words;
    };
    // Exact 3.0.3 ARM instructions: getter, resolver and recipe-card decoding.
    constexpr Check checks[]{
        {0x0251288c, {0xa9bd7bfd, 0xf9000bf5, 0xa9024ff4, 0x910003fd, 0x79400008}},
        {0x025128ac, {0xd343fd13, 0xf9437929, 0x52933c0a, 0x72a001ca, 0xf9400129}},
        {0x0251292c, {0x5290d209, 0x8b090115, 0xf94012a8, 0xb40001c8, 0xf9400100}},
        {0x02512974, {0xa94026aa, 0x8b0a0129, 0x38736929, 0x6a09029f, 0x1a9f07f3}},
        {0x0307aa00, {0x910003e0, 0xaa1503e1, 0x97d25fe9, 0x910003e0, 0xaa1403e1}},
        {0x01a0d338, {0xb9400e68, 0x52898f60, 0x12003d09, 0x711ffd3f, 0x540001e8}},
        {0x01a0d360, {0x52933c0a, 0x72a001ca, 0x386a6929, 0x1200090a, 0x1aca2529}},
        {0x0251e658, {0xa9bd7bfd, 0xf9000bf5, 0xa9024ff4, 0x910003fd, 0x12003c28}},
        {0x0251e680, {0x8b130008, 0x5280002a, 0x38696908, 0x12000829, 0x1ac92154}},
        {0x0251e6b0, {0x1a893108, 0x390073a8, 0x90017728, 0x91068108, 0xf9400100}},
        {0x0251e708, {0xf9400108, 0xd63f0100, 0x52000008, 0x14000008, 0x2a1f03f3}},
        {0x024830c0, {0xd10143ff, 0xa9027bfd, 0xa90357f6, 0xa9044ff4, 0x910083fd}},
        {0x01a0c480, {0x91002260, 0x9431c490, 0x5282d448, 0x6b08001f, 0x54000261}},
        {0x01a0c494, {0xb9400e68, 0x52898f60, 0x12003d09, 0x711ffd3f, 0x540001e8}},
        {0x0307aaf4, {0xd000d1c8, 0x2a0003e1, 0xf9437908, 0xaa1403e2, 0xf9400100}},
    };
    for (const auto &check : checks) {
        if (std::memcmp(reinterpret_cast<const void *>(MainAddress(check.offset)),
                        check.words.data(), sizeof(check.words))) {
            return false;
        }
    }
    return true;
}
} // namespace catalog_aware_shops::game
