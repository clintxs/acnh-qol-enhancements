#include "program/pockets/features/pockets.hpp"
#include "program/pockets/game/storage_menu_native.hpp"
#include <cassert>
#include <iostream>
int main() {
    using namespace qol_pockets::features;
    namespace game = qol_pockets::game;
    int cases=0;
    for (int count: {-1,0,1,2,7,8}) for (int action: {0,1,46,68})
    for (const char* bank: {"CmnSubWin","Other",static_cast<const char*>(nullptr)})
    for (const char* key: {"0202","0123",static_cast<const char*>(nullptr)})
    for (unsigned flags: {0u,1u}) for (unsigned extra: {0u,1u}) {
        const bool expected=count==1 && action==0 && bank && key &&
            std::string_view(bank)=="CmnSubWin" && std::string_view(key)=="0202" && !flags && !extra;
        const game::MenuEntry entry{game::IsMoveToPockets(action, bank, key)
            ? ItemAction::MoveToPockets : ItemAction::Unknown, !flags && !extra};
        const bool result = count == 1 && WithdrawDirectly({&entry, 1}).has_value();
        assert(result == expected);
        ++cases;
    }
    std::cout << "PASS " << cases << " storage single-action policy cases\n";
}
