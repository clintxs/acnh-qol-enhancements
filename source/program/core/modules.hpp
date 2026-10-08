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
};
extern const std::array<ModuleDescription, 6> Modules;
void ActivateModules();
} // namespace qol::core
