#pragma once
#include "config.hpp"
#include <array>
namespace qol::core {
struct ModuleDescription {
    const char *name;
    const char *slug;
    Version minimum;
    bool (*install)();
    // Optional per-module parser, used identically for regular and custom INIs.
    unsigned (*configure)(std::string_view) = nullptr;
    // RomFS overrides still load when behavior is disabled or config is invalid.
    // Prepare their allocation support whenever the package marker is present.
    bool (*prepareResources)() = nullptr;
    // Report effective settings after both layers, including disabled modules.
    void (*reportSettings)() = nullptr;
};
extern const std::array<ModuleDescription, 8> Modules;
void ActivateModules();
} // namespace qol::core
