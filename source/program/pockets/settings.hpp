#pragma once
#include "program/core/config.hpp"

namespace qol_pockets {
struct Settings {
    bool bypassMenuOpenContainers = true;
    bool bypassMenuClaimRecycleBox = true;
    bool bypassMenuWithdrawHomeStorage = true;
    bool enableMultiselect = true;
};
// Omitted/invalid keys inherit prior values. Duplicates restore the value
// from before this file, matching the other modules' custom-file policy.
inline unsigned ApplySettings(std::string_view input, Settings &output) {
    namespace core = qol::core;
    if (input.size() > core::MaxConfigBytes || !core::ValidUtf8(input, true)) return 1;
    const auto previous = output;
    if (input.starts_with("\xef\xbb\xbf")) input.remove_prefix(3);
    unsigned warnings = 0, seen = 0, duplicate = 0;
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
        const unsigned bit = key == "bypass_menu_open_containers" ? 1 :
                             key == "bypass_menu_claim_recycle_box" ? 2 :
                             key == "bypass_menu_withdraw_home_storage" ? 4 :
                             key == "enable_multiselect" ? 8 : 0;
        if (!bit) { ++warnings; continue; }
        if (seen & bit) { duplicate |= bit; ++warnings; continue; }
        seen |= bit;
        bool &setting = bit == 1 ? output.bypassMenuOpenContainers :
                        bit == 2 ? output.bypassMenuClaimRecycleBox :
                        bit == 4 ? output.bypassMenuWithdrawHomeStorage : output.enableMultiselect;
        if (value == "true" || value == "false") setting = value == "true";
        else ++warnings;
    }
    if (duplicate & 1) output.bypassMenuOpenContainers = previous.bypassMenuOpenContainers;
    if (duplicate & 2) output.bypassMenuClaimRecycleBox = previous.bypassMenuClaimRecycleBox;
    if (duplicate & 4) output.bypassMenuWithdrawHomeStorage = previous.bypassMenuWithdrawHomeStorage;
    if (duplicate & 8) output.enableMultiselect = previous.enableMultiselect;
    return warnings;
}
unsigned Configure(std::string_view input);
void ReportSettings();
const Settings &GetSettings();
}
