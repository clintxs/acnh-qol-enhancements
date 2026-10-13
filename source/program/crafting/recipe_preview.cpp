#include "recipe_preview.hpp"
#include "game/preview_install.hpp"
namespace qol::crafting {
bool InstallRecipePreview() { return preview_internal::InstallPhonePreview(); }
bool InstallWorkbenchPreviewTilt() { return preview_internal::InstallWorkbenchTilt(); }
}
