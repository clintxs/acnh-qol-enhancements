#include "hook_plan.hpp"
#include "native/speedfix.hpp"
namespace qol::core {
// 160 claims fit below Exlaunch's 163 200-byte trampoline slots. Every program
// hook goes through a plan; inline entries have a larger independent capacity.
static_assert(160 < exl::setting::JitSize / 200);
HookRegistry<> OwnedHooks{};
HookPlan::HookPlan(std::initializer_list<HookSite> sites) {
    for (auto site : sites) {
        if (!site.enabled) continue;
        site.original = native::CompatibleFingerprint(native::MainExecutable(),
            native::Fingerprint<>{site.offset, site.original, site.owner}).words;
        if (count_ == entries_.size()) { Fail(site, ClaimError::Capacity); break; }
        const auto reservation = OwnedHooks.Prepare(site, native::MainExecutable());
        if (!reservation) { Fail(site, reservation.error); break; }
        entries_[count_++] = {site, reservation.token};
    }
}
HookPlan::~HookPlan() {
    for (std::size_t i = 0; i < count_; ++i) OwnedHooks.Release(entries_[i].token);
}
bool HookPlan::Contains(std::uintptr_t offset) const {
    if (!valid_) return false;
    for (std::size_t i = 0; i < count_; ++i) if (entries_[i].site.offset == offset) return true;
    return false;
}
void HookPlan::Fail(const HookSite &site, ClaimError error) {
    valid_ = false;
    if (error == ClaimError::Unsupported)
        native::ReportMismatch(native::MainExecutable().Check(native::Fingerprint<>{site.offset, site.original, site.owner}));
    const char *reason = error == ClaimError::Unsupported ? "original instructions unavailable/mismatched" :
        error == ClaimError::Conflict ? "incompatible owner or overlapping patch" :
        error == ClaimError::Capacity ? "hook claim capacity" :
        error == ClaimError::Changed ? "owned/reserved instructions changed" : "publication failed";
    ReportActivationFailure(error == ClaimError::Unsupported || error == ClaimError::Conflict || error == ClaimError::Changed
        ? FeatureState::Unsupported : error == ClaimError::Capacity ? FeatureState::DependencyUnavailable : FeatureState::Failed, reason);
    Logging.Error("[QoL Enhancements] hook=%s main+0x%lx rejected: %s\n", site.owner, site.offset, reason);
}
} // namespace qol::core
