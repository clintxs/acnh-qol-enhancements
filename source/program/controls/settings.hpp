#pragma once
#include "program/core/config.hpp"
namespace qol::controls {
struct Settings {
    bool holdToHoover = true;
    bool holdToAdvanceDialogue = true;
    bool autoVault = false;
    bool autoLadder = false; // Equipped-ladder prototype; opt in for live testing.
};
inline unsigned ApplySettings(std::string_view input, Settings &output) {
    if (input.size() > core::MaxConfigBytes || !core::ValidUtf8(input, true)) return 1;
    const auto previous = output;
    if (input.starts_with("\xef\xbb\xbf")) input.remove_prefix(3);
    unsigned warnings = 0;
    unsigned seen = 0, duplicate = 0;
    while (!input.empty()) {
        const auto end = input.find('\n');
        const auto line = core::Trim(input.substr(0, end));
        input = end == std::string_view::npos ? std::string_view{} : input.substr(end + 1);
        if (line.empty() || line.front() == '#' || line.front() == ';') continue;
        const auto equals = line.find('=');
        if (equals == std::string_view::npos) { ++warnings; continue; }
        const auto key = core::Trim(line.substr(0, equals));
        const auto value = core::Trim(line.substr(equals + 1));
        if (key == "enabled" || key == "version") continue;
        const unsigned bit = key == "hold_to_hoover" ? 1 : key == "hold_to_advance_dialogue" ? 2 : key == "auto_vault" ? 4 : key == "auto_ladder" ? 8 : 0;
        if (!bit) { ++warnings; continue; }
        if (seen & bit) { duplicate |= bit; ++warnings; continue; }
        seen |= bit;
        if (value == "true" || value == "false")
            (bit == 1 ? output.holdToHoover : bit == 2 ? output.holdToAdvanceDialogue : bit == 4 ? output.autoVault : output.autoLadder) = value == "true";
        else ++warnings;
    }
    if (duplicate & 1) output.holdToHoover = previous.holdToHoover;
    if (duplicate & 2) output.holdToAdvanceDialogue = previous.holdToAdvanceDialogue;
    if (duplicate & 4) output.autoVault = previous.autoVault;
    if (duplicate & 8) output.autoLadder = previous.autoLadder;
    return warnings;
}
unsigned Configure(std::string_view input);
void ReportSettings();
bool Install();
}
