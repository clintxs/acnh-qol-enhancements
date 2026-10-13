#include "storage_hint.hpp"
#include "pockets.hpp"
namespace qol_pockets::features {
bool RegisterPocketFooterExtension(game::ExtendPocketFooter);
bool InstallStorageHint() { return RegisterPocketFooterExtension(AddShortcutHints); }
}
