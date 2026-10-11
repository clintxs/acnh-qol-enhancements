#pragma once

namespace qol_pockets::features::shortcuts {
// UI input bit positions (not nn::hid's button mask). Native footer glyph
// records at 032f94c4 identify buttons 10/11 as ZL/ZR.
constexpr unsigned Storage = 1u << 7, Drop = 1u << 10, Equip = 1u << 11;
constexpr unsigned Buttons = Storage | Drop | Equip;
// Row indices in the original ItemUIContextMenu.bcsv, NOT its operation enum.
constexpr int Wear = 5, Hold = 6, DropItem = 30, PutAway = 35, Release = 41, PutInStorage = 49;
// Verified 3.0.3 rows: Eat/0133, EatOne/0134, Drink/0133_01.
// EatTurnip (39) is a separate ten-turnip action, not part of this shortcut.
constexpr int Eat = 37, EatOne = 38, Drink = 40;
constexpr bool IsConsume(int action) {
    return action == Eat || action == EatOne || action == Drink;
}
constexpr bool Matches(unsigned button, int action) {
    return (button == Storage && action == PutInStorage) ||
           (button == Drop && (action == Release || action == DropItem)) ||
           (button == Equip && (action == Hold || action == Wear || action == PutAway ||
                                IsConsume(action)));
}
}
