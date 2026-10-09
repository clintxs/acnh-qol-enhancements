#include "program/dialogue/settings.hpp"
#include <cassert>
#include <string>
#include <cstdio>
using namespace qol::dialogue;
int main() {
    Settings settings{};
    assert(!ParseSettings("", settings));
    for (auto kind : {MessageKind::Npc, MessageKind::Ui, MessageKind::Unnamed, MessageKind::Unknown}) {
        assert(SelectSpeed(settings, kind, "rcm", false) == 2);
        assert(SelectSpeed(settings, kind, "rcm", true) == 4);
    }
    assert(!ParseSettings("\xef\xbb\xbf# Personal settings\r\nbase_speed = 2\r\n"
                          "boost_speed = 4\nfaster_ui = true\nfaster_player = true\nfaster_npc_list = Timmy, Tommy, Orville, Wilbur, Kapp'n, rcm, cat00\n", settings));
    assert(settings.npcCount == 7);
    for (auto id : {"rcm", "rct", "dod", "doc", "kpp", "kppB", "cat00", "RCM"}) {
        assert(SelectSpeed(settings, MessageKind::Npc, id, false) == 4);
        assert(SelectSpeed(settings, MessageKind::Npc, id, true) == 4);
    }
    assert(SelectSpeed(settings, MessageKind::Npc, "rco", false) == 2);
    assert(SelectSpeed(settings, MessageKind::Ui, "", false) == 4);
    assert(SelectSpeed(settings, MessageKind::Unknown, "rcm", false) == 2);
    assert(ParseSettings("base_speed=0\nboost_speed=nan\nfaster_ui=yes\n"
                         "faster_npc_list=Timmy,Not An NPC,,Orville,\nunknown=true\nmalformed\n", settings) == 7);
    assert(settings.baseSpeed == 2 && settings.boostSpeed == 4 && !settings.fasterUi && settings.npcCount == 3);
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
    assert(!ParseSettings("faster_ui=true", settings));
    assert(SelectSpeed(settings, MessageKind::Ui, "", false) == 4);
    assert(SelectSpeed(settings, MessageKind::Unnamed, "", false) == 2);
    assert(!ParseSettings("faster_player=true", settings));
    assert(SelectSpeed(settings, MessageKind::Unnamed, "", false) == 4);
    assert(SelectSpeed(settings, MessageKind::Ui, "", false) == 2);
    assert(SelectSpeed(settings, MessageKind::Npc, "rcm", false) == 2);
    assert(SelectSpeed(settings, MessageKind::Unknown, "", false) == 2);
    assert(ParseSettings("faster_player=yes", settings) == 1 && !settings.fasterUnnamed);
    assert(ParseSettings("faster_player=true\nfaster_player=false", settings) == 1 && !settings.fasterUnnamed);
    assert(!ParseSettings("faster_unnamed=true", settings) && settings.fasterUnnamed);
    assert(!ApplySettings("faster_player=false", settings) && !settings.fasterUnnamed);
    assert(!ApplySettings("faster_unnamed=true", settings) && settings.fasterUnnamed);
    assert(ApplySettings("faster_player=false\nfaster_unnamed=false", settings) == 1 && settings.fasterUnnamed);
    assert(ParseSettings("faster_unnamed=true\nfaster_player=false", settings) == 1 && !settings.fasterUnnamed);
    assert(ParseSettings("faster_unnamed=yes", settings) == 1 && !settings.fasterUnnamed);
    // Layer inheritance, valid false overrides, list replacement/clearing, and
    // invalid/duplicate custom values preserving their regular-file values.
    settings = {};
    assert(!ApplySettings("enabled=true\nversion=0.2.4\nbase_speed=4\nboost_speed=2\n"
                          "faster_ui=true\nfaster_player=true\nfaster_npc_list=Timmy,Tommy", settings));
    assert(!ApplySettings("base_speed=2\nfaster_ui=false\nfaster_npc_list=Wilbur", settings));
    assert(!NormalizeSettings(settings));
    assert(settings.baseSpeed == 2 && settings.boostSpeed == 2 && !settings.fasterUi && settings.fasterUnnamed);
    assert(settings.npcCount == 1 && Equal(settings.npcs[0].data(), "doc"));
    assert(ApplySettings("boost_speed=bad\nfaster_player=bad\nfaster_npc_list=Timmy\nfaster_npc_list=Tommy", settings) == 3);
    assert(settings.boostSpeed == 2 && settings.fasterUnnamed && settings.npcCount == 1);
    assert(Equal(settings.npcs[0].data(), "doc"));
    assert(!ApplySettings("faster_npc_list=", settings) && settings.npcCount == 0);
    assert(!ParseSettings("faster_npc_list=Blathers,Sócrates,フータ,傅达,Блезерс,Mr. Resetti", settings));
    assert(settings.npcCount == 6);
    for (auto name : {"Blathers", "blathers", "Sócrates", "フータ", "傅达", "Блезерс", "Mr. Resetti"}) {
        assert(SelectSpeed(settings, MessageKind::Npc, "owl", false, name) == 4);
        assert(SelectSpeed(settings, MessageKind::Unknown, "owl", false, name) == 2);
    }
    assert(SelectSpeed(settings, MessageKind::Npc, "owl", false, "Thibou") == 2);
    assert(!ParseSettings("faster_npc_list=owl", settings));
    assert(SelectSpeed(settings, MessageKind::Npc, "owl", false, "Thibou") == 4);
    assert(!AddNpc(settings, std::string(NpcNameBytes, 'a')));
    assert(!AddNpc(settings, "bad\x01name"));
    assert(!AddNpc(settings, "\xc0\xaf"));
    char label[NpcNameBytes]{};
    const auto convert = [&](std::u16string input) {
        input.push_back(0);
        return CopyLabelName(input, label);
    };
    assert(convert(u"Sócrates") && std::string(label) == "Sócrates");
    assert(convert(u"フータ") && std::string(label) == "フータ");
    assert(convert(u"\U0001f989") && std::string(label) == "🦉");
    // Embedded zero units belong to tag parameters, not the terminator.
    std::u16string russian = u"Блезерс";
    russian += std::u16string{0xe, 50, 22, 14, 0, 2, u'а', 2, u'у', 2, u'а'};
    assert(convert(russian) && std::string(label) == "Блезерс");
    std::u16string compound = u"Кид";
    const std::u16string grammar{0xe, 50, 22, 14, 0, 2, u'а', 2, u'у', 2, u'а'};
    compound += grammar + u" Кэт" + grammar;
    assert(convert(compound) && std::string(label) == "Кид Кэт");
    assert(!convert(std::u16string{0xd800}) && !label[0]);
    assert(!convert(std::u16string{0xdc00}) && !label[0]);
    assert(!convert(std::u16string(NpcNameBytes, u'A')) && !label[0]);
    assert(!convert(std::u16string{u'A', 0xe, 0, 0, 400}) && !label[0]);
    assert(!convert(std::u16string{u'A', 0xe, 0, 0, 1}) && !label[0]);
    assert(!CopyLabelName(std::u16string(64, u'A'), label) && !label[0]);
    for (auto bank : {"FTR_WorkingTable", "SYS_Player", "SYS_Get_Fish", "SYS_Get_Insect", "SYS_Get_DiveFish",
                      "TalkFtr/FTR_WorkingTable", "TalkSys/SYS_Player.msbt"})
        assert(IsUnnamedMessageBank(bank));
    for (auto bank : {"", "SYS_Get_Fish_Extra", "NotSYS_Player", "SP_rcm_00_common", "SYS_Door",
                      "OBJ_RcoMachine", "TalkSys/SYS_Player.msbt/Unknown", "FTR_WorkingTableExtra"})
        assert(!IsUnnamedMessageBank(bank));
    puts("PASS: dialogue defaults, aliases/IDs, independent UI/unnamed/NPC policy, exact self-message banks, non-stacking boost, invalid keys/values, duplicates and bounded lists");
}
