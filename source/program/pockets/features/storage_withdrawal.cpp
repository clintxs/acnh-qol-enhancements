#include "storage_withdrawal.hpp"
#include "pockets.hpp"
namespace qol_pockets::features {
bool RegisterWithdrawalPolicy(game::ChooseMenuEntry);
bool InstallStorageWithdrawal() { return RegisterWithdrawalPolicy(WithdrawDirectly); }
}
