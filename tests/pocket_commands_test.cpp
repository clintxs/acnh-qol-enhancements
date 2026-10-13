#include "program/pockets/services/shortcut_declarations.hpp"
#include "program/game/inventory.hpp"
#include "program/pockets/game/quick_store_profile.hpp"
#include <cassert>
#include <iostream>
#include <type_traits>

using namespace qol_pockets::commands;
using qol::game::inventory::ItemIdentity;
using qol::game::inventory::ItemValue;
constexpr ShortcutRule rules[]{
    {UiButton::Y, PocketAction::PutInStorage},
    {UiButton::ZL, PocketAction::DropOrRelease},
    {UiButton::ZR, PocketAction::EquipOrConsume},
};
static_assert(ValidShortcuts(rules));
static_assert(!std::is_convertible_v<UiButton, unsigned>);
static_assert(!std::is_convertible_v<PocketAction, int>);
static_assert(!std::is_convertible_v<qol_pockets::game::v303::quick_store::MenuRowId, int>);
template<class View> concept UncheckedItemAccess = requires(const View &view) { view.ItemAt(0); };
static_assert(!UncheckedItemAccess<qol::game::inventory::ResidentView>);
static_assert(!UncheckedItemAccess<qol::game::inventory::PocketSlots>);
static_assert(!std::is_constructible_v<qol::game::inventory::ResidentView, std::uintptr_t, const unsigned char *>);
int main() {
    assert(!ValidShortcuts({}));
    const ShortcutRule duplicate[]{{UiButton::Y, PocketAction::PutInStorage},
                                   {UiButton::Y, PocketAction::DropOrRelease}};
    assert(!ValidShortcuts(duplicate));
    for (auto invalid : {0u, 1u, 0x480u, 0x10000u}) {
        const ShortcutRule bad[]{{static_cast<UiButton>(invalid), PocketAction::PutInStorage}};
        assert(!ValidShortcuts(bad));
    }
    for (auto invalid : {PocketAction::OpenContainer, static_cast<PocketAction>(99)}) {
        const ShortcutRule bad[]{{UiButton::Y, invalid}};
        assert(!ValidShortcuts(bad));
    }
    const ShortcutRule remapped[]{{UiButton::Y, PocketAction::DropOrRelease}};
    assert(ValidShortcuts(remapped));
    ShortcutDeclarations owned;
    {
        ShortcutRule local[]{{UiButton::Y, PocketAction::DropOrRelease}};
        assert(owned.Own(local));
        local[0].action = PocketAction::PutInStorage;
    }
    assert(owned.Rules().size() == 1 && owned.Rules()[0].action == PocketAction::DropOrRelease);
    assert(owned.Compare(remapped) == DeclarationMatch::Identical && owned.Own(remapped));
    assert(owned.Compare(rules) == DeclarationMatch::Conflict && !owned.Own(rules));
    assert(owned.Rules().size() == 1 && owned.Rules()[0].action == PocketAction::DropOrRelease);
    const ItemValue item{true, 0x123456789abc4321ull};
    const auto identity = item.Identity();
    assert(identity == ItemIdentity{item.value});
    for (unsigned bit = 0; bit < 64; ++bit)
        assert(identity != ItemIdentity{item.value ^ (1ull << bit)});
    auto copied = item;
    copied.value = 0xfffe;
    assert(identity == item.Identity() && identity != copied.Identity());
    std::cout << "PASS: typed UI/action/row boundaries, bounded shortcut registration and exact 64-bit item identity\n";
}
