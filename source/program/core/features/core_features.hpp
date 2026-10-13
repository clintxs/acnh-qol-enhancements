#pragma once
#include "program/core/feature_activation.hpp"
#include "program/core/settings.hpp"

namespace qol::core {
// Startup-only: called once after regular/custom Core preferences are applied.
FeatureResult ActivateCoreFeatures(const Settings &settings);
}
