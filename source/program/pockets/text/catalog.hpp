#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace qol_pockets::text {
inline constexpr std::string_view GrabFiveKey = "optimized_ui.grab_five";
inline constexpr std::string_view GrabTenKey = "optimized_ui.grab_ten";
inline constexpr std::array<std::string_view, 16> Locales{
    "JPja", "USen", "USes", "USfr", "USpt", "EUen", "EUes", "EUfr",
    "EUde", "EUit", "EUpt", "EUnl", "EUru", "KRko", "CNzh", "TWzh"};
inline constexpr std::size_t MaxFileBytes = 16384;

// Plain UTF-8 only: no game control codes, escapes, or embedded newlines.
// Reject malformed encoding instead of passing arbitrary control data to ACNH.
inline bool Decode(std::string_view input, char16_t *output, std::size_t capacity) {
    std::size_t used = 0;
    for (std::size_t i = 0; i < input.size();) {
        auto first = static_cast<unsigned char>(input[i++]);
        std::uint32_t cp = first, minimum = 0;
        unsigned remaining = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            cp = first & 0x1f;
            remaining = 1;
            minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            cp = first & 0x0f;
            remaining = 2;
            minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            cp = first & 7;
            remaining = 3;
            minimum = 0x10000;
        } else if (first >= 0x80) {
            return false;
        }
        while (remaining--) {
            if (i == input.size()) {
                return false;
            }
            auto next = static_cast<unsigned char>(input[i++]);
            if ((next & 0xc0) != 0x80) {
                return false;
            }
            cp = (cp << 6) | (next & 0x3f);
        }
        if (cp < minimum || cp < 0x20 || (cp >= 0x7f && cp <= 0x9f) ||
            (cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff) {
            return false;
        }
        const std::size_t units = cp >= 0x10000 ? 2 : 1;
        if (used + units >= capacity) {
            return false;
        }
        if (units == 2) {
            cp -= 0x10000;
            output[used++] = static_cast<char16_t>(0xd800 + (cp >> 10));
            output[used++] = static_cast<char16_t>(0xdc00 + (cp & 0x3ff));
        } else {
            output[used++] = static_cast<char16_t>(cp);
        }
    }
    if (used == 0 || capacity == 0) {
        return false;
    }
    output[used] = 0;
    return true;
}

class Catalog {
    struct Entry {
        char key[64]{};
        char16_t value[128]{};
        unsigned locale{};
    };
    std::array<Entry, 64> entries{};
    std::size_t count = 0;
    static std::string_view Trim(std::string_view s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
            s.remove_prefix(1);
        }
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
            s.remove_suffix(1);
        }
        return s;
    }
    const char16_t *Find(std::string_view key, unsigned locale) const {
        for (std::size_t i = 0; i < count; ++i) {
            if (entries[i].locale == locale && key == entries[i].key) {
                return entries[i].value;
            }
        }
        return nullptr;
    }
    bool Parse(std::string_view file) {
        unsigned locale = Locales.size();
        if (file.starts_with("\xef\xbb\xbf")) {
            file.remove_prefix(3);
        }
        while (!file.empty()) {
            auto end = file.find('\n');
            auto line = Trim(file.substr(0, end));
            file = end == file.npos ? std::string_view{} : file.substr(end + 1);
            if (line.empty() || line.front() == '#' || line.front() == ';') {
                continue;
            }
            if (line.front() == '[') {
                if (line.back() != ']') {
                    return false;
                }
                auto name = line.substr(1, line.size() - 2);
                locale = 0;
                while (locale < Locales.size() && Locales[locale] != name) {
                    ++locale;
                }
                if (locale == Locales.size()) {
                    return false;
                }
                continue;
            }
            const auto equals = line.find('=');
            if (equals == line.npos || locale == Locales.size() || count == entries.size()) {
                return false;
            }
            auto key = Trim(line.substr(0, equals));
            auto value = Trim(line.substr(equals + 1));
            if (key.empty() || key.size() >= sizeof(Entry::key) || Find(key, locale)) {
                return false;
            }
            for (char c : key) {
                if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_')) {
                    return false;
                }
            }
            auto &entry = entries[count];
            if (!Decode(value, entry.value, std::size(entry.value))) {
                return false;
            }
            for (std::size_t i = 0; i < key.size(); ++i) {
                entry.key[i] = key[i];
            }
            entry.key[key.size()] = 0;
            entry.locale = locale;
            ++count;
        }
        return count != 0;
    }

  public:
    bool Load(std::string_view file) {
        count = 0;
        if (file.size() > MaxFileBytes || file.find('\0') != file.npos || !Parse(file)) {
            count = 0; // Never expose a partially parsed catalog.
            return false;
        }
        return true;
    }
    std::size_t Size() const {
        return count;
    }
    const char16_t *Text(std::string_view key, unsigned locale) const {
        if (const auto *value = Find(key, locale)) {
            return value;
        }
        if (const auto *value = Find(key, 1)) {
            return value; // US English fallback.
        }
        if (key == GrabFiveKey) {
            return u"Grab 5";
        }
        if (key == GrabTenKey) {
            return u"Grab 10";
        }
        // Retain the existing override key for compatibility with personal INIs.
        if (key == "qol_pockets.hold_wear") {
            return u"Use Item";
        }
        return nullptr;
    }
};

inline bool CopyCaption(char16_t *target, std::size_t capacity, const char16_t *text) {
    if (!target || !text) {
        return false;
    }
    std::size_t length = 0;
    while (text[length]) {
        ++length;
    }
    if (length >= capacity) {
        return false; // Preserve vanilla text; never truncate.
    }
    for (std::size_t i = 0; i <= length; ++i) {
        target[i] = text[i];
    }
    return true;
}
} // namespace qol_pockets::text
