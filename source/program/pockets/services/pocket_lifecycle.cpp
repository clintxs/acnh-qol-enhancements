#include "pocket_lifecycle.hpp"
#include "program/core/feature_activation.hpp"
namespace qol_pockets::lifecycle {
namespace { PlayEntryReset listener{}; }
bool RegisterBeforePlayEntry(PlayEntryReset reset) {
    if (!reset) return false;
    if (listener == reset) return true;
    if (listener) {
        qol::core::ReportActivationFailure(qol::core::FeatureState::DependencyUnavailable,
                                          "pocket Play listener capacity");
        return false;
    }
    listener = reset;
    qol::core::RecordMutation();
    return true;
}
void BeforePlayEntry(void *controller) { if (listener) listener(controller); }
}
