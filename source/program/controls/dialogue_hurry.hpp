#pragma once
#include <cstdint>
namespace qol::controls {
inline constexpr unsigned DialogueHurryFlag = 5, DialogueHurrySpeed = 30;
bool InstallHoldToAdvanceDialogue();
unsigned SelectDialogueHurry(std::uintptr_t text, unsigned currentFlag);
void TraceDialogueInput(std::uintptr_t manager, std::uintptr_t text,
                        std::uintptr_t controller, unsigned held);
}
