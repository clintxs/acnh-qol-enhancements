#pragma once
#include "hook_registry.hpp"
#include "feature_activation.hpp"
#include "native/runtime.hpp"
#include <initializer_list>
namespace qol::core {
extern HookRegistry<> OwnedHooks;
template<class Hook>
HookSite Site(const char *name, std::uintptr_t offset, std::array<std::uint32_t, 5> original, bool enabled = true) {
    return {name, reinterpret_cast<std::uintptr_t>(&Hook::Callback), offset, original, enabled};
}
// All reservations precede all publication for this dependent hook group.
// Startup is synchronous; plans and registrations are never used on workers.
class HookPlan {
    struct Entry { HookSite site{}; HookRegistry<>::Token token{}; };
    std::array<Entry, 20> entries_{};
    std::size_t count_{};
    bool valid_ = true;
    void Fail(const HookSite &site, ClaimError error);
public:
    HookPlan(std::initializer_list<HookSite> sites);
    ~HookPlan();
    HookPlan(const HookPlan &) = delete;
    HookPlan &operator=(const HookPlan &) = delete;
    explicit operator bool() const { return valid_; }
    bool Contains(std::uintptr_t offset) const;
    template<class Hook> bool Publish(std::uintptr_t offset) {
        if (!valid_) return false;
        for (std::size_t i = 0; i < count_; ++i) {
            auto &entry = entries_[i];
            if (entry.site.offset != offset || entry.site.callback != reinterpret_cast<std::uintptr_t>(&Hook::Callback)) continue;
            const auto view = native::MainExecutable();
            if (const auto error = OwnedHooks.Verify(entry.token, view); error != ClaimError::None) {
                Fail(entry.site, error);
                return false;
            }
            if (OwnedHooks.Installed(entry.token)) return true;
            // Exlaunch pool allocation/relocation aborts on failure. Do not claim
            // recoverable rollback. Track attempted publication conservatively.
            RecordMutation();
            Hook::InstallAtOffset(offset);
            const auto bytes = view.Check(native::Fingerprint<>{offset, entry.site.original, entry.site.owner});
            if (!bytes.actualReadable || OwnedHooks.Published(entry.token, bytes.actual) != ClaimError::None) {
                Fail(entry.site, ClaimError::Invalid);
                return false;
            }
            return true;
        }
        ReportActivationFailure(FeatureState::Failed, "hook not prepared by this plan");
        return false;
    }
};
} // namespace qol::core
