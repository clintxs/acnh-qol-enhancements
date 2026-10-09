#pragma once
#include <cstdint>
#include <string_view>

namespace qol::core {
// Strict Unicode scalar decoding: reject overlong sequences, surrogates,
// truncated input and values beyond U+10FFFF.
inline bool NextUtf8(std::string_view input, std::size_t &cursor, std::uint32_t &codepoint) {
    if (cursor == input.size()) return false;
    const auto first = static_cast<unsigned char>(input[cursor++]);
    codepoint = first;
    unsigned remaining = 0;
    std::uint32_t minimum = 0;
    if (first >= 0xc2 && first <= 0xdf) { codepoint &= 0x1f; remaining = 1; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { codepoint &= 0xf; remaining = 2; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { codepoint &= 7; remaining = 3; minimum = 0x10000; }
    else if (first >= 0x80) return false;
    while (remaining--) {
        if (cursor == input.size()) return false;
        const auto next = static_cast<unsigned char>(input[cursor++]);
        if ((next & 0xc0) != 0x80) return false;
        codepoint = (codepoint << 6) | (next & 0x3f);
    }
    return codepoint >= minimum && codepoint <= 0x10ffff &&
           !(codepoint >= 0xd800 && codepoint <= 0xdfff);
}
inline bool ValidUtf8(std::string_view input, bool allowLines = false) {
    std::size_t cursor = 0;
    while (cursor < input.size()) {
        std::uint32_t cp;
        if (!NextUtf8(input, cursor, cp)) return false;
        if (allowLines && (cp == '\n' || cp == '\r' || cp == '\t')) continue;
        if (cp < 0x20 || (cp >= 0x7f && cp <= 0x9f)) return false;
    }
    return true;
}
} // namespace qol::core
