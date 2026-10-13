#include "lib.hpp"
#include "feature_activation.hpp"
namespace qol::core {
FeatureResults<> ActivationResults{};
namespace {
std::uint64_t Mutations{};
FeatureResult *CurrentFailure{};
}
std::uint64_t MutationCount() { return Mutations; }
void RecordMutation() { ++Mutations; }
void ReportActivationFailure(FeatureState state, const char *reason) {
    if (CurrentFailure) { CurrentFailure->state = state; CurrentFailure->reason = reason; }
}
const char *FeatureStateName(FeatureState state) {
    switch (state) {
        case FeatureState::Disabled: return "Disabled";
        case FeatureState::Ready: return "Ready";
        case FeatureState::Unsupported: return "Unsupported";
        case FeatureState::DependencyUnavailable: return "DependencyUnavailable";
        case FeatureState::Failed: return "Failed";
    }
    return "Failed";
}
FeatureResult ActivateFeature(const FeatureDescription &feature) {
    if (const auto *previous = ActivationResults.Find(feature.id)) return *previous;
    FeatureResult result{FeatureState::Failed, false, "activation incomplete"};
    // Reserve bounded result storage before invoking an installer.
    if (!ActivationResults.Store(feature.id, result))
        return {FeatureState::Failed, false, "feature result capacity"};
    if (!feature.enabled) result = {};
    else if (!ActivationResults.DependenciesReady(feature))
        result = {FeatureState::DependencyUnavailable, false, "required feature unavailable"};
    else {
        FeatureResult failure{FeatureState::Unsupported, false, "native preparation rejected"};
        auto *previous = CurrentFailure;
        CurrentFailure = &failure;
        const auto before = MutationCount();
        const bool complete = feature.install && feature.install();
        CurrentFailure = previous;
        result = complete ? FeatureResult{FeatureState::Ready, false, "installed"} : failure;
        result.published = MutationCount() != before;
        if (!complete && result.published && result.state == FeatureState::Unsupported &&
            std::string_view(result.reason) == "native preparation rejected")
            result = {FeatureState::Failed, true, "publication incomplete"};
    }
    ActivationResults.Store(feature.id, result);
    if (result.Complete())
        Logging.Debug("[QoL Enhancements] feature=%s state=%s published=%s reason=%s\n", feature.id,
                      FeatureStateName(result.state), result.published ? "true" : "false", result.reason);
    else
        Logging.Error("[QoL Enhancements] feature=%s state=%s published=%s reason=%s\n", feature.id,
                      FeatureStateName(result.state), result.published ? "true" : "false", result.reason);
    return result;
}
InstallResult ActivateFeatures(std::span<const FeatureDescription> features) {
    InstallResult summary{};
    for (const auto &feature : features) {
        const auto result = ActivateFeature(feature);
        summary.complete &= result.Complete();
        summary.published |= result.published;
        summary.ready += result.state == FeatureState::Ready;
        summary.disabled += result.state == FeatureState::Disabled;
        summary.unavailable += !result.Complete();
    }
    return summary;
}
} // namespace qol::core
