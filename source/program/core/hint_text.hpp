#pragma once
#include "registration.hpp"
#include <cstddef>
namespace qol::core {
enum class CaptionKey : unsigned { Bag, UseItem, DropOrRelease, Count };
// Providers return immutable, process-lifetime UTF-16 (catalogs load once).
// Maximum 127 non-NUL units. Missing/invalid text uses a service-owned fallback.
using CaptionProvider = const char16_t *(*)();
inline constexpr std::size_t MaximumCaptionUnits = 127;
RegistrationResult RegisterCaption(CaptionKey key, CaptionProvider provider);
}
