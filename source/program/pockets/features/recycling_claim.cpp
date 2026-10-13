#include "recycling_claim.hpp"
#include "pockets.hpp"
namespace qol_pockets::features {
bool RegisterClaimPolicy(game::ChooseMenuEntry);
bool InstallRecyclingClaim() { return RegisterClaimPolicy(ClaimDirectly); }
}
