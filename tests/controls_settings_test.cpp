#include "program/controls/settings.hpp"
#include <cassert>
#include <iostream>
#include <string>
using namespace qol::controls;
int main() {
    Settings settings{};
    assert(settings.autoEquipTraversal && settings.autoVault && settings.autoLadder);
    assert(!ApplySettings("# inherit defaults", settings));
    assert(settings.autoEquipTraversal && settings.autoVault && settings.autoLadder);
    assert(!ApplySettings("auto_equip_traversal=true", settings) && settings.autoEquipTraversal);
    assert(ApplySettings("auto_equip_traversal=false\nauto_equip_traversal=true", settings) == 1);
    assert(settings.autoEquipTraversal);
    assert(ApplySettings("auto_equip_traversal=yes", settings) == 1 && settings.autoEquipTraversal);
    assert(!ApplySettings("auto_equip_traversal=false", settings) && !settings.autoEquipTraversal);
    assert(settings.autoVault);
    assert(!ApplySettings("auto_vault=true", settings) && settings.autoVault);
    assert(!ApplySettings("# inherit", settings) && settings.autoVault);
    assert(!ApplySettings("auto_vault=false", settings) && !settings.autoVault);
    for (bool prior : {false, true}) {
        settings.autoVault = prior;
        for (const auto *value : {"", "TRUE", "0", "1", "yes", "true # comment"}) {
            assert(ApplySettings(std::string("auto_vault=") + value, settings) == 1);
            assert(settings.autoVault == prior);
        }
        assert(ApplySettings("auto_vault=false\nauto_vault=true", settings) == 1);
        assert(settings.autoVault == prior);
        assert(ApplySettings("auto_vault=false\n\xff", settings) == 1 && settings.autoVault == prior);
        assert(ApplySettings(std::string("auto_vault=false\n") + std::string(4096, ' '), settings) == 1);
        assert(settings.autoVault == prior);
    }
    assert(settings.autoLadder);
    assert(!ApplySettings("auto_ladder=true", settings) && settings.autoLadder);
    assert(!ApplySettings("# inherit", settings) && settings.autoLadder);
    for (bool prior : {false, true}) {
        settings.autoLadder = prior;
        for (const auto *value : {"", "TRUE", "0", "1", "yes", "true # comment"}) {
            assert(ApplySettings(std::string("auto_ladder=") + value, settings) == 1);
            assert(settings.autoLadder == prior);
        }
        assert(ApplySettings("auto_ladder=false\nauto_ladder=true", settings) == 1);
        assert(settings.autoLadder == prior);
        assert(ApplySettings("auto_ladder=false\n\xff", settings) == 1 && settings.autoLadder == prior);
    }
    assert(!ApplySettings("auto_ladder=false\nauto_vault=true", settings));
    assert(!settings.autoLadder && settings.autoVault);
    assert(settings.holdToAdvanceDialogue && settings.holdToHoover);
    assert(!ApplySettings("enabled=true\nversion=0.4.15\nhold_to_advance_dialogue=false", settings));
    assert(!settings.holdToAdvanceDialogue);
    assert(!ApplySettings("# inherit\n", settings) && !settings.holdToAdvanceDialogue);
    assert(!ApplySettings("\xef\xbb\xbf ; comment\r\n hold_to_advance_dialogue = true\r\n", settings));
    assert(settings.holdToAdvanceDialogue);
    for (bool prior : {false, true}) {
        settings.holdToAdvanceDialogue = prior;
        for (const auto *value : {"", "TRUE", "0", "1", "yes", "true # comment", "false; comment"}) {
            assert(ApplySettings(std::string("hold_to_advance_dialogue=") + value, settings) == 1);
            assert(settings.holdToAdvanceDialogue == prior);
        }
        assert(ApplySettings("hold_to_advance_dialogue=false\nhold_to_advance_dialogue=true", settings) == 1);
        assert(settings.holdToAdvanceDialogue == prior);
        for (const auto &input : {std::string("hold_to_advance_dialogue=false\n\xff"),
                                 std::string("hold_to_advance_dialogue=true\n") + std::string(4096, ' ')}) {
            assert(ApplySettings(input, settings) == 1 && settings.holdToAdvanceDialogue == prior);
        }
        assert(ApplySettings("unknown=true\nbroken", settings) == 2);
    }
    for (bool prior : {false, true}) {
        settings.holdToHoover = prior;
        for (const auto *value : {"", "TRUE", "0", "1", "yes", "true # comment"}) {
            assert(ApplySettings(std::string("hold_to_hoover=") + value, settings) == 1);
            assert(settings.holdToHoover == prior);
        }
        assert(ApplySettings("hold_to_hoover=false\nhold_to_hoover=true", settings) == 1);
        assert(settings.holdToHoover == prior);
        assert(ApplySettings("hold_to_hoover=false\n\xff", settings) == 1);
        assert(settings.holdToHoover == prior);
        assert(!ApplySettings("# inherit", settings) && settings.holdToHoover == prior);
    }
    assert(!ApplySettings("hold_to_hoover=false\nhold_to_advance_dialogue=true", settings));
    assert(!settings.holdToHoover && settings.holdToAdvanceDialogue);
    assert(!ApplySettings("hold_to_hoover=true\nhold_to_advance_dialogue=false", settings));
    assert(settings.holdToHoover && !settings.holdToAdvanceDialogue);
    assert(ApplySettings("hold_to_hoover=false\nhold_to_hoover=true\nhold_to_advance_dialogue=true", settings) == 1);
    assert(settings.holdToHoover && settings.holdToAdvanceDialogue);
    {
        Settings settings{};
        assert(settings.autoSwim && settings.swimStrokeBoost);
        for (bool prior : {false, true}) {
            settings.autoSwim = prior;
            assert(!ApplySettings("# inherit", settings) && settings.autoSwim == prior);
            assert(ApplySettings("auto_swim=yes", settings) == 1 && settings.autoSwim == prior);
            assert(ApplySettings("auto_swim=false\nauto_swim=true", settings) == 1 && settings.autoSwim == prior);
            assert(ApplySettings("auto_swim=false\n\xff", settings) == 1 && settings.autoSwim == prior);
            assert(!ApplySettings("auto_swim=false", settings) && !settings.autoSwim);
            assert(!ApplySettings("auto_swim=true", settings) && settings.autoSwim);
        }
        assert(!ApplySettings("enabled=false\nversion=0.4.21\nauto_swim=false", settings));
        assert(!settings.autoSwim);
        assert(!ApplySettings("\xef\xbb\xbf ; comment\r\nauto_swim = true\r\n", settings));
        assert(settings.autoSwim);
        assert(ApplySettings(std::string(4097, ' '), settings) == 1 && settings.autoSwim);
        assert(ApplySettings("unknown=true\nbroken", settings) == 2);
        for (bool prior : {false, true}) {
            settings.swimStrokeBoost = prior;
            assert(!ApplySettings("# inherit", settings) && settings.swimStrokeBoost == prior);
            assert(ApplySettings("swim_stroke_boost=yes", settings) == 1 && settings.swimStrokeBoost == prior);
            assert(ApplySettings("swim_stroke_boost=false\nswim_stroke_boost=true", settings) == 1 && settings.swimStrokeBoost == prior);
            assert(!ApplySettings("swim_stroke_boost=false", settings) && !settings.swimStrokeBoost);
            assert(!ApplySettings("swim_stroke_boost=true", settings) && settings.swimStrokeBoost);
        }
        assert(ApplySettings("auto_swim=false\nauto_swim=true\nswim_stroke_boost=false", settings) == 1);
        assert(settings.autoSwim && !settings.swimStrokeBoost);
        assert(ApplySettings("auto_swim=false\nswim_stroke_boost=true\nswim_stroke_boost=false", settings) == 1);
        assert(!settings.autoSwim && !settings.swimStrokeBoost);
    }
    assert(ApplySettings("camera_tilt_while_swimming=false", settings) == 1); // Retired key.
    std::cout << "PASS: Controls default, inheritance, strict boolean, duplicate rollback and atomic invalid files\n";
}
