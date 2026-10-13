#pragma once
#include "native/executable_view.hpp"
#include <string_view>

namespace qol::core {
enum class ClaimError { None, Unsupported, Conflict, Capacity, Changed, Invalid };
struct HookSite {
    const char *owner;
    std::uintptr_t callback, offset;
    std::array<std::uint32_t, 5> original;
    bool enabled = true;
};
// Reserve the maximum Exlaunch patch span (20 bytes), even when a near branch
// needs only four. No allocation, runtime unregistering or foreign-hook adoption.
template<std::size_t Capacity = 160> class HookRegistry {
public:
    struct Token { std::size_t slot = Capacity; unsigned generation{}; };
    struct Reservation { Token token{}; ClaimError error{}; explicit operator bool() const { return error == ClaimError::None; } };
private:
    struct Claim {
        HookSite site{};
        std::array<std::uint32_t, 5> published{};
        unsigned generation{}, reservations{};
        bool occupied{}, installed{};
    };
    std::array<Claim, Capacity> claims_{};
    unsigned generation_{};
    Claim *Find(Token token) {
        if (token.slot >= Capacity) return nullptr;
        auto &claim = claims_[token.slot];
        return claim.occupied && claim.generation == token.generation ? &claim : nullptr;
    }
    static bool Same(const HookSite &a, const HookSite &b) {
        return a.callback == b.callback && a.offset == b.offset && a.original == b.original &&
               std::string_view(a.owner) == b.owner;
    }
public:
    bool Contains(std::uintptr_t offset) const {
        for (const auto &claim : claims_) if (claim.occupied && claim.site.offset == offset) return true;
        return false;
    }
    Reservation Prepare(const HookSite &site, const native::ExecutableView &text) {
        if (!site.owner || !site.callback) return {{}, ClaimError::Invalid};
        std::size_t free = Capacity;
        for (std::size_t i = 0; i < Capacity; ++i) {
            auto &claim = claims_[i];
            if (!claim.occupied) { if (free == Capacity) free = i; continue; }
            const auto a = claim.site.offset, b = site.offset;
            if ((a <= b ? b - a : a - b) >= 20) continue;
            if (!Same(claim.site, site)) return {{}, ClaimError::Conflict};
            const auto &expected = claim.installed ? claim.published : claim.site.original;
            if (!text.Check(native::Fingerprint<>{site.offset, expected, site.owner}))
                return {{}, ClaimError::Changed};
            ++claim.reservations;
            return {{i, claim.generation}, ClaimError::None};
        }
        if (!text.Check(native::Fingerprint<>{site.offset, site.original, site.owner}))
            return {{}, ClaimError::Unsupported};
        if (free == Capacity) return {{}, ClaimError::Capacity};
        auto &claim = claims_[free];
        claim = {.site = site, .generation = ++generation_, .reservations = 1, .occupied = true};
        return {{free, claim.generation}, ClaimError::None};
    }
    ClaimError Verify(Token token, const native::ExecutableView &text) {
        auto *claim = Find(token);
        if (!claim) return ClaimError::Invalid;
        const auto &expected = claim->installed ? claim->published : claim->site.original;
        return text.Check(native::Fingerprint<>{claim->site.offset, expected, claim->site.owner})
            ? ClaimError::None : ClaimError::Changed;
    }
    bool Installed(Token token) { const auto *claim = Find(token); return claim && claim->installed; }
    // Called only after Exlaunch returns. Its allocator/relocator may abort the
    // process; this is not a rollback guarantee or a recoverable transaction.
    ClaimError Published(Token token, const std::array<std::uint32_t, 5> &bytes) {
        auto *claim = Find(token);
        if (!claim || claim->installed || bytes == claim->site.original) return ClaimError::Invalid;
        claim->published = bytes;
        claim->installed = true;
        return ClaimError::None;
    }
    void Release(Token token) {
        auto *claim = Find(token);
        if (!claim || !claim->reservations) return;
        if (--claim->reservations == 0 && !claim->installed) claim->occupied = false;
    }
    bool Owned(const HookSite &site, const native::ExecutableView &text) {
        auto reservation = Prepare(site, text);
        if (!reservation) return false;
        const bool installed = Installed(reservation.token);
        Release(reservation.token);
        return installed;
    }
};
} // namespace qol::core
