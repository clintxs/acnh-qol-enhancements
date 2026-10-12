#pragma once
#include "program/core/config.hpp"
namespace qol::swimming {
struct Settings { bool autoSwim = true; };
inline unsigned ApplySettings(std::string_view input, Settings &output) {
    if (input.size() > core::MaxConfigBytes || !core::ValidUtf8(input, true)) return 1;
    const auto previous = output;
    if (input.starts_with("\xef\xbb\xbf")) input.remove_prefix(3);
    unsigned warnings = 0;
    bool seen = false, duplicate = false;
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
        if (key != "auto_swim") { ++warnings; continue; }
        if (seen) { duplicate = true; ++warnings; continue; }
        seen = true;
        if (value == "true" || value == "false") output.autoSwim = value == "true";
        else ++warnings;
    }
    if (duplicate) output = previous;
    return warnings;
}
unsigned Configure(std::string_view input);
void ReportSettings();
bool Install();
}
