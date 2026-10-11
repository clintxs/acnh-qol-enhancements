#pragma once
namespace qol::core {
// Return nullptr for keys owned by another feature. Providers are called only
// during native message initialization, not each rendered frame.
using HintTextProvider = const char16_t *(*)(const char *key);
bool RegisterHintText(HintTextProvider provider);
}
