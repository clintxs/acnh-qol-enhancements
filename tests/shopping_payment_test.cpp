#include "program/shopping/features/shopping_features.hpp"
#include <vector>
#include <string>
#include <array>
#include <cassert>
#include <cstdio>
#include <limits>

using namespace qol::shopping;
struct NativePayment final : game::CardPayment {
    Balances funds{300000,200000,99000};
    bool readable{true};
    std::vector<std::pair<std::string,unsigned>> calls;
    bool ReadFunds(Balances &out) override { calls.emplace_back("read",0); out=funds; return readable; }
    void WithdrawSavings(unsigned amount) override { calls.emplace_back("savings",amount); }
    void SpendNativeCash(unsigned amount) override { calls.emplace_back("cash",amount); }
    void ReturnNativeChange(unsigned amount) override { calls.emplace_back("change",amount); }
};
int main() {
    for (auto context : {PurchaseContext::ShopPurchase, PurchaseContext::CatalogOrder, PurchaseContext::StorageUpgrade})
        assert(AcceptsCard(context));
    assert(!AcceptsCard(PurchaseContext::OrdinaryCash));
    NativePayment native;
    assert(PayForPurchase(native,500000).affordable);
    const decltype(native.calls) expected{{"read",0},{"savings",300000},{"cash",299000},{"change",99000}};
    assert(native.calls==expected);
    for (bool readable : {false,true}) {
        native.calls.clear(); native.readable=readable;
        assert(!PayForPurchase(native,600000).affordable);
        assert(native.calls.size()==1 && native.calls.front().first=="read");
    }
    native.calls.clear(); assert(PayForPurchase(native,0).affordable && native.calls.size()==1);
    assert(RouteSaleToSavings(true) && !RouteSaleToSavings(false));
    assert(UseOrdinarySaleMessage(true) && !UseOrdinarySaleMessage(false));
    assert(PurchaseSpaceBells(500000)==0);
    char16_t display[12];
    assert(FormatSavings(display,1004000000)==11 && std::u16string(display)==u"999,999,999");
    assert(FormatSavings(display,0)==1 && std::u16string(display)==u"0");

    const auto example = PlanPayment({300000, 200000, 99000}, 500000);
    assert(example.affordable && example.savings == 300000 && example.pockets == 200000);
    assert(example.wallet == 0 && example.cashDebit == 299000 && example.walletRefund == 99000);
    const auto walletLast = PlanPayment({300000, 150000, 99000}, 500000);
    assert(walletLast.wallet == 50000 && walletLast.walletRefund == 49000);
    unsigned cases = 0;
    constexpr std::array<unsigned, 10> savings{0, 1, 99, 100, 999, 1000, 300000, 500000, 999999998, SavingsLimit};
    constexpr std::array<unsigned, 8> pockets{0, 1, 100, 1000, 99000, 150000, 200000, PocketBellsLimit};
    constexpr std::array<unsigned, 7> wallets{0, 1, 99, 1000, 50000, 99000, WalletLimit};
    for (const auto s : savings) for (const auto p : pockets) for (const auto w : wallets) {
        const Balances balance{s, p, w};
        const auto total = TotalBells(balance);
        const std::array prices{0u, 1u, s, s + 1, s + p, s + p + 1, total, total + 1,
                                std::numeric_limits<unsigned>::max()};
        for (const auto price : prices) {
            const auto plan = PlanPayment(balance, price);
            assert(plan.affordable == (price <= total));
            if (!plan.affordable) {
                assert(!plan.savings && !plan.pockets && !plan.wallet && !plan.cashDebit && !plan.walletRefund);
            } else {
                // Independent priority and conservation invariants, including
                // the native wallet-first debit plus wallet-preserving refund.
                assert(plan.savings + plan.pockets + plan.wallet == price);
                assert(plan.savings <= s && plan.pockets <= p && plan.wallet <= w);
                assert(!plan.pockets || plan.savings == s);
                assert(!plan.wallet || (plan.savings == s && plan.pockets == p));
                assert(plan.cashDebit <= p + w && plan.walletRefund <= w);
                assert(total - plan.savings - plan.cashDebit + plan.walletRefund == total - price);
                assert(plan.cashDebit - plan.walletRefund == plan.pockets + plan.wallet);
                if (!price) assert(!plan.cashDebit && !plan.walletRefund);
            }
            ++cases;
        }
    }
    for (const Balances invalid : {Balances{SavingsLimit + 1,0,0}, {0,PocketBellsLimit + 1,0},
                                   {0,0,WalletLimit + 1}}) {
        assert(!PlanPayment(invalid, 0).affordable && TotalBells(invalid) == 0);
    }
    std::printf("PASS: %u payment cases, savings -> Bell bags -> wallet, combined affordability, native refund conservation and bounds\n", cases);
}
