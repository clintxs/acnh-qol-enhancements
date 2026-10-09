#pragma once
#include "config.hpp"

namespace qol::core {
// Native BC44B0..BC44BC decrements an unsigned halfword, without saturation.
// Zero means leave the native initialization and countdown entirely untouched.
inline constexpr unsigned MaxAmiiboTimeoutFrames = 65535;
struct Settings { unsigned amiiboTimeoutFrames = 9000; };

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
    bool seenTimeout = false, duplicateTimeout = false;
    unsigned warnings = 0;
    while (!input.empty()) {
        const auto newline = input.find('\n');
        const auto line = Trim(input.substr(0, newline));
        input = newline == std::string_view::npos ? std::string_view{} : input.substr(newline + 1);
        if (line.empty() || line.front() == '#' || line.front() == ';') continue;
        const auto equals = line.find('=');
        if (equals == std::string_view::npos || Trim(line.substr(0, equals)) != "amiibo_timeout_frames") {
            ++warnings;
            continue;
        }
        if (seenTimeout) {
            duplicateTimeout = true;
            ++warnings;
            continue;
        }
        seenTimeout = true;
        unsigned frames;
        if (ParseAmiiboTimeout(Trim(line.substr(equals + 1)), frames)) output.amiiboTimeoutFrames = frames;
        else ++warnings;
    }
    if (duplicateTimeout) output.amiiboTimeoutFrames = previous.amiiboTimeoutFrames;
    return warnings;
}

bool InstallAmiiboTimeout(unsigned frames);
} // namespace qol::core
