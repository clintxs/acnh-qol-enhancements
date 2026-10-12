#include "program/dialogue/settings.hpp"
#include <cassert>
#include <string>
#include <cstdio>
using namespace qol::dialogue;
int main() {
    Settings settings{};
    assert(settings.boostUiMessages && settings.boostUnnamedMessages);
    for (unsigned base = MinSpeed; base <= MaxSpeed; ++base) {
        for (unsigned boost = MinSpeed; boost <= MaxSpeed; ++boost) {
            const auto config = "base_speed=" + std::to_string(base) + "\nboost_speed=" + std::to_string(boost);
            assert(ParseSettings(config, settings) == unsigned(boost < base));
            assert(settings.baseSpeed == base && settings.boostSpeed == (boost < base ? base : boost));
            assert(SelectSpeed(settings, MessageKind::Unknown, "", false) == base);
            assert(SelectSpeed(settings, MessageKind::Unknown, "", true) == settings.boostSpeed);
        }
    }
    assert(!ParseSettings("base_speed=4\nboost_speed=8", settings));
    for (auto invalid : {"", "0", "31", "100", "4294967296", "-1", "+2", "02", "2.5", "nan", "inf"}) {
        assert(ApplySettings(std::string("base_speed=") + invalid + "\nboost_speed=" + invalid, settings) == 2);
        assert(settings.baseSpeed == 4 && settings.boostSpeed == 8);
    }
    assert(!ApplySettings("base_speed=9\nboost_speed=3", settings));
    assert(!ApplySettings("base_speed=1", settings));
    assert(!NormalizeSettings(settings) && settings.baseSpeed == 1 && settings.boostSpeed == 3);
    assert(ApplySettings("boost_speed=10\nboost_speed=7", settings) == 1 && settings.boostSpeed == 3);
    assert(!ParseSettings("", settings));
    for (auto kind : {MessageKind::Npc, MessageKind::Ui, MessageKind::Unnamed, MessageKind::Unknown}) {
        assert(SelectSpeed(settings, kind, "rcm", false) ==
               (kind == MessageKind::Ui || kind == MessageKind::Unnamed ? 4 : 2));
        assert(SelectSpeed(settings, kind, "rcm", true) == 4);
    }
    assert(!ParseSettings("\xef\xbb\xbf# Personal settings\r\nbase_speed = 2\r\n"
                          "boost_speed = 4\nboost_ui_messages = true\nboost_unnamed_messages = true\nboost_npc_list = Timmy, Tommy, Orville, Wilbur, Kapp'n, rcm, cat00\n", settings));
    assert(settings.npcCount == 7);
    for (auto id : {"rcm", "rct", "dod", "doc", "kpp", "kppB", "cat00", "RCM"}) {
        assert(SelectSpeed(settings, MessageKind::Npc, id, false) == 4);
        assert(SelectSpeed(settings, MessageKind::Npc, id, true) == 4);
    }
    assert(SelectSpeed(settings, MessageKind::Npc, "rco", false) == 2);
    assert(SelectSpeed(settings, MessageKind::Ui, "", false) == 4);
    assert(SelectSpeed(settings, MessageKind::Unknown, "rcm", false) == 2);
    assert(ParseSettings("base_speed=0\nboost_speed=nan\nboost_ui_messages=yes\n"
                         "boost_npc_list=Timmy,Not An NPC,,Orville,\nunknown=true\nmalformed\n", settings) == 7);
    assert(settings.baseSpeed == 2 && settings.boostSpeed == 4 && settings.boostUiMessages && settings.npcCount == 3);
    assert(ParseSettings("base_speed=4\nbase_speed=4\nboost_speed=2\nboost_speed=2\n"
                         "boost_ui_messages=true\nboost_ui_messages=true\nboost_npc_list=doc\nboost_npc_list=rcm", settings) == 4);
    assert(settings.baseSpeed == 2 && settings.boostSpeed == 4 && settings.boostUiMessages && !settings.npcCount);
    assert(ParseSettings("base_speed=4\nboost_speed=2", settings) == 1);
    assert(settings.baseSpeed == 4 && settings.boostSpeed == 4);
    assert(!ParseSettings("boost_npc_list=", settings) && !settings.npcCount);
    assert(ParseSettings(std::string(4097, 'x'), settings) && settings.baseSpeed == 2);
    std::string list = "boost_npc_list=";
    for (unsigned i = 0; i < 65; ++i) list += (i ? ",npc" : "npc") + std::to_string(i);
    assert(ParseSettings(list, settings) == 1 && settings.npcCount == MaxNpcs);
    assert(!ParseSettings("boost_ui_messages=true\nboost_unnamed_messages=false", settings));
    assert(SelectSpeed(settings, MessageKind::Ui, "", false) == 4);
    assert(SelectSpeed(settings, MessageKind::Unnamed, "", false) == 2);
    assert(!ParseSettings("boost_ui_messages=false\nboost_unnamed_messages=true", settings));
    assert(SelectSpeed(settings, MessageKind::Unnamed, "", false) == 4);
    assert(SelectSpeed(settings, MessageKind::Ui, "", false) == 2);
    assert(SelectSpeed(settings, MessageKind::Npc, "rcm", false) == 2);
    assert(SelectSpeed(settings, MessageKind::Unknown, "", false) == 2);
    assert(ParseSettings("boost_unnamed_messages=yes", settings) == 1 && settings.boostUnnamedMessages);
    assert(ParseSettings("boost_unnamed_messages=false\nboost_unnamed_messages=true", settings) == 1 && settings.boostUnnamedMessages);
    assert(!ApplySettings("boost_unnamed_messages=false", settings) && !settings.boostUnnamedMessages);
    assert(ApplySettings("boost_unnamed_messages=true\nboost_unnamed_messages=false", settings) == 1 && !settings.boostUnnamedMessages);
    // Removed legacy key is unknown and cannot change the supported preference.
    assert(ApplySettings("faster_player=true", settings) == 1 && !settings.boostUnnamedMessages);
    assert(ParseSettings("faster_player=false", settings) == 1 && settings.boostUnnamedMessages);
    // Layer inheritance, valid false overrides, list replacement/clearing, and
    // invalid/duplicate custom values preserving their regular-file values.
    settings = {};
    assert(!ApplySettings("enabled=true\nversion=0.2.4\nbase_speed=4\nboost_speed=2\n"
                          "boost_ui_messages=true\nboost_unnamed_messages=true\nboost_npc_list=Timmy,Tommy", settings));
    assert(!ApplySettings("base_speed=2\nboost_ui_messages=false\nboost_npc_list=Wilbur", settings));
    assert(!NormalizeSettings(settings));
    assert(settings.baseSpeed == 2 && settings.boostSpeed == 2 && !settings.boostUiMessages && settings.boostUnnamedMessages);
    assert(settings.npcCount == 1 && Equal(settings.npcs[0].data(), "doc"));
    assert(ApplySettings("boost_speed=bad\nboost_unnamed_messages=bad\nboost_npc_list=Timmy\nboost_npc_list=Tommy", settings) == 3);
    assert(settings.boostSpeed == 2 && settings.boostUnnamedMessages && settings.npcCount == 1);
    assert(Equal(settings.npcs[0].data(), "doc"));
    assert(!ApplySettings("boost_npc_list=", settings) && settings.npcCount == 0);
    assert(!ParseSettings("boost_npc_list=Blathers,Sócrates,フータ,傅达,Блезерс,Mr. Resetti", settings));
    assert(settings.npcCount == 6);
    for (auto name : {"Blathers", "blathers", "Sócrates", "フータ", "傅达", "Блезерс", "Mr. Resetti"}) {
        assert(SelectSpeed(settings, MessageKind::Npc, "owl", false, name) == 4);
        assert(SelectSpeed(settings, MessageKind::Unknown, "owl", false, name) == 2);
    }
    assert(SelectSpeed(settings, MessageKind::Npc, "owl", false, "Thibou") == 2);
    assert(!ParseSettings("boost_npc_list=owl", settings));
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
    // Retired setting names are rejected rather than silently changing new preferences.
    settings = {};
    assert(ApplySettings("faster_ui=false\nfaster_unnamed=false\nfaster_npc_list=owl", settings) == 3);
    assert(settings.boostUiMessages && settings.boostUnnamedMessages && !settings.npcCount);
    puts("PASS: dialogue defaults, aliases/IDs, independent UI/unnamed/NPC policy, exact self-message banks, non-stacking boost, invalid keys/values, duplicates and bounded lists");
}
