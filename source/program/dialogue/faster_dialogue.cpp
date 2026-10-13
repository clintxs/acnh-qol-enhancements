#include "features/faster_dialogue.hpp"
#include "lib.hpp"
namespace qol::dialogue {
namespace {
Settings Preferences{};
RevealDecision SelectConfiguredDialogueSpeed(const game::DialogueContext &dialogue) {
    return ChooseSpeed(dialogue, Preferences);
}
}
unsigned Configure(std::string_view input) {
    return ApplySettings(input, Preferences);
}

void ReportSettings() {
    if (NormalizeSettings(Preferences))
        Logging.Warning("[QoL Enhancements/Faster Dialogue] WARNING: boost_speed below base_speed; using base speed for both\n");
    Logging.Settings("[QoL Enhancements/Faster Dialogue] settings: base=%u boost=%u boost_ui_messages=%s boost_unnamed_messages=%s npc_count=%u\n",
                Preferences.baseSpeed, Preferences.boostSpeed, Preferences.boostUiMessages ? "true" : "false",
                Preferences.boostUnnamedMessages ? "true" : "false", Preferences.npcCount);
    for (unsigned i = 0; i < Preferences.npcCount; ++i)
        Logging.Settings("[QoL Enhancements/Faster Dialogue] boost_npc_list[%u]=%s\n", i, Preferences.npcs[i].data());
}

bool Install() {
    return RegisterFasterDialoguePolicy({SelectConfiguredDialogueSpeed, Preferences.baseSpeed, Preferences.boostSpeed});
}
}
