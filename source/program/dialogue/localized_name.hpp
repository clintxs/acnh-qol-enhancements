#pragma once
#include <cstdint>
#include <span>

namespace qol::dialogue {
inline constexpr unsigned NpcNameBytes = 96, MaxLabelUnits = 128;

// The native name holder supplies UTF-16 data and a capacity, not a length.
// MSBT tag parameters can contain zero units, so skip tags before looking for
// the final terminator. Russian names contain trailing declension metadata.
// Copy only complete names; invalid/oversized data must never match a prefix.
inline bool CopyLabelName(std::span<const char16_t> input, std::span<char> output) {
    if (output.empty()) return false;
    output[0] = 0;
    if (input.size() > MaxLabelUnits) return false;
    std::size_t used = 0;
    for (std::size_t cursor = 0; cursor < input.size();) {
        std::uint32_t cp = input[cursor++];
        if (!cp) { output[used] = 0; return used != 0; }
        if (cp == 0xe) {
            if (input.size() - cursor < 3) break;
            const unsigned bytes = input[cursor + 2];
            const auto units = 3 + bytes / 2;
            if ((bytes & 1) || units > input.size() - cursor) break;
            cursor += units;
            continue;
        }
        if (cp == 0xf) {
            if (input.size() - cursor < 2) break;
            cursor += 2;
            continue;
        }
        if (cp >= 0xd800 && cp <= 0xdbff) {
            if (cursor == input.size() || input[cursor] < 0xdc00 || input[cursor] > 0xdfff) break;
            cp = 0x10000 + ((cp - 0xd800) << 10) + (input[cursor++] - 0xdc00);
        } else if (cp >= 0xdc00 && cp <= 0xdfff) break;
        if (cp < 0x20 || (cp >= 0x7f && cp <= 0x9f)) break;
        const unsigned bytes = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
        if (used + bytes >= output.size()) break;
        if (bytes == 1) output[used++] = static_cast<char>(cp);
        else {
            const auto start = used;
            used += bytes;
            for (unsigned i = bytes - 1; i; --i) {
                output[start + i] = static_cast<char>(0x80 | (cp & 0x3f));
                cp >>= 6;
            }
            output[start] = static_cast<char>((bytes == 2 ? 0xc0 : bytes == 3 ? 0xe0 : 0xf0) | cp);
        }
    }
    output[0] = 0;
    return false;
}
} // namespace qol::dialogue
