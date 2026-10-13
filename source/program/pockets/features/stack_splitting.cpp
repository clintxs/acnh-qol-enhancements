#include "stack_splitting.hpp"
#include "pockets.hpp"
namespace qol_pockets::features {
bool RegisterStackMenuExtension(void (*)(game::StackMenu &, int, bool));
bool InstallStackSplitting() { return RegisterStackMenuExtension(AddGrabFiveAndTen); }
}
