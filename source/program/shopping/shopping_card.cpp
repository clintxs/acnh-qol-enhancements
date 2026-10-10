#include "lib.hpp"
#include "shopping_card.hpp"
#include "program/core/resource_budget.hpp"
#include "payment_policy.hpp"
#include <array>
#include <cstring>

namespace qol::shopping {
namespace {
namespace address {
constexpr std::uintptr_t ResourceSizeQuery = 0x03e3cad0;
constexpr std::uintptr_t Wallet = 0x0257abf0, Debit = 0x0257b340;
constexpr std::uintptr_t WalletSpace = 0x0257ad90, AddCash = 0x0257af90;
constexpr std::uintptr_t BellDisplayUpdate = 0x0301b394, BellPageUpdate = 0x02b13bf0;
constexpr std::uintptr_t StepNumber = 0x03353b14, NumberFeedback = 0x03025400, FormatNumber = 0x03025a4c;
constexpr std::uintptr_t FindParameter = 0x02b12c24, SafeStringVtableGot = 0x04ab2020;
constexpr std::uintptr_t FindPane = 0x0027d1d0, SetPaneText = 0x03c1b588, SetGroupText = 0x03c47b90;
constexpr std::uintptr_t PurchaseSpace = 0x02579fe0;
constexpr std::uintptr_t Capacity = 0x0257b6f0, MoneyCapacityQuery = 0x00f86e68;
constexpr std::uintptr_t Savings = 0x0257f8b0, Withdraw = 0x0257fb30;
constexpr std::uintptr_t FieldManagerGot = 0x04ab2720, FieldTypesGot = 0x04ab2640;
constexpr std::uintptr_t IndoorFieldInfoGot = 0x04ac3a80, HashString = 0x005bd550;
}
namespace field {
constexpr unsigned ActiveField = 0x330, FieldTypeKey = 0x338, StageHash = 0x38;
constexpr unsigned ActionEvent = 8, EventPlayer = 0x2a8, CapacityModeText = 0x100;
}
namespace catalog {
constexpr std::uintptr_t WalletReturn = 0x02b77640, DebitReturn = 0x02b7f58c;
constexpr std::uintptr_t Open = 0x02b70ffc, Close = 0x02b711b8, Destroy = 0x02b66cf0;
constexpr unsigned Player = 0x1f8, DisplayKey = 1;
}
namespace display {
constexpr unsigned Counter = 0x210, Player = 0x230, NumberBinding = 0x250;
constexpr unsigned Page = 0x210, PageLayout = 0x3f8, Parameters = 0x38, HudPlayer = 0x224;
constexpr unsigned BindingAdapter = 0x48, AdapterPayload = 8, AdapterMethods = 0x18;
constexpr unsigned SetTextMethod = 0x5e0, GroupCount = 0x10, GroupLinkOffset = 0x14;
constexpr unsigned NextLink = 8, MemberPane = 0x10, MemberMethods = 0x18;
constexpr unsigned PaneParent = 0x18, PaneFlags = 0x58, CashCurrent = 0x260;
}
// MurmurHash3(name, 0), independently checked against native 005bd550.
constexpr unsigned MarketSmall = 0xbe3cad72, MarketLarge = 0x8a17f484, Tailor = 0x91e12aa0;
constexpr unsigned IndoorFieldType = 0x758f7eaf;
bool installed{};
template<class T> T Read(std::uintptr_t object, unsigned offset = 0) {
    T result;
    std::memcpy(&result, reinterpret_cast<const void *>(object + offset), sizeof(result));
    return result;
}
std::uintptr_t Main(std::uintptr_t offset) { return exl::util::modules::GetTargetStart() + offset; }
template<class T> T Fn(std::uintptr_t offset) { return reinterpret_cast<T>(Main(offset)); }
bool NativeFunction(std::uintptr_t pointer) {
    const auto &text = exl::util::GetMainModuleInfo().m_Text;
    return pointer >= text.m_Start && pointer - text.m_Start < text.m_Size && !(pointer & 3);
}
unsigned CurrentShop() {
    // Mirror ShopHasBellCount (00e7ba00..00e7bf84). Its field-type table returns
    // a SafeString; only indoor fields use IndoorFieldInfo. Never retain a room
    // or player between calls: leaving a shop must immediately stop routing.
    const auto managerSlot = Read<std::uintptr_t>(Main(address::FieldManagerGot));
    const auto manager = managerSlot ? Read<std::uintptr_t>(managerSlot) : 0;
    if (!manager || !Read<std::uintptr_t>(manager, field::ActiveField)) return 0;
    const auto typesSlot = Read<std::uintptr_t>(Main(address::FieldTypesGot));
    const auto types = typesSlot ? Read<std::uintptr_t>(typesSlot) : 0;
    const auto methods = types ? Read<std::uintptr_t>(types) : 0;
    const auto getter = methods ? Read<std::uintptr_t>(methods, 0x18) : 0;
    if (!NativeFunction(getter)) return 0;
    const auto name = reinterpret_cast<std::uintptr_t (*)(std::uintptr_t, unsigned)>(getter)(
        types, Read<unsigned>(manager, field::FieldTypeKey));
    const auto text = name ? Read<std::uintptr_t>(name) : 0;
    if (!text || Fn<unsigned (*)(std::uintptr_t, unsigned)>(address::HashString)(text, 0) != IndoorFieldType) return 0;
    const auto info = Read<std::uintptr_t>(Main(address::IndoorFieldInfoGot));
    if (!info) return 0;
    const auto stage = Read<unsigned>(info, field::StageHash);
    return stage == MarketSmall || stage == MarketLarge || stage == Tailor ? stage : 0;
}
bool Valid(const std::uint8_t *player) { return player && *player < 8; }
bool ReadBalances(const std::uint8_t *player, Balances &balance) {
    if (!Valid(player)) return false;
    // Native accessors include resident lookup and tracked money reads.
    // WalletSpace reports 99,999 minus wallet Bells, excluding Bell-bag items;
    // Wallet reports their combined value. Calls from this module are outside
    // the reviewed shop callsites, so the Wallet hook returns native cash.
    const auto space = Fn<unsigned (*)(const std::uint8_t *)>(address::WalletSpace)(player);
    const auto cash = Fn<unsigned (*)(const std::uint8_t *)>(address::Wallet)(player);
    if (space > WalletLimit || cash < WalletLimit - space) return false;
    balance.wallet = WalletLimit - space;
    balance.pockets = cash - balance.wallet;
    balance.savings = Fn<unsigned (*)(const std::uint8_t *)>(address::Savings)(player);
    return ValidBalances(balance);
}
unsigned CombinedBalance(const std::uint8_t *player) {
    Balances balance;
    return ReadBalances(player, balance) ? TotalBells(balance) : 0;
}

// Exact BL return addresses, not broad address ranges. Shared helpers also
// serve the ABD, pockets, storage, travel, Harv, HHP and other vendors.
constexpr std::array WalletCallers{
    0x00e7bf88u, // ShopHasBellCount
    0x02ad2fd0u, 0x02ad41e8u, 0x02ad5a58u, 0x02ad5fecu, // shop selection/affordability
    0x031192dcu, // fitting-room initial balance (also used without Fitting Room module)
};
constexpr std::array DebitCallers{
    0x00ace114u, 0x00acf38cu, 0x00adc1d4u, // shop purchase actions
    0x00b168b8u, // ShopBuyDone (floor displays and cabinet)
    0x0311d404u, // native fitting-room checkout, including suite shopping bag
};
template<std::size_t N> bool Contains(const std::array<unsigned, N> &sites, std::uintptr_t caller) {
    const auto start = exl::util::modules::GetTargetStart();
    if (caller < start || caller - start > UINT32_MAX) return false;
    for (auto site : sites) if (caller - start == site) return true;
    return false;
}
// Catalog and BellPage can belong to different native UI contexts (phone,
// Nook Stop, and field HUD). Follow Catalog's actual open/close lifecycle
// rather than searching BellPage's unrelated root list. Keep only resident bits;
// never retain a controller, layout or save pointer across calls.
unsigned catalogPlayers{};
void SetCatalogOpen(std::uintptr_t owner, bool active) {
    const auto player = owner ? reinterpret_cast<const std::uint8_t *>(owner + catalog::Player) : nullptr;
    if (!Valid(player)) return;
    const auto bit = 1u << *player;
    if (active) catalogPlayers |= bit;
    else catalogPlayers &= ~bit;
}
HOOK_DEFINE_TRAMPOLINE(CatalogOpened) {
    static void Callback(std::uintptr_t owner) {
        Orig(owner);
        SetCatalogOpen(owner, true);
    }
};
HOOK_DEFINE_TRAMPOLINE(CatalogClosed) {
    static void Callback(std::uintptr_t owner) {
        SetCatalogOpen(owner, false);
        Orig(owner);
    }
};
HOOK_DEFINE_TRAMPOLINE(CatalogDestroyed) {
    static void Callback(std::uintptr_t owner) {
        // Covers both ordinary and deleting destructors: 02b67074 calls this
        // same native destructor. Clear before native teardown invalidates owner.
        SetCatalogOpen(owner, false);
        Orig(owner);
    }
};
// Currency display calls deliberately stay on the native wallet. In 0.3.1,
// the generic counter initialized from pocket Bells, then refreshed from savings
// and played a spurious change animation after closing pockets. The savings
// display has its own text pane, independent of the native cash animation.
std::uintptr_t BoundNumberPane(std::uintptr_t counter) {
    const auto binding = counter ? Read<std::uintptr_t>(counter, display::NumberBinding) : 0;
    const auto adapter = binding ? Read<std::uintptr_t>(binding, display::BindingAdapter) : 0;
    const auto payload = adapter ? Read<std::uintptr_t>(adapter, display::AdapterPayload) : 0;
    const auto methods = adapter ? Read<std::uintptr_t>(adapter, display::AdapterMethods) : 0;
    if (!payload || !methods) return 0;
    const auto setter = Read<std::uintptr_t>(methods, display::SetTextMethod);
    if (setter == Main(address::SetPaneText)) return payload;
    if (setter != Main(address::SetGroupText)) return 0;
    // Same typed intrusive text group used by Catalog and Fitting Room. A group
    // header is not a Pane: resolve a member before following PaneParent.
    const auto count = Read<unsigned>(payload, display::GroupCount);
    const auto offset = Read<int>(payload, display::GroupLinkOffset);
    if (count > 16 || offset < 0 || offset > 128) return 0;
    auto node = Read<std::uintptr_t>(payload, display::NextLink);
    for (unsigned i = 0; i < count && node && node != payload; ++i) {
        const auto member = node - offset;
        const auto table = Read<std::uintptr_t>(member, display::MemberMethods);
        if (table && Read<std::uintptr_t>(table, display::SetTextMethod) == Main(address::SetPaneText))
            return Read<std::uintptr_t>(member, display::MemberPane);
        node = Read<std::uintptr_t>(node, display::NextLink);
    }
    return 0;
}
std::uintptr_t FindPane(std::uintptr_t root, const char *name) {
    return root ? Fn<std::uintptr_t (*)(std::uintptr_t, const char *, bool)>(address::FindPane)(root, name, true) : 0;
}
std::uintptr_t ShoppingCardPane(std::uintptr_t counter) {
    auto root = BoundNumberPane(counter);
    for (unsigned depth = 0; root && depth < 12; ++depth) {
        if (const auto card = FindPane(root, "N_ShoppingCard_00")) return card;
        root = Read<std::uintptr_t>(root, display::PaneParent);
    }
    return 0;
}
HOOK_DEFINE_TRAMPOLINE(CashNumberFeedback) {
    static void Callback(std::uintptr_t counter, const void *event) {
        const auto card = ShoppingCardPane(counter);
        if (card && (Read<std::uint8_t>(card, display::PaneFlags) & 1)) {
            // The combined counter supplies the one visible transition. Keep
            // native cash text current without replaying its hidden animation
            // and sound, including cash changes while the display is capped.
            Fn<void (*)(std::uintptr_t, unsigned)>(address::FormatNumber)(
                counter, Read<unsigned>(reinterpret_cast<std::uintptr_t>(event), 4));
            return;
        }
        Orig(counter, event);
    }
};
void ShowPane(std::uintptr_t pane, bool visible) {
    if (!pane) return;
    auto &flags = *reinterpret_cast<std::uint8_t *>(pane + display::PaneFlags);
    flags = (flags & ~1u) | unsigned(visible);
}
unsigned FormatSavings(char16_t (&output)[12], unsigned balance) {
    // Match the native nine-digit presentation; affordability uses the full total.
    balance = std::min(balance, SavingsLimit);
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
enum class DisplaySurface : unsigned { Auxiliary, Hud };
// The native number transition is a value object (counter+260), separate from
// the controller and its text binding. Use its exact stepping/rounding with an
// empty callback, then send feedback through the live HUD's native Up/Down and
// Finish animations. No save/UI pointer is retained across frames.
bool NoNumberCallback(const void *) { return false; }
const std::uintptr_t NumberMethods[]{0, 0, reinterpret_cast<std::uintptr_t>(&NoNumberCallback)};
struct NumberTransition {
    int current{}, target{}, step{}, padding{};
    const std::uintptr_t *methods{NumberMethods};
    std::uint8_t reserved[24]{};
    std::uint8_t queued{};
    std::uint8_t padding2[3]{};
    int start{}, end{};
    std::uint8_t resetStart{1};
    std::uint8_t padding3[3]{};
    std::uint8_t finished{};
};
static_assert(offsetof(NumberTransition, queued) == 0x30);
static_assert(offsetof(NumberTransition, finished) == 0x40);
struct SavingsAnimation {
    NumberTransition number{};
    unsigned shop{}, player{};
    bool initialized{};
};
SavingsAnimation savingsAnimation{};
unsigned AnimatedSavings(std::uintptr_t counter, unsigned balance, unsigned shop, unsigned player) {
    auto &state = savingsAnimation;
    auto &number = state.number;
    if (!state.initialized || state.shop != shop || state.player != player) {
        state = {};
        state.initialized = true;
        state.shop = shop;
        state.player = player;
        number.current = number.target = number.end = balance;
        return balance; // Opening a shop or switching residents is silent.
    }
    if (number.end != static_cast<int>(balance)) {
        number.start = number.current;
        number.end = balance;
        number.queued = 1;
    }
    const auto previous = number.current;
    Fn<void (*)(NumberTransition *)>(address::StepNumber)(&number);
    if (number.current != previous) {
        struct Event { int previous, current, descending; bool finished; };
        const Event event{previous, number.current, previous >= number.target, bool(number.finished)};
        CashNumberFeedback::Orig(counter, &event);
        // Feedback also formats T_Num_00. Restore that hidden cash text at once
        // so leaving the shop never exposes a stale savings amount. The native
        // cash controller's current/target/cache fields are never changed.
        Fn<void (*)(std::uintptr_t, unsigned)>(address::FormatNumber)(
            counter, Read<unsigned>(counter, display::CashCurrent));
    }
    return number.current;
}
std::uintptr_t BellPageCounter(std::uintptr_t owner) {
    // BellPage (02b13c44..02b13c6c) owns a Page, not a counter. Resolve its
    // /L_Bell_00 typed parameter through the same native layout lookup.
    const auto page = owner ? Read<std::uintptr_t>(owner, display::Page) : 0;
    const auto holder = page ? Read<std::uintptr_t>(page, display::PageLayout) : 0;
    const auto layout = holder ? Read<std::uintptr_t>(holder) : 0;
    const auto methods = Read<std::uintptr_t>(Main(address::SafeStringVtableGot));
    if (!layout || !methods) return 0;
    struct SafeString { std::uintptr_t methods; const char *text; };
    const SafeString path{methods + 0x10, "/L_Bell_00"};
    return Fn<std::uintptr_t (*)(std::uintptr_t, const SafeString *)>(address::FindParameter)(
        layout + display::Parameters, &path);
}
void UpdateSavingsDisplay(std::uintptr_t counter, const std::uint8_t *player, DisplaySurface surface) {
    const auto shop = !Valid(player) ? 0 : (catalogPlayers & (1u << *player)) ? catalog::DisplayKey : CurrentShop();
    if (surface == DisplaySurface::Hud && !shop) savingsAnimation.initialized = false;
    auto root = BoundNumberPane(counter);
    std::uintptr_t card = 0;
    for (unsigned depth = 0; root && depth < 12; ++depth) {
        card = FindPane(root, "N_ShoppingCard_00");
        if (card) break;
        root = Read<std::uintptr_t>(root, display::PaneParent); // native PaneParent
    }
    // Bounded diagnostics distinguish a missing resource/binding from a wrong
    // HUD path without retaining UI/player pointers between updates.
    static std::array<unsigned, 2> missingReports{};
    const auto index = static_cast<unsigned>(surface);
    const char *source = surface == DisplaySurface::Hud ? "HUD" : "auxiliary";
    if (!card) {
        if (shop && missingReports[index]++ < 4)
            Logging.Error("[QoL Enhancements/Shopping Card] SAVINGS_DISPLAY_MISSING source=%s counter=%lx root=%lx\n",
                        source, counter, root);
        return;
    }
    const auto number = FindPane(card, "T_SavingsNum_00");
    const bool active = number && shop;
    const bool wasActive = Read<std::uint8_t>(card, display::PaneFlags) & 1;
    if (active) {
        char16_t text[12]{};
        const auto balance = std::min(CombinedBalance(player), SavingsLimit);
        // Only the regular HUD drives feedback, so an auxiliary counter cannot
        // double-play the same purchase sound. Its balance remains immediate.
        const auto shown = surface == DisplaySurface::Hud ? AnimatedSavings(counter, balance, shop, *player) : balance;
        const auto length = FormatSavings(text, shown);
        static std::array<bool, 2> reported{};
        if (!reported[index]) {
            reported[index] = true;
            Logging.Debug("[QoL Enhancements/Shopping Card] SAVINGS_DISPLAY_READY source=%s player=%u balance=%u\n",
                        source, *player, balance);
        }
        Fn<void (*)(std::uintptr_t, const char16_t *, unsigned)>(address::SetPaneText)(number, text, length);
    }
    ShowPane(card, active);
    if (active || wasActive) {
        for (const char *name : {"P_IconBase_00", "W_BaseL_00", "W_BaseStripe_00",
                                "N_RegionType_00", "P_Icon_00", "T_Num_00"})
            ShowPane(FindPane(root, name), !active);
    }
}
HOOK_DEFINE_TRAMPOLINE(ShopBalanceDisplay) {
    static void Callback(std::uintptr_t owner) {
        // The ordinary cash controller still owns its native lifecycle, amount,
        // sounds and visibility. Only its top-right presentation is switched;
        // generic pockets/price counters are never restyled. No UI pointer
        // survives the call, including across scene destruction.
        Orig(owner);
        const auto counter = owner ? Read<std::uintptr_t>(owner, display::Counter) : 0;
        const auto player = owner ? reinterpret_cast<const std::uint8_t *>(owner + display::Player) : nullptr;
        UpdateSavingsDisplay(counter, player, DisplaySurface::Auxiliary);
    }
};
HOOK_DEFINE_TRAMPOLINE(BellPageBalanceDisplay) {
    static void Callback(std::uintptr_t owner) {
        // The regular in-shop HUD uses BellPage, a different controller from
        // ShopBalanceDisplay. Cash initialization/refresh stays native on both.
        Orig(owner);
        const auto player = owner ? reinterpret_cast<const std::uint8_t *>(owner + display::HudPlayer) : nullptr;
        UpdateSavingsDisplay(BellPageCounter(owner), player, DisplaySurface::Hud);
    }
};
HOOK_DEFINE_TRAMPOLINE(ShopWallet) {
    static unsigned Callback(const std::uint8_t *player) {
        const auto caller = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
        // Catalog's exact checkout caller works at Nook Stop and on the phone,
        // including outdoors. Its ordinary cash display calls remain untouched.
        if (Valid(player) && (caller == Main(catalog::WalletReturn) ||
                              (Contains(WalletCallers, caller) && CurrentShop())))
            return CombinedBalance(player);
        return Orig(player);
    }
};
HOOK_DEFINE_TRAMPOLINE(ShopDebit) {
    static void Callback(const std::uint8_t *player, unsigned amount) {
        const auto caller = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
        const bool catalogOrder = caller == Main(catalog::DebitReturn);
        if (!Valid(player) || (!catalogOrder && !(Contains(DebitCallers, caller) && CurrentShop()))) {
            Orig(player, amount);
            return;
        }
        Balances balance;
        const auto plan = ReadBalances(player, balance) ? PlanPayment(balance, amount) : Payment{};
        if (!plan.affordable) {
            Logging.Debug("[QoL Enhancements/Shopping Card] INSUFFICIENT player=%u bells=%u\n", *player, amount);
            return;
        }
        if (plan.savings)
            Fn<void (*)(const std::uint8_t *, unsigned)>(address::Withdraw)(player, plan.savings);
        if (plan.cashDebit) Orig(player, plan.cashDebit);
        if (plan.walletRefund)
            Fn<void (*)(const std::uint8_t *, unsigned)>(address::AddCash)(player, plan.walletRefund);
        Logging.Debug("[QoL Enhancements/Shopping Card] PURCHASE player=%u bells=%u savings=%u pockets=%u wallet=%u\n",
                    *player, amount, plan.savings, plan.pockets, plan.wallet);
    }
};
HOOK_DEFINE_TRAMPOLINE(ShopCapacity) {
    static unsigned Callback(const std::uint8_t *player, unsigned reservedSlots) {
        const auto caller = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
        const auto shop = caller == Main(0x00b1d4d8) && Valid(player) ? CurrentShop() : 0;
        if (shop == MarketSmall || shop == MarketLarge) {
            // Native ShopSellDone compares this capacity BEFORE choosing either
            // wallet/Bell bags or its existing savings deposit at 00b1d4e8.
            Logging.Debug("[QoL Enhancements/Shopping Card] SALE_TO_SAVINGS player=%u\n", *player);
            return 0;
        }
        return Orig(player, reservedSlots);
    }
};
HOOK_DEFINE_TRAMPOLINE(ShopPurchaseSpace) {
    static unsigned Callback(const std::uint8_t *player, unsigned bells, unsigned tickets) {
        const auto caller = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
        if ((caller == Main(0x00f92560) || caller == Main(0x03114400)) && Valid(player) && CurrentShop()) {
            // Keep the conservative card-space preflight: do not promise slots
            // freed by bag spending, because native change/refunds may reuse a
            // slot. Preserve empty-slot and tailor-ticket accounting as before.
            bells = 0;
        }
        return Orig(player, bells, tickets);
    }
};
HOOK_DEFINE_TRAMPOLINE(SaleMessage) {
    static unsigned Callback(std::uintptr_t query) {
        const auto event = query ? Read<std::uintptr_t>(query, field::ActionEvent) : 0;
        const auto player = event ? reinterpret_cast<const std::uint8_t *>(event + field::EventPlayer) : nullptr;
        const auto text = query ? Read<const char *>(query, field::CapacityModeText) : nullptr;
        const auto shop = Valid(player) && text && !std::strcmp(text, "cShop") ? CurrentShop() : 0;
        // Case 0 is the ordinary localized "Thank you! Please come again!".
        // Case 2 explains that pockets are full, which is misleading for card
        // payments. Dialogue selection is independent of ShopCapacity: that
        // hook still routes the actual sale proceeds to native savings.
        if (shop == MarketSmall || shop == MarketLarge) return 0;
        return Orig(query);
    }
};
} // namespace
unsigned AvailableBells(const std::uint8_t *player) {
    if (installed && Valid(player) && CurrentShop())
        return CombinedBalance(player);
    return Fn<unsigned (*)(const std::uint8_t *)>(address::Wallet)(player);
}
bool PrepareResources() { return core::PrepareShoppingResources(); }
bool Install() {
    struct Fingerprint { std::uintptr_t offset; std::array<std::uint32_t, 4> words; };
    const Fingerprint checks[]{
#include "fingerprints.inc"
    };
    const auto &text = exl::util::GetMainModuleInfo().m_Text;
    for (const auto &check : checks) {
        if (check.offset + sizeof(check.words) > text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(Main(check.offset)), check.words.data(), sizeof(check.words))) {
            Logging.Error("[QoL Enhancements/Shopping Card] DISABLED: instruction mismatch at %lx\n", check.offset);
            return false;
        }
    }
    if (!PrepareResources()) return false;
    ShopWallet::InstallAtOffset(address::Wallet);
    ShopDebit::InstallAtOffset(address::Debit);
    ShopCapacity::InstallAtOffset(address::Capacity);
    ShopPurchaseSpace::InstallAtOffset(address::PurchaseSpace);
    SaleMessage::InstallAtOffset(address::MoneyCapacityQuery);
    ShopBalanceDisplay::InstallAtOffset(address::BellDisplayUpdate);
    BellPageBalanceDisplay::InstallAtOffset(address::BellPageUpdate);
    CatalogOpened::InstallAtOffset(catalog::Open);
    CatalogClosed::InstallAtOffset(catalog::Close);
    CatalogDestroyed::InstallAtOffset(catalog::Destroy);
    CashNumberFeedback::InstallAtOffset(address::NumberFeedback);
    installed = true;
    Logging.Debug("[QoL Enhancements/Shopping Card] READY: scoped counter resource budget; combined Bells display; savings -> pocket Bell bags -> wallet in Nook's Cranny/Able Sisters/Nook Shopping\n");
    return true;
}
} // namespace qol::shopping
