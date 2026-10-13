#include "shopping_features.hpp"
#include <algorithm>

namespace qol::shopping {
bool AcceptsCard(PurchaseContext context) {
    switch (context) {
    case PurchaseContext::ShopPurchase: // Nook's Cranny and Able Sisters, including fitting checkout
    case PurchaseContext::CatalogOrder: // Nook Stop and phone, including outdoors
    case PurchaseContext::StorageUpgrade: // Tom Nook's verified storage subflow only
        return true;
    default: return false;
    }
}
bool ValidBalances(const Balances &balance) {
    return balance.savings <= SavingsLimit && balance.pockets <= PocketBellsLimit &&
           balance.wallet <= WalletLimit;
}
unsigned TotalBells(const Balances &balance) {
    return ValidBalances(balance) ? balance.savings + balance.pockets + balance.wallet : 0;
}
Payment PlanPayment(const Balances &balance, unsigned price) {
    if (!ValidBalances(balance) || price > TotalBells(balance)) return {};
    Payment plan{true};
    plan.savings = std::min(price, balance.savings);
    const auto cashPrice = price - plan.savings;
    plan.pockets = std::min(cashPrice, balance.pockets);
    plan.wallet = cashPrice - plan.pockets;
    if (!plan.pockets) {
        plan.cashDebit = plan.wallet;
    } else {
        // Native cash spending is wallet-first. Include the wallet so native
        // debit consumes the requested Bell bags, then preserve the unused
        // wallet through native cash addition and its normal change handling.
        plan.cashDebit = balance.wallet + plan.pockets;
        plan.walletRefund = balance.wallet - plan.wallet;
    }
    return plan;
}
Payment PayForPurchase(game::CardPayment &native, unsigned price) {
    Balances balance;
    const auto plan = native.ReadFunds(balance) ? PlanPayment(balance, price) : Payment{};
    if (!plan.affordable) return plan;
    if (plan.savings) native.WithdrawSavings(plan.savings);
    if (plan.cashDebit) native.SpendNativeCash(plan.cashDebit);
    if (plan.walletRefund) native.ReturnNativeChange(plan.walletRefund);
    // The native actions return void: this means the planned calls returned,
    // not an atomic commit, delivery success, or a rollback guarantee.
    return plan;
}
bool RouteSaleToSavings(bool nookMarket) { return nookMarket; }
bool UseOrdinarySaleMessage(bool nookMarket) { return nookMarket; }
unsigned PurchaseSpaceBells(unsigned) {
    // Do not promise delivery slots freed by spending Bell bags: native change
    // can reuse them. The native preflight still accounts for tailor tickets.
    return 0;
}
unsigned DisplayedBalance(unsigned total) { return std::min(total, SavingsLimit); }
unsigned FormatSavings(char16_t (&output)[12], unsigned balance) {
    balance = DisplayedBalance(balance);
    char16_t reversed[12]{};
    unsigned length = 0, digits = 0;
    do {
        if (digits && digits % 3 == 0) reversed[length++] = u',';
        reversed[length++] = u'0' + balance % 10;
        balance /= 10;
        ++digits;
    } while (balance && length < 11);
    for (unsigned i = 0; i < length; ++i) output[i] = reversed[length - i - 1];
    output[length] = 0;
    return length;
}
}
