#include "lib.hpp"
#include "settings.hpp"
namespace qol_pockets {
namespace { Settings Preferences{}; }
unsigned Configure(std::string_view input) { return ApplySettings(input, Preferences); }
const Settings &GetSettings() { return Preferences; }
void ReportSettings() {
    Logging.Settings("[QoL Enhancements/Pockets] settings: bypass_menu_open_containers=%s "
                     "bypass_menu_claim_recycle_box=%s bypass_menu_withdraw_home_storage=%s enable_multiselect=%s\n",
                     Preferences.bypassMenuOpenContainers ? "true" : "false",
                     Preferences.bypassMenuClaimRecycleBox ? "true" : "false",
                     Preferences.bypassMenuWithdrawHomeStorage ? "true" : "false",
                     Preferences.enableMultiselect ? "true" : "false");
}
}
