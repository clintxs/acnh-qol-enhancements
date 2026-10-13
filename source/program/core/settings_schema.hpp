#pragma once
#include "config.hpp"
#include <array>

namespace qol::core::settings {
enum class Overflow { Reject, Clamp };
enum class LeadingZeros { Allow, Reject };
struct NumericPolicy {
    unsigned minimum;
    unsigned maximum;
    Overflow overflow;
    LeadingZeros leadingZeros;
};

inline bool ParseUnsigned(std::string_view value, unsigned &output, NumericPolicy policy) {
    if (value.empty() || (policy.leadingZeros == LeadingZeros::Reject &&
                         value.size() > 1 && value.front() == '0')) return false;
    unsigned parsed = 0;
    for (char character : value) {
        if (character < '0' || character > '9') return false;
        const unsigned digit = character - '0';
        // Compare before multiplying, including when maximum is UINT_MAX.
        if (digit > policy.maximum || parsed > (policy.maximum - digit) / 10) {
            if (policy.overflow == Overflow::Reject) return false;
            parsed = policy.maximum;
        } else {
            parsed = parsed * 10 + digit;
        }
    }
    if (parsed < policy.minimum) return false;
    output = parsed;
    return true;
}

template <class Settings> struct Field {
    std::string_view name;
    unsigned (*apply)(std::string_view, Settings &);
    void (*restore)(Settings &, const Settings &);
};

template <auto Member, auto... Rest, class Settings>
constexpr decltype(auto) Destination(Settings &settings) {
    if constexpr (sizeof...(Rest) == 0) return (settings.*Member);
    else return Destination<Rest...>(settings.*Member);
}

template <class Settings, auto... Members>
constexpr Field<Settings> Boolean(std::string_view name) {
    return {name,
        [](std::string_view value, Settings &output) -> unsigned {
            if (value != "true" && value != "false") return 1;
            Destination<Members...>(output) = value == "true";
            return 0;
        },
        [](Settings &output, const Settings &previous) {
            Destination<Members...>(output) = Destination<Members...>(previous);
        }};
}

template <class Settings, auto Member, auto Parse>
constexpr Field<Settings> Number(std::string_view name) {
    return {name,
        [](std::string_view value, Settings &output) -> unsigned {
            return Parse(value, output.*Member) ? 0 : 1;
        },
        [](Settings &output, const Settings &previous) { output.*Member = previous.*Member; }};
}

template <class Settings, size_t Count>
constexpr bool ValidSchema(const std::array<Field<Settings>, Count> &fields) {
    for (size_t i = 0; i < Count; ++i) {
        if (fields[i].name.empty() || !fields[i].apply || !fields[i].restore) return false;
        for (size_t j = 0; j < i; ++j)
            if (fields[i].name == fields[j].name) return false;
    }
    return true;
}

enum class Metadata { Module, None };
// Preferences inherit from the previous file. Metadata activation remains the
// loader's responsibility; Core has no installation marker and skips no keys.
template <class Settings, size_t Count>
inline unsigned Apply(std::string_view input, Settings &output,
                      const std::array<Field<Settings>, Count> &fields,
                      Metadata metadata = Metadata::Module) {
    if (input.size() > MaxConfigBytes || !ValidUtf8(input, true)) return 1;
    if (input.starts_with("\xef\xbb\xbf")) input.remove_prefix(3);
    const auto previous = output;
    std::array<bool, Count> seen{}, duplicate{};
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
        if (metadata == Metadata::Module && (key == "enabled" || key == "version")) continue;
        size_t index = 0;
        while (index < Count && fields[index].name != key) ++index;
        if (index == Count) { ++warnings; continue; }
        if (seen[index]) { duplicate[index] = true; ++warnings; continue; }
        // Invalid first occurrences still count: a later value cannot repair a
        // duplicate. Domain parsers may return multiple entry-level warnings.
        seen[index] = true;
        warnings += fields[index].apply(value, output);
    }
    for (size_t i = 0; i < Count; ++i)
        if (duplicate[i]) fields[i].restore(output, previous);
    return warnings;
}
} // namespace qol::core::settings
