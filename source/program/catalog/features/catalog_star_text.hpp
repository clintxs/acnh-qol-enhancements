#pragma once
#include <cstddef>
#include <cstdint>

namespace catalog_aware_shops::features {
// Native item names retain a leading MSBT noun-metadata tag, including embedded
// zero units. Preserve that verified tag byte-for-byte and mark the visible name.
// Other control/tag formats (including ruby) remain unchanged in this prototype.
template <class Capitalize>
inline bool PrefixCatalogStar(char16_t *text, std::size_t capacity, Capitalize capitalize,
                              char16_t marker = u'★') {
    constexpr std::size_t NativeCapacityUnits = 128;
    constexpr char16_t StartControlTag = 0x000e, NounMetadataGroup = 0x0032;
    constexpr std::size_t NounMetadataUnits = 6;
    constexpr std::size_t MarkerAndTerminatorUnits = 3; // star + space + NUL
    if (!text || capacity != NativeCapacityUnits || (marker != u'★' && marker != u'☆')) {
        return false;
    }
    std::size_t visibleNameStart = 0;
    if (text[0] == StartControlTag) {
        // Start tag, group 50, tag 0, four payload bytes: six UTF-16 units total.
        if (text[1] != NounMetadataGroup || text[2] != 0 || text[3] != 4) {
            return false;
        }
        visibleNameStart = NounMetadataUnits;
    }
    if (!text[visibleNameStart] ||
        (text[visibleNameStart] == u'★' || text[visibleNameStart] == u'☆')) {
        return false;
    }
    std::size_t terminatorIndex = visibleNameStart;
    while (terminatorIndex < capacity && text[terminatorIndex]) {
        if (text[terminatorIndex] < 0x20) {
            return false;
        }
        ++terminatorIndex;
    }
    if (terminatorIndex + MarkerAndTerminatorUnits > capacity) {
        return false;
    }
    // The star consumes the formatter's initial-character capitalization. Apply
    // the same native case rules to the original name before adding the marker.
    capitalize(text + visibleNameStart);
    // Include the terminator; the metadata prefix is outside the moved range.
    for (std::size_t i = terminatorIndex + 1; i > visibleNameStart; --i) {
        text[i + 1] = text[i - 1];
    }
    text[visibleNameStart] = marker;
    text[visibleNameStart + 1] = u' ';
    return true;
}
} // namespace catalog_aware_shops::features
