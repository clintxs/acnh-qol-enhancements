#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace qol::core {
enum class FeatureState { Disabled, Ready, Unsupported, DependencyUnavailable, Failed };
struct FeatureResult {
    FeatureState state = FeatureState::Disabled;
    bool published{};
    const char *reason = "not requested";
    bool Complete() const { return state == FeatureState::Disabled || state == FeatureState::Ready; }
};
struct FeatureDescription {
    const char *id;
    bool enabled;
    bool (*install)();
    std::array<const char *, 3> dependencies{};
};
struct InstallResult {
    bool complete = true, published{};
    unsigned ready{}, disabled{}, unavailable{};
};
// Startup-only, bounded outcomes; no feature implementations are included here.
template<std::size_t Capacity = 64> class FeatureResults {
    struct Entry { const char *id{}; FeatureResult result{}; };
    std::array<Entry, Capacity> entries_{};
public:
    const FeatureResult *Find(std::string_view id) const {
        for (const auto &entry : entries_) if (entry.id && id == entry.id) return &entry.result;
        return nullptr;
    }
    bool Store(const char *id, FeatureResult result) {
        for (auto &entry : entries_) if (entry.id && std::string_view(id) == entry.id) { entry.result = result; return true; }
        for (auto &entry : entries_) if (!entry.id) { entry = {id, result}; return true; }
        return false;
    }
    bool DependenciesReady(const FeatureDescription &feature) const {
        for (auto dependency : feature.dependencies) if (dependency) {
            const auto *result = Find(dependency);
            if (!result || result->state != FeatureState::Ready) return false;
        }
        return true;
    }
};
extern FeatureResults<> ActivationResults;
std::uint64_t MutationCount();
void RecordMutation();
void ReportActivationFailure(FeatureState state, const char *reason);
FeatureResult ActivateFeature(const FeatureDescription &feature);
InstallResult ActivateFeatures(std::span<const FeatureDescription> features);
const char *FeatureStateName(FeatureState state);
} // namespace qol::core
