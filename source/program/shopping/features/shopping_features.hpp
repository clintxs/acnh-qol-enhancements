#pragma once
#include "program/shopping/game/card_payment.hpp"

namespace qol::shopping {
// Native adapters establish these contexts from exact reviewed callers/current
// EventFlow nodes. Ordinary cash callers never acquire a purchase context.
enum class PurchaseContext { OrdinaryCash, ShopPurchase, CatalogOrder, StorageUpgrade };
bool AcceptsCard(PurchaseContext context);
Payment PayForPurchase(game::CardPayment &native, unsigned price);
bool RouteSaleToSavings(bool nookMarket);
bool UseOrdinarySaleMessage(bool nookMarket);
unsigned PurchaseSpaceBells(unsigned price);
unsigned DisplayedBalance(unsigned total);
unsigned FormatSavings(char16_t (&output)[12], unsigned balance);
}
