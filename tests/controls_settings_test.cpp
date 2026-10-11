#include "program/controls/settings.hpp"
#include <cassert>
#include <iostream>
#include <string>
using namespace qol::controls;
int main() {
    Settings settings{};
    assert(settings.holdToAdvanceDialogue);
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
    std::cout << "PASS: Controls default, inheritance, strict boolean, duplicate rollback and atomic invalid files\n";
}
