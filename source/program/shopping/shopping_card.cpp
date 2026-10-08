#include "lib.hpp"
#include "shopping_card.hpp"
#include <array>
#include <cstring>

namespace qol::shopping {
namespace {
namespace address {
constexpr std::uintptr_t Wallet = 0x0257abf0, Debit = 0x0257b340;
constexpr std::uintptr_t BellDisplayUpdate = 0x0301b394;
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
namespace display {
constexpr unsigned Counter = 0x210, Player = 0x230, NumberBinding = 0x250;
constexpr unsigned BindingAdapter = 0x48, AdapterPayload = 8, AdapterMethods = 0x18;
constexpr unsigned SetTextMethod = 0x5e0, GroupCount = 0x10, GroupLinkOffset = 0x14;
constexpr unsigned NextLink = 8, MemberPane = 0x10, MemberMethods = 0x18;
constexpr unsigned PaneParent = 0x18, PaneFlags = 0x58;
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
void ShowPane(std::uintptr_t pane, bool visible) {
    if (!pane) return;
    auto &flags = *reinterpret_cast<std::uint8_t *>(pane + display::PaneFlags);
    flags = (flags & ~1u) | unsigned(visible);
}
unsigned FormatSavings(char16_t (&output)[12], unsigned balance) {
    // Savings uses a fixed nine-digit native cap. Fit the complete amount,
    // including thousands separators, without the six-digit wallet counter.
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
void UpdateSavingsDisplay(std::uintptr_t owner) {
    const auto counter = owner ? Read<std::uintptr_t>(owner, display::Counter) : 0;
    auto root = BoundNumberPane(counter);
    std::uintptr_t card = 0;
    for (unsigned depth = 0; root && depth < 12; ++depth) {
        card = FindPane(root, "N_ShoppingCard_00");
        if (card) break;
        root = Read<std::uintptr_t>(root, display::PaneParent); // native PaneParent
    }
    if (!card) return; // Old/missing resource: leave the ordinary Bells UI intact.
    const auto number = FindPane(card, "T_SavingsNum_00");
    const auto player = reinterpret_cast<const std::uint8_t *>(owner + display::Player);
    const bool active = number && Valid(player) && CurrentShop();
    const bool wasActive = Read<std::uint8_t>(card, display::PaneFlags) & 1;
    if (active) {
        char16_t text[12]{};
        const auto length = FormatSavings(text, Fn<unsigned (*)(const std::uint8_t *)>(address::Savings)(player));
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
        // generic pockets/price counters are never restyled. No pointer survives
        // the call, including across resident changes or scene destruction.
        Orig(owner);
        UpdateSavingsDisplay(owner);
    }
};
HOOK_DEFINE_TRAMPOLINE(ShopWallet) {
    static unsigned Callback(const std::uint8_t *player) {
        const auto caller = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
        if (Contains(WalletCallers, caller) && Valid(player) && CurrentShop())
            return Fn<unsigned (*)(const std::uint8_t *)>(address::Savings)(player);
        return Orig(player);
    }
};
HOOK_DEFINE_TRAMPOLINE(ShopDebit) {
    static void Callback(const std::uint8_t *player, unsigned amount) {
        const auto caller = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
        if (!Contains(DebitCallers, caller) || !Valid(player) || !CurrentShop()) { Orig(player, amount); return; }
        // The native shop confirmation checks this same account. Never fall back
        // to pocket Bells if it is empty. Use the game's tracked encrypted-money
        // writer; no raw save offsets or transient wallet swaps are involved.
        Fn<void (*)(const std::uint8_t *, unsigned)>(address::Withdraw)(player, amount);
        Logging.Log("[QoL Enhancements/Shopping Card] PURCHASE player=%u bells=%u\n", *player, amount);
    }
};
HOOK_DEFINE_TRAMPOLINE(ShopCapacity) {
    static unsigned Callback(const std::uint8_t *player, unsigned reservedSlots) {
        const auto caller = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
        const auto shop = caller == Main(0x00b1d4d8) && Valid(player) ? CurrentShop() : 0;
        if (shop == MarketSmall || shop == MarketLarge) {
            // Native ShopSellDone compares this capacity BEFORE choosing either
            // wallet/Bell bags or its existing savings deposit at 00b1d4e8.
            Logging.Log("[QoL Enhancements/Shopping Card] SALE_TO_SAVINGS player=%u\n", *player);
            return 0;
        }
        return Orig(player, reservedSlots);
    }
};
HOOK_DEFINE_TRAMPOLINE(ShopPurchaseSpace) {
    static unsigned Callback(const std::uint8_t *player, unsigned bells, unsigned tickets) {
        const auto caller = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
        if ((caller == Main(0x00f92560) || caller == Main(0x03114400)) && Valid(player) && CurrentShop()) {
            // Payment from savings cannot free Bell-bag pocket slots. Preserve
            // native empty-slot and tailor-ticket accounting with a zero Bell
            // payment, or vanilla could accept a purchase it cannot deliver.
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
        return Fn<unsigned (*)(const std::uint8_t *)>(address::Savings)(player);
    return Fn<unsigned (*)(const std::uint8_t *)>(address::Wallet)(player);
}
bool Install() {
    struct Fingerprint { std::uintptr_t offset; std::array<std::uint32_t, 4> words; };
    const Fingerprint checks[]{
#include "fingerprints.inc"
    };
    const auto &text = exl::util::GetMainModuleInfo().m_Text;
    for (const auto &check : checks) {
        if (check.offset + sizeof(check.words) > text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(Main(check.offset)), check.words.data(), sizeof(check.words))) {
            Logging.Log("[QoL Enhancements/Shopping Card] DISABLED: instruction mismatch at %lx\n", check.offset);
            return false;
        }
    }
    ShopWallet::InstallAtOffset(address::Wallet);
    ShopDebit::InstallAtOffset(address::Debit);
    ShopCapacity::InstallAtOffset(address::Capacity);
    ShopPurchaseSpace::InstallAtOffset(address::PurchaseSpace);
    SaleMessage::InstallAtOffset(address::MoneyCapacityQuery);
    ShopBalanceDisplay::InstallAtOffset(address::BellDisplayUpdate);
    installed = true;
    Logging.Log("[QoL Enhancements/Shopping Card] READY: green savings display; always enabled in Nook's Cranny/Able Sisters; native savings\n");
    return true;
}
} // namespace qol::shopping
