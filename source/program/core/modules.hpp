#pragma once
#include "config.hpp"
#include <array>
namespace qol::core {
struct ModuleDescription {
    const char *name;
    const char *slug;
    Version minimum;
    bool (*install)();
};
extern const std::array<ModuleDescription, 5> Modules;
void ActivateModules();
} // namespace qol::core
