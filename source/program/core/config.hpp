#pragma once
#include <compare>
#include <cstdint>
#include <string_view>

namespace qol::core {
struct Version {
    std::uint16_t major{}, minor{}, patch{};
    constexpr auto operator<=>(const Version &) const = default;
};
inline constexpr std::size_t MaxConfigBytes = 4096;
struct Config { bool enabled{}; Version version{}; };

constexpr std::string_view Trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.remove_suffix(1);
    return value;
}

// Numeric comparisons: 1.10.0 is newer than 1.9.0. No suffixes or coercion.
constexpr bool ParseVersion(std::string_view value, Version &output) {
    std::uint16_t parts[3]{};
    for (unsigned index = 0; index < 3; ++index) {
        const auto dot = value.find('.');
        const auto part = value.substr(0, dot);
        if (part.empty() || (part.size() > 1 && part.front() == '0')) return false;
        unsigned number = 0;
        for (char character : part) {
            if (character < '0' || character > '9') return false;
            number = number * 10 + static_cast<unsigned>(character - '0');
            if (number > 65535) return false;
        }
        parts[index] = static_cast<std::uint16_t>(number);
        if (index < 2) {
            if (dot == std::string_view::npos) return false;
            value.remove_prefix(dot + 1);
        } else if (dot != std::string_view::npos) return false;
    }
    output = {parts[0], parts[1], parts[2]};
    return true;
}

inline bool ParseConfig(std::string_view input, Config &output) {
    if (input.size() > MaxConfigBytes) return false;
    if (input.starts_with("\xef\xbb\xbf")) input.remove_prefix(3);
    Config parsed{};
    bool hasEnabled = false, hasVersion = false;
    for (char character : input) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte >= 127 || (byte < 32 && character != '\n' && character != '\r' && character != '\t')) return false;
    }
    while (!input.empty()) {
        const auto newline = input.find('\n');
        const auto line = Trim(input.substr(0, newline));
        input = newline == std::string_view::npos ? std::string_view{} : input.substr(newline + 1);
        if (line.empty() || line.front() == '#' || line.front() == ';') continue;
        const auto equals = line.find('=');
        if (equals == std::string_view::npos) return false;
        const auto key = Trim(line.substr(0, equals));
        const auto value = Trim(line.substr(equals + 1));
        if (key == "enabled") {
            if (hasEnabled || (value != "true" && value != "false")) return false;
            hasEnabled = true;
            parsed.enabled = value == "true";
        } else if (key == "version") {
            if (hasVersion || !ParseVersion(value, parsed.version)) return false;
            hasVersion = true;
        } else return false; // Reject typos and unimplemented settings.
    }
    if (!hasEnabled || !hasVersion) return false;
    output = parsed;
    return true;
}

enum class Compatibility { TooOld, Supported, NewerThanCore };
constexpr Compatibility CheckCompatibility(Version installed, Version minimum, Version core) {
    if (installed < minimum) return Compatibility::TooOld;
    return installed > core ? Compatibility::NewerThanCore : Compatibility::Supported;
}
} // namespace qol::core
