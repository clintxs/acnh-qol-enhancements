#include "program/pockets/features/pockets.hpp"
#include "program/pockets/services/pocket_commands.hpp"
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
using namespace qol_pockets;
using namespace features;
using game::ItemAction;
using game::MenuEntry;
int main() {
    // Mixed native ordering: first supported equipment wins over earlier food.
    std::array entries{MenuEntry{ItemAction::Eat, true}, MenuEntry{ItemAction::Hold, false},
        MenuEntry{ItemAction::Drink, true}, MenuEntry{ItemAction::Wear, true},
        MenuEntry{ItemAction::PutAway, true}};
    assert(EquipOrConsume(entries) == 3);
    entries[3].executable = entries[4].executable = false;
    assert(EquipOrConsume(entries) == 0);
    entries[0].executable = false;
    assert(EquipOrConsume(entries) == 2);
    entries[2].executable = false;
    assert(!EquipOrConsume(entries));
    const std::array drop{MenuEntry{ItemAction::Unknown, true}, MenuEntry{ItemAction::Drop, false},
        MenuEntry{ItemAction::Release, true}, MenuEntry{ItemAction::Drop, true}};
    assert(DropOrRelease(drop) == 2);
    assert(!StoreAtHome(drop));
    const std::array store{MenuEntry{ItemAction::PutAway, true}, MenuEntry{ItemAction::Store, true}};
    assert(StoreAtHome(store) == 1); // Equipment Put Away is distinct from home storage.
    for (auto [meaning, choose] : {
        std::pair{ItemAction::Open, OpenContainerDirectly},
        std::pair{ItemAction::MoveToPockets, WithdrawDirectly},
        std::pair{ItemAction::Claim, ClaimDirectly}}) {
        assert(!choose({}));
        const std::array sole{MenuEntry{meaning, true}};
        assert(choose(sole) == 0);
        const std::array unsupported{MenuEntry{meaning, false}};
        assert(!choose(unsupported));
        const std::array multiple{MenuEntry{meaning, true}, MenuEntry{ItemAction::Unknown, false}};
        assert(!choose(multiple)); // Even an unsupported extra row prevents bypass.
        const std::array wrong{MenuEntry{ItemAction::Wear, true}};
        assert(!choose(wrong));
    }
    game::ActionList list{{game::Action::GrabOne}, 1};
    game::StackMenu menu(list);
    assert(!menu.InsertAfter(game::Action::End, game::Action::GrabFive));
    assert(menu.InsertAfter(game::Action::GrabOne, game::Action::GrabFive));
    assert(!menu.InsertAfter(game::Action::GrabOne, game::Action::GrabFive));
    assert(menu.InsertAfter(game::Action::GrabFive, game::Action::GrabTen));
    assert(list.items[2] == game::Action::GrabTen); // Handle survives preceding insertion.
    game::ActionList ambiguous{{game::Action::GrabOne, game::Action::GrabOne}, 2};
    game::StackMenu ambiguousMenu(ambiguous);
    AddGrabFiveAndTen(ambiguousMenu, 30, true);
    assert(ambiguous.count == 2);
    game::ActionList collision{{game::Action::GrabOne, game::Action::GrabTen}, 2};
    game::StackMenu collisionMenu(collision);
    AddGrabFiveAndTen(collisionMenu, 30, true);
    assert(collision.count == 3 && collision.items[1] == game::Action::GrabFive);
    assert(collision.items[2] == game::Action::GrabTen);
    for (int limit : {-1, 0, 1, 2, 40}) {
        assert(CanHoldSelect(game::SelectionContext::ItemSelection, limit) == (limit == -1 || limit > 1));
        assert(CanHoldSelect(game::SelectionContext::HomeStorage, limit) == (limit == -1 || limit > 1));
        assert(!CanHoldSelect(game::SelectionContext::Other, limit));
    }
    HoldSelection hold;
    assert(!hold.CanStart());
    hold.Release();
    assert(hold.CanStart() && hold.Begin(false));
    assert(hold.WantsToggle(false, true) && !hold.WantsToggle(true, true));
    assert(!hold.WantsToggle(false, false)); // Revisit/no movement cannot create a request.
    assert(!hold.Begin(true) && hold.WantsToggle(false, true)); // Keep initial intent.
    hold.Block();
    assert(!hold.CanStart() && !hold.WantsToggle(false, true));
    hold.Release();
    assert(hold.Begin(true));
    assert(hold.WantsToggle(true, true) && !hold.WantsToggle(false, true));
    hold.InitialClick(false); // Native click moved in the same initial frame.
    assert(hold.WantsToggle(false, true));
    std::vector<game::PocketHint> hints;
    game::PocketFooter footer(&hints, [](void *context, game::PocketHint hint) {
        static_cast<std::vector<game::PocketHint> *>(context)->push_back(hint);
    });
    AddShortcutHints(footer, false);
    assert((hints == std::vector{game::PocketHint::Drop, game::PocketHint::Use}));
    hints.clear(); AddShortcutHints(footer, true);
    assert((hints == std::vector{game::PocketHint::Store, game::PocketHint::Drop, game::PocketHint::Use}));
    commands::MenuPolicy invalid{DropOrRelease, static_cast<game::FeedbackTiming>(99), true};
    assert(!invalid.Valid());
    std::cout << "PASS: Pockets action priority, sole-action checks, stable insertion/collisions, held selection intent and fixed hints\n";
}
