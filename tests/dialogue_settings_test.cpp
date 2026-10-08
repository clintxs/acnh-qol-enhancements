#include "program/dialogue/settings.hpp"
#include <cassert>
#include <string>
#include <cstdio>
using namespace qol::dialogue;
int main() {
    Settings settings{};
    assert(!ParseSettings("", settings));
    for (auto kind : {MessageKind::Npc, MessageKind::Ui, MessageKind::Unknown}) {
        assert(SelectSpeed(settings, kind, "rcm", false) == 2);
        assert(SelectSpeed(settings, kind, "rcm", true) == 4);
    }
    assert(!ParseSettings("\xef\xbb\xbf# Personal settings\r\nbase_speed = 2\r\n"
                          "boost_speed = 4\nfaster_ui = true\nfaster_npc_list = Timmy, Tommy, Orville, Wilbur, Kapp'n, rcm, cat00\n", settings));
    assert(settings.npcCount == 7);
    for (auto id : {"rcm", "rct", "dod", "doc", "kpp", "kppB", "cat00", "RCM"}) {
        assert(SelectSpeed(settings, MessageKind::Npc, id, false) == 4);
        assert(SelectSpeed(settings, MessageKind::Npc, id, true) == 4);
    }
    assert(SelectSpeed(settings, MessageKind::Npc, "rco", false) == 2);
    assert(SelectSpeed(settings, MessageKind::Ui, "", false) == 4);
    assert(SelectSpeed(settings, MessageKind::Unknown, "rcm", false) == 2);
    assert(ParseSettings("base_speed=0\nboost_speed=nan\nfaster_ui=yes\n"
                         "faster_npc_list=Timmy,Not An NPC,,Orville,\nunknown=true\nmalformed\n", settings) == 8);
    assert(settings.baseSpeed == 2 && settings.boostSpeed == 4 && !settings.fasterUi && settings.npcCount == 2);
    assert(ParseSettings("base_speed=4\nbase_speed=4\nboost_speed=2\nboost_speed=2\n"
                         "faster_ui=true\nfaster_ui=true\nfaster_npc_list=doc\nfaster_npc_list=rcm", settings) == 4);
    assert(settings.baseSpeed == 2 && settings.boostSpeed == 4 && !settings.fasterUi && !settings.npcCount);
    assert(ParseSettings("base_speed=4\nboost_speed=2", settings) == 1);
    assert(settings.baseSpeed == 4 && settings.boostSpeed == 4);
    assert(!ParseSettings("faster_npc_list=", settings) && !settings.npcCount);
    assert(ParseSettings(std::string(4097, 'x'), settings) && settings.baseSpeed == 2);
    std::string list = "faster_npc_list=";
    for (unsigned i = 0; i < 65; ++i) list += (i ? ",npc" : "npc") + std::to_string(i);
    assert(ParseSettings(list, settings) == 1 && settings.npcCount == MaxNpcs);
    puts("PASS: dialogue defaults, aliases/IDs, independent UI/NPC policy, non-stacking boost, invalid keys/values, duplicates and bounded lists");
}
