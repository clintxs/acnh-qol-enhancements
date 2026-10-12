#pragma once
#include "program/core/config.hpp"
namespace catalog_aware_shops {
struct Settings { bool artDonationStars = true; };
inline unsigned ApplySettings(std::string_view input, Settings &output) {
    namespace core = qol::core;
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
        const unsigned bit = key == "art_donation_stars" ? 1 : 0;
        if (!bit) { ++warnings; continue; }
        if (seen & bit) { duplicate |= bit; ++warnings; continue; }
        seen |= bit;
        bool &setting = output.artDonationStars;
        if (value == "true" || value == "false") setting = value == "true";
        else ++warnings;
    }
    if (duplicate & 1) output.artDonationStars = previous.artDonationStars;
    return warnings;
}
unsigned Configure(std::string_view input);
void ReportSettings();
const Settings &GetSettings();
}
