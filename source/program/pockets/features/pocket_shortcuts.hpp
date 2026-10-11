#pragma once

namespace qol_pockets::features::shortcuts {
// UI input bit positions (not nn::hid's button mask). Native footer glyph
// records at 032f94c4 identify buttons 10/11 as ZL/ZR.
constexpr unsigned Storage = 1u << 7, Drop = 1u << 10, Equip = 1u << 11;
constexpr unsigned Buttons = Storage | Drop | Equip;
// Row indices in the original ItemUIContextMenu.bcsv, NOT its operation enum.
constexpr int Wear = 5, Hold = 6, DropItem = 30, PutAway = 35, Release = 41, PutInStorage = 49;
constexpr bool Matches(unsigned button, int action) {
    return (button == Storage && action == PutInStorage) ||
           (button == Drop && (action == Release || action == DropItem)) ||
           (button == Equip && (action == Hold || action == Wear || action == PutAway));
}
}
