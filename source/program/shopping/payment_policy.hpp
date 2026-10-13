#pragma once
#include <cstdint>

namespace qol::shopping {
inline constexpr unsigned SavingsLimit = 999999999;
inline constexpr unsigned WalletLimit = 99999;
inline constexpr unsigned PocketBellsLimit = 40 * WalletLimit;
struct Balances { unsigned savings{}, pockets{}, wallet{}; };
bool ValidBalances(const Balances &balance);
unsigned TotalBells(const Balances &balance);
static_assert(SavingsLimit + PocketBellsLimit + WalletLimit < INT32_MAX);
struct Payment {
    bool affordable{};
    unsigned savings{}, pockets{}, wallet{};
    unsigned cashDebit{}, walletRefund{};
};
Payment PlanPayment(const Balances &balance, unsigned price);
} // namespace qol::shopping
