#pragma once
#include "program/core/config.hpp"

namespace qol::crafting {
inline constexpr unsigned MaximumCraftingSpeed = 30;

inline bool ParseCraftingSpeed(std::string_view value, unsigned &output) {
    // Saturate while parsing so even a very large decimal cannot overflow.
    // Validate every character before publishing; malformed values inherit.
    if (value.empty() || value.front() == '0') return false;
    unsigned speed = 0;
    for (const char digit : value) {
        if (digit < '0' || digit > '9') return false;
        const unsigned next = speed * 10 + (digit - '0');
        speed = next > MaximumCraftingSpeed ? MaximumCraftingSpeed : next;
    }
    output = speed;
    return true;
}

struct Settings {
    bool diyAppPreview = true;
    bool workbenchPreviewTilt = true;
    bool craftingQuantityWrap = true;
    unsigned craftingSpeed = 3;
};

// Same layering rules as other modules: omitted/invalid values inherit, and
// duplicate keys restore the value from before this file. Encoding/size failures
// reject the whole overlay. Core validates enabled/version before calling this.
inline unsigned ApplySettings(std::string_view input, Settings &output) {
    if (input.size() > core::MaxConfigBytes || !core::ValidUtf8(input, true)) return 1;
    if (input.starts_with("\xef\xbb\xbf")) input.remove_prefix(3);
    const auto previous = output;
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
        const unsigned bit = key == "diy_app_preview" ? 1 : key == "workbench_preview_tilt" ? 2 :
                             key == "crafting_quantity_wrap" ? 4 : key == "crafting_speed" ? 8 : 0;
        if (!bit) { ++warnings; continue; }
        if (seen & bit) { duplicate |= bit; ++warnings; continue; }
        seen |= bit;
        if (bit == 8) {
            if (!ParseCraftingSpeed(value, output.craftingSpeed)) ++warnings;
        } else if (value == "true" || value == "false") {
            (bit == 1 ? output.diyAppPreview : bit == 2 ? output.workbenchPreviewTilt :
                        output.craftingQuantityWrap) = value == "true";
        } else ++warnings;
    }
    if (duplicate & 1) output.diyAppPreview = previous.diyAppPreview;
    if (duplicate & 2) output.workbenchPreviewTilt = previous.workbenchPreviewTilt;
    if (duplicate & 4) output.craftingQuantityWrap = previous.craftingQuantityWrap;
    if (duplicate & 8) output.craftingSpeed = previous.craftingSpeed;
    return warnings;
}

unsigned Configure(std::string_view input);
bool Install();
bool InstallFasterCrafting(unsigned maximumSpeed);
bool InstallCraftingQuantityWrap();
bool InstallCraftingQuantityShortcut();
} // namespace qol::crafting
