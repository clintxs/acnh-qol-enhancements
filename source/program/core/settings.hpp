#pragma once
#include "config.hpp"
#include "logging_policy.hpp"

namespace qol::core {
// Native BC44B0..BC44BC decrements an unsigned halfword, without saturation.
// Zero means leave the native initialization and countdown entirely untouched.
inline constexpr unsigned MaxAmiiboTimeoutFrames = 65535;
struct Settings {
    unsigned amiiboTimeoutFrames = 9000;
    logging::Preferences logging{};
};

inline bool ParseAmiiboTimeout(std::string_view value, unsigned &output) {
    if (value.empty()) return false;
    unsigned result = 0;
    for (char c : value) {
        if (c < '0' || c > '9') return false;
        const auto digit = static_cast<unsigned>(c - '0');
        if (result > (MaxAmiiboTimeoutFrames - digit) / 10) return false;
        result = result * 10 + digit;
    }
    output = result;
    return true;
}

// Core already exists in the executable: both files are optional settings
// overlays, not installation markers, and require no enabled/version metadata.
// Match feature preference behavior: bad values and duplicates retain the
// value from before this file. Whole-file encoding/size errors are atomic.
inline unsigned ApplyCoreSettings(std::string_view input, Settings &output) {
    if (input.size() > MaxConfigBytes || !ValidUtf8(input, true)) return 1;
    if (input.starts_with("\xef\xbb\xbf")) input.remove_prefix(3);
    const auto previous = output;
    unsigned seen = 0, duplicate = 0;
    unsigned warnings = 0;
    while (!input.empty()) {
        const auto newline = input.find('\n');
        const auto line = Trim(input.substr(0, newline));
        input = newline == std::string_view::npos ? std::string_view{} : input.substr(newline + 1);
        if (line.empty() || line.front() == '#' || line.front() == ';') continue;
        const auto equals = line.find('=');
        if (equals == std::string_view::npos) { ++warnings; continue; }
        const auto key = Trim(line.substr(0, equals));
        const auto value = Trim(line.substr(equals + 1));
        const unsigned bit = key == "amiibo_timeout_frames" ? 1 : key == "log_versions" ? 2 :
                             key == "log_settings" ? 4 : key == "log_debug" ? 8 : 0;
        if (!bit) { ++warnings; continue; }
        if (seen & bit) { duplicate |= bit; ++warnings; continue; }
        seen |= bit;
        if (bit == 1) {
            unsigned frames;
            if (ParseAmiiboTimeout(value, frames)) output.amiiboTimeoutFrames = frames;
            else ++warnings;
        } else if (value == "true" || value == "false") {
            (bit == 2 ? output.logging.versions : bit == 4 ? output.logging.settings :
                        output.logging.debug) = value == "true";
        } else ++warnings;
    }
    if (duplicate & 1) output.amiiboTimeoutFrames = previous.amiiboTimeoutFrames;
    if (duplicate & 2) output.logging.versions = previous.logging.versions;
    if (duplicate & 4) output.logging.settings = previous.logging.settings;
    if (duplicate & 8) output.logging.debug = previous.logging.debug;
    return warnings;
}

bool InstallAmiiboTimeout(unsigned frames);
} // namespace qol::core
