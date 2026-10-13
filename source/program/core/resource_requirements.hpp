#pragma once
#include "registration.hpp"
#include <array>
#include <cstdint>
#include <string_view>
namespace qol::core {
template<unsigned Capacity = 4> class ResourceRequirements {
    struct Entry { std::array<char, 96> path{}; std::uint32_t bytes{}; };
    std::array<Entry, Capacity> entries_{};
public:
    RegistrationResult CanRequire(std::string_view path, std::uint32_t minimum) const {
        if (path.empty() || path.size() >= 96 || path.find('\0') != path.npos || !minimum)
            return {RegistrationStatus::Invalid};
        bool free = false;
        for (const auto &entry : entries_) {
            if (entry.bytes && path == entry.path.data())
                return {minimum <= entry.bytes ? RegistrationStatus::Identical : RegistrationStatus::Ready};
            free |= !entry.bytes;
        }
        return {free ? RegistrationStatus::Ready : RegistrationStatus::Capacity};
    }
    RegistrationResult Require(std::string_view path, std::uint32_t minimum) {
        const auto check = CanRequire(path, minimum);
        if (!check || check.status == RegistrationStatus::Identical) return check;
        for (auto &entry : entries_) if (entry.bytes && path == entry.path.data()) {
            entry.bytes = minimum; return check;
        }
        for (auto &entry : entries_) if (!entry.bytes) {
            for (unsigned i = 0; i < path.size(); ++i) entry.path[i] = path[i];
            entry.bytes = minimum; return check;
        }
        return {RegistrationStatus::Capacity};
    }
    std::uint32_t MinimumFor(std::string_view path) const {
        for (const auto &entry : entries_) if (entry.bytes && path == entry.path.data()) return entry.bytes;
        return 0;
    }
};
}
