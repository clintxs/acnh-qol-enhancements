#pragma once
namespace qol::dialogue {
// Native reveal plumbing shared by Faster Dialogue and Controls. Installing
// these hooks alone leaves ordinary dialogue speed/input unchanged.
bool InstallRevealHooks();
}
