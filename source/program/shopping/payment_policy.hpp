#pragma once
#include <algorithm>
#include <cstdint>

namespace qol::shopping {
inline constexpr unsigned SavingsLimit = 999999999;
inline constexpr unsigned WalletLimit = 99999;
inline constexpr unsigned PocketBellsLimit = 40 * WalletLimit;
struct Balances { unsigned savings{}, pockets{}, wallet{}; };
constexpr bool ValidBalances(const Balances &balance) {
    return balance.savings <= SavingsLimit && balance.pockets <= PocketBellsLimit &&
           balance.wallet <= WalletLimit;
}
constexpr unsigned TotalBells(const Balances &balance) {
    return ValidBalances(balance) ? balance.savings + balance.pockets + balance.wallet : 0;
}
static_assert(SavingsLimit + PocketBellsLimit + WalletLimit < INT32_MAX);
struct Payment {
    bool affordable{};
    unsigned savings{}, pockets{}, wallet{};
    unsigned cashDebit{}, walletRefund{};
};
constexpr Payment PlanPayment(const Balances &balance, unsigned price) {
    if (!ValidBalances(balance) || price > TotalBells(balance)) return {};
    Payment plan{true};
    plan.savings = std::min(price, balance.savings);
    const auto cashPrice = price - plan.savings;
    plan.pockets = std::min(cashPrice, balance.pockets);
    plan.wallet = cashPrice - plan.pockets;
    if (!plan.pockets) {
        plan.cashDebit = plan.wallet;
    } else {
        // Native cash spending is wallet-first. Include that wallet in the
        // debit so the requested Bell-bag portion is actually consumed, then
        // return the preserved wallet through native cash addition. Its normal
        // change/overflow handling keeps excess bag value as ordinary Bells.
        plan.cashDebit = balance.wallet + plan.pockets;
        plan.walletRefund = balance.wallet - plan.wallet;
    }
    return plan;
}
} // namespace qol::shopping
