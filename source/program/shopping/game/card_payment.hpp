#pragma once
#include "program/shopping/payment_policy.hpp"

namespace qol::shopping::game {
// Callback-local access to the verified purchase paths. Native debit owns
// wallet/Bell-bag removal and native addition owns change and overflow.
class CardPayment {
public:
    virtual bool ReadFunds(Balances &balance) = 0;
    virtual void WithdrawSavings(unsigned amount) = 0;
    virtual void SpendNativeCash(unsigned amount) = 0;
    virtual void ReturnNativeChange(unsigned amount) = 0;
};
}
