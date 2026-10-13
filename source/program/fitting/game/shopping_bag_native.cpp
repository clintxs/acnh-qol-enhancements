#include "program/core/hook_plan.hpp"
#include "lib.hpp"
#include "program/fitting/features/fitting_features.hpp"
#include "program/fitting/features/fitting_hint.hpp"
#include "program/shopping/shopping_card.hpp"
#include <array>
#include <cstring>
#include <cstdio>

namespace able_bag {
namespace {
namespace address {
constexpr std::uintptr_t Begin = 0x031187d0, Destroy = 0x03114484;
constexpr std::uintptr_t Focus = 0x03114c3c, Row = 0x03115920;
constexpr std::uintptr_t Play = 0x0311a538, Price = 0x0311e180;
constexpr std::uintptr_t Checkout = 0x0311d3b0, Collect = 0x0311ca40;
constexpr std::uintptr_t Dialog = 0x031141cc, GrossPrice = 0x0311dec0;
constexpr std::uintptr_t PurchaseRecord = 0x024d74cc;
constexpr std::uintptr_t DialogTable = 0x05621af8;
constexpr std::uintptr_t StorageFree = 0x0257c230, PocketsFree = 0x025731d0;
constexpr std::uintptr_t PageVtable = 0x04a10348, InputSlot = PageVtable + 0x50;
constexpr std::uintptr_t OriginalInput = 0x03c58d24, ControllerVtable = 0x04a10620;
constexpr std::uintptr_t DialogInputSlot = 0x049cd3f8;
constexpr std::uintptr_t DialogInstance = 0x05590d38, DialogVtable = 0x049cd650;
constexpr std::uintptr_t FocusButton = 0x031017e0;
constexpr std::uintptr_t Application = 0x059f83f0;
constexpr std::uintptr_t FindPane = 0x0027d1d0, SetText = 0x03c1b588;
constexpr std::uintptr_t SetGroupText = 0x03c47b90, FindParameter = 0x02b12c24;
constexpr std::uintptr_t SafeStringVtableGot = 0x04ab2020;
}
namespace field {
constexpr unsigned Player = 0x1f8, Page = 0x210, State = 0x1b0;
constexpr unsigned Command = 0x1c318, Stock = 0x1c340, StockCount = 0x1c338;
constexpr unsigned IndexCapacity = 0x1c358, Indices = 0x1c360, IndexCount = 0x1c368;
constexpr unsigned Outfit = 0x1c36c, PriceState = 0x1c330;
constexpr unsigned HasStorage = 0x1c4c0, NativeTotal = 0x1c4c8;
constexpr unsigned List = 0x590, Rows = 0x210, RowCount = 0x208;
}
template<class T> T &At(std::uintptr_t object, std::size_t offset = 0) {
    return *reinterpret_cast<T *>(object + offset);
}
std::uintptr_t Main(std::uintptr_t offset) { return exl::util::modules::GetTargetStart() + offset; }
template<class T> T Fn(std::uintptr_t offset) { return reinterpret_cast<T>(Main(offset)); }
struct Session {
    std::uintptr_t owner{};
    Bag bag{};
    PressEdge edge{};
    int focus = -1, inputRow = -1;
    bool dirty = true;
    const char *notice = "Y: Add/remove bag (test)";
    PressEdge backEdge{};
    std::uintptr_t backDialog{};
    int backRow = -1;
};
Session session;
PurchasePlan purchase;
struct NativeReferences {
    Item *send[game::DeliveryLimit]{}, *wear[game::DeliveryLimit]{};
} purchaseRefs;
BadgeMotions motions{};
unsigned nextMotion{};
bool HasBadgeMotion() { return HasBagBadgeMotion(motions); }
void AdvanceBadgeMotion() { AdvanceBagBadgeMotions(motions); }
constexpr unsigned DeliveryLimit = game::DeliveryLimit;
using InputFn = void (*)(std::uintptr_t, std::uintptr_t, unsigned);
using PriceFn = int (*)(std::uintptr_t);
InputFn originalInput{};

bool IsOwner(std::uintptr_t owner) {
    return owner && owner == session.owner && At<std::uintptr_t>(owner) == Main(address::ControllerVtable);
}
bool StockItem(std::uintptr_t owner, int index, Item &item) {
    const unsigned count = At<std::uint16_t>(owner, field::StockCount);
    const auto stock = At<std::uintptr_t>(owner, field::Stock);
    if (!stock || index < 0 || unsigned(index) >= count || count > 4096) return false;
    const auto pointer = At<std::uintptr_t>(stock, unsigned(index) * 8);
    if (!pointer) return false;
    item = At<Item>(pointer);
    return item.id < 0xfffe;
}
bool DisplayedItem(std::uintptr_t owner, int index, Item &item) {
    if (index < 0 || index >= At<int>(owner, field::IndexCount) ||
        index >= At<int>(owner, field::IndexCapacity)) return false;
    const auto indices = At<std::uintptr_t>(owner, field::Indices);
    return indices && StockItem(owner, At<std::int16_t>(indices, index * 2), item);
}
unsigned OutfitItems(std::uintptr_t owner, Item *items) {
    unsigned count = 0;
    for (unsigned i = 0; i < 10; ++i) {
        Item item;
        // +4 is the persistent stock index, +0 is the category-filtered index.
        if (StockItem(owner, At<int>(owner, field::Outfit + i * 12 + 4), item) &&
            !Bag::Contains(items, count, item)) items[count++] = item;
    }
    return count;
}
// Match the native stack list initialized at 03114268. Nodes are 8-byte item
// storage: their next pointers are overwritten when native collection fills them.
struct NativeList {
    int count = 0, capacity = 10;
    Item **items{};
    void *free{}, *tail{};
    std::array<std::uintptr_t, 10> nodes{};
    std::array<Item *, 10> pointers{};
    NativeList() {
        items = pointers.data(); free = tail = nodes.data();
        for (unsigned i = 0; i + 1 < nodes.size(); ++i)
            nodes[i] = reinterpret_cast<std::uintptr_t>(&nodes[i + 1]);
    }
};
static_assert(offsetof(NativeList, nodes) == 0x20 && offsetof(NativeList, pointers) == 0x70);
int DeliveryCount(std::uintptr_t owner, bool wear) {
    NativeList list;
    Fn<void (*)(std::uintptr_t, NativeList *, unsigned)>(address::Collect)(owner, &list, wear);
    return list.count >= 0 && list.count <= 10 ? list.count : -1;
}
int ItemPrice(const Item &item) {
    // Read-only synthetic outfit for the native price helper. It reads the
    // stock-pointer field and ten (display index, stock index, ticket flag) rows.
    alignas(8) std::byte data[0xb8]{};
    const Item *items[]{&item};
    const auto state = reinterpret_cast<std::uintptr_t>(data);
    At<const Item **>(state, 0x10) = items;
    for (unsigned i = 0; i < 10; ++i) {
        At<int>(state, 0x3c + i * 12) = -1;
        At<int>(state, 0x40 + i * 12) = -1;
    }
    At<int>(state, 0x40) = 0;
    return Fn<PriceFn>(address::Price)(state);
}

unsigned Wallet(std::uintptr_t owner) {
    const auto player = At<std::uint8_t>(owner, field::Player);
    return player < 8 ? qol::shopping::AvailableBells(&player) : 0;
}
unsigned ErrorDialog(std::uintptr_t owner, std::uintptr_t output, unsigned code) {
    At<unsigned>(owner, 0x1c4f0) = code;
    At<std::uint8_t>(output) = At<std::uint8_t>(owner, field::Player);
    At<std::uintptr_t>(output, 0x10) = At<std::uintptr_t>(Main(address::DialogTable), code * 0x10);
    At<std::uintptr_t>(output, 0x28) = owner + 0x1c4d0;
    return 1;
}

bool Named(std::uintptr_t pane, const char *name) {
    return !std::strncmp(reinterpret_cast<const char *>(pane + 0xb0), name, 24);
}
std::uintptr_t DirectChild(std::uintptr_t pane, const char *name) {
    auto sentinel = pane + 0x20;
    auto node = At<std::uintptr_t>(sentinel, 8);
    for (unsigned n = 0; node && node != sentinel && n < 128; ++n) {
        const auto child = node - 8;
        if (At<std::uintptr_t>(child, 0x18) != pane) return 0;
        if (Named(child, name)) return child;
        node = At<std::uintptr_t>(node, 8);
    }
    return 0;
}
std::uintptr_t PricePane(std::uintptr_t row) {
    const auto bindings = At<std::uintptr_t>(row, 0x2e0);
    const auto binding = bindings ? At<std::uintptr_t>(bindings) : 0;
    const auto adapter = binding ? At<std::uintptr_t>(binding, 0x48) : 0;
    if (!adapter) return 0;
    const auto payload = At<std::uintptr_t>(adapter, 8), methods = At<std::uintptr_t>(adapter, 0x18);
    if (!payload || !methods) return 0;
    const auto setter = At<std::uintptr_t>(methods, 0x5e0);
    if (setter == Main(address::SetText)) return payload;
    if (setter != Main(address::SetGroupText)) return 0;
    const auto count = At<unsigned>(payload, 0x10);
    const auto offset = At<int>(payload, 0x14);
    if (count > 16 || offset < 0 || offset > 128) return 0;
    auto node = At<std::uintptr_t>(payload, 8);
    for (unsigned i = 0; i < count && node && node != payload; ++i) {
        const auto member = node - offset;
        const auto table = At<std::uintptr_t>(member, 0x18);
        if (table && At<std::uintptr_t>(table, 0x5e0) == Main(address::SetText))
            return At<std::uintptr_t>(member, 0x10);
        node = At<std::uintptr_t>(node, 8);
    }
    return 0;
}
std::uintptr_t RowRoot(std::uintptr_t row) {
    auto pane = PricePane(row);
    for (unsigned depth = 0; pane && depth < 12; ++depth) {
        if (DirectChild(pane, "B_Btn_00")) return pane;
        if (Named(pane, "RootPane") || !std::strncmp(reinterpret_cast<const char *>(pane + 0xb0), "L_Btn_", 6)) break;
        pane = At<std::uintptr_t>(pane, 0x18);
    }
    return 0;
}
std::uintptr_t Find(std::uintptr_t root, const char *name) {
    return root ? Fn<std::uintptr_t (*)(std::uintptr_t, const char *, bool)>(address::FindPane)(root, name, true) : 0;
}
void UpdateBadge(std::uintptr_t row, const Item *item) {
    const auto root = RowRoot(row), pane = Find(root, "N_Bag_00");
    const bool visible = item && session.bag.Find(*item) >= 0;
    if (pane) {
        const auto visual = BagBadgeVisual(session.bag, motions, item);
        const bool shown = visual.shown;
        const float scale = visual.scale;
        const auto alpha = visual.alpha;
        // Native matrix propagation 0027e470..0027e49c requires bit 4.
        // Without it the renderer can retain the initial half-scale matrix
        // until a parent animation (such as moving the cursor) invalidates it.
        const bool changed = At<float>(pane, 0x48) != scale || At<float>(pane, 0x4c) != scale;
        At<std::uint8_t>(pane, 0x58) = (At<std::uint8_t>(pane, 0x58) & ~1u)
            | unsigned(shown) | (changed ? 0x10u : 0u);
        // Pane constructor 0027bfe4..0027c068 maps scale to +48/+4c and alpha +59.
        At<float>(pane, 0x48) = At<float>(pane, 0x4c) = scale;
        At<std::uint8_t>(pane, 0x59) = alpha;
    }
    static unsigned reports{};
    if (reports < 48) {
        ++reports;
        Logging.Debug("[QoL Enhancements/Fitting Room] BADGE item=%u visible=%d found=%d\n", item ? item->id : 65535, visible, pane != 0);
    }
}
int RowLogicalIndex(std::uintptr_t list, int row) {
    const int columns = At<int>(list, 0x2e8);
    if (columns < 1 || columns > 32) return -1;
    const int rowLine = row / columns;
    int first = At<int>(list, 0x430) - At<int>(list, 0x438);
    int line = first + rowLine;
    if (At<std::uint8_t>(list, 0x3e8) & 2) {
        const int span = At<int>(list, 0x434), totalLines = At<int>(list, 0x3d0);
        if (totalLines < 1 || totalLines > 4096 || span < 0 || span > 4096) return -1;
        if (first > span) line += ~span;
        if (first < 0) line += span + 1;
        if (line < 0) line += totalLines;
        else if (line >= totalLines) line -= totalLines;
    }
    const int index = line * columns + row % columns;
    return index >= 0 && index < At<int>(list, 0x3cc) ? index : -1;
}
void Refresh(std::uintptr_t owner) {
    bool retryCounter = false;
    const auto page = At<std::uintptr_t>(owner, field::Page);
    const auto list = page ? At<std::uintptr_t>(page, field::List) : 0;
    if (list) {
        const int count = At<int>(list, field::RowCount);
        const auto rows = At<std::uintptr_t>(list, field::Rows);
        if (rows && count > 0 && count <= 128) {
            for (int i = 0; i < count; ++i) {
                const auto row = At<std::uintptr_t>(rows, i * 8);
                if (!row) continue;
                Item item;
                const bool valid = DisplayedItem(owner, RowLogicalIndex(list, i), item);
                UpdateBadge(row, valid ? &item : nullptr);
            }
        }
    }
    // Use the same number-counter transition as native 0311b778..0311b7b0.
    const int total = Fn<PriceFn>(address::Price)(owner + field::PriceState);
    At<int>(owner, field::NativeTotal) = total;
    const auto layout = page ? At<std::uintptr_t>(page, 0x3f8) : 0;
    const auto parameters = layout ? At<std::uintptr_t>(layout) : 0;
    if (parameters) {
        struct String { std::uintptr_t vtable; const char *text; } key{
            At<std::uintptr_t>(Main(address::SafeStringVtableGot)) + 0x10, "/L_TotalBell_00"};
        const auto counter = Fn<std::uintptr_t (*)(std::uintptr_t, const void *)>(address::FindParameter)(parameters + 0x38, &key);
        if (counter && At<std::uint8_t>(counter, 0x290)) {
            retryCounter = At<int>(counter, 0x298) != total;
        }
        if (counter && !At<std::uint8_t>(counter, 0x290)) {
            const int current = At<int>(counter, 0x260);
            if (current != total || At<int>(counter, 0x264) != total) {
                At<int>(counter, 0x294) = current;
                At<int>(counter, 0x298) = total;
                At<std::uint8_t>(counter, 0x290) = 1;
                At<std::uint8_t>(counter, 0x29c) = 1;
            }
        }
    }
    session.dirty = retryCounter;
}
void TapFocusedCursor(std::uintptr_t owner) {
    if (session.inputRow != 0) return;
    const auto page = At<std::uintptr_t>(owner, field::Page);
    const auto list = page ? At<std::uintptr_t>(page, field::List) : 0;
    if (!list) return;
    const auto global = At<std::uintptr_t>(Main(0x04ab2230));
    const auto app = global ? At<std::uintptr_t>(global) : 0;
    const auto cursor = app ? At<std::uintptr_t>(app, 0xd0) : 0;
    if (!cursor || At<std::uintptr_t>(cursor) != Main(0x04a0ea80) || At<std::uint8_t>(cursor, 0x26c)) return;
    const auto focused = At<std::uintptr_t>(cursor, 0x218);
    // Only animate a cursor belonging to the currently selected fitting row.
    const auto rows = At<std::uintptr_t>(list, field::Rows);
    const int count = At<int>(list, field::RowCount);
    bool matches = false;
    for (int i=0; rows && i<count && i<128; ++i) if (RowLogicalIndex(list,i)==session.focus) {
        const auto row = At<std::uintptr_t>(rows, i*8);
        const auto root = row ? RowRoot(row) : 0;
        auto pane = focused;
        for (unsigned depth=0; root && pane && depth<16; ++depth) {
            if (pane==root) { matches=true; break; }
            pane=At<std::uintptr_t>(pane,0x18);
        }
    }
    if (!matches) return;
    auto layout = At<std::uintptr_t>(cursor,0x220);
    if (!layout) {
        const auto cursorPage=At<std::uintptr_t>(cursor,0x210);
        layout=cursorPage ? At<std::uintptr_t>(cursorPage,0x570) : 0;
    }
    const auto animation=layout ? At<std::uintptr_t>(layout,0x208) : 0;
    if (!animation) return;
    const auto interface=At<std::uintptr_t>(animation), target=At<std::uintptr_t>(animation,8);
    const auto implementation=At<std::uintptr_t>(animation,0x18);
    if (!interface || !target || !implementation) return;
    struct Operation { std::uintptr_t vtable; int operation, padding; };
    const Operation play{At<std::uintptr_t>(Main(0x04ac4e68))+16,1,0};
    At<void (*)(std::uintptr_t,const Operation *)>(interface,0x18)(animation,&play);
    At<void (*)(std::uintptr_t,float)>(implementation,0x190)(target,1.0f);
}
// Feature diagnostics are synchronous observers, not game operations.
void ReportInvalidPreflight() {
    Logging.Error("[QoL Enhancements/Fitting Room] PREFLIGHT_INVALID feature validation\n");
}
template<class Operations> class NativeSource : public Operations {
protected:
    std::uintptr_t owner;
public:
    explicit NativeSource(std::uintptr_t owner) : owner(owner) {}
    unsigned ReadOutfit(Item *items) override { return OutfitItems(owner, items); }
    bool ReadDeliveryCounts(unsigned &send, unsigned &wear) override {
        const int purchases = DeliveryCount(owner, false), oldClothes = DeliveryCount(owner, true);
        if (purchases < 0 || oldClothes < 0) return false;
        send = purchases; wear = oldClothes;
        return true;
    }
    bool ReadDeliveries(game::DeliveryAlternatives &items) override {
        NativeList send, wear;
        Fn<void (*)(std::uintptr_t, NativeList *, unsigned)>(address::Collect)(owner, &send, 0);
        Fn<void (*)(std::uintptr_t, NativeList *, unsigned)>(address::Collect)(owner, &wear, 1);
        if (send.count < 0 || send.count > 10 || wear.count < 0 || wear.count > 10) return false;
        items.sendCount = send.count; items.wearCount = wear.count;
        for (int i = 0; i < send.count; ++i) {
            if (!send.items[i]) return false;
            items.send[i] = *send.items[i];
        }
        for (int i = 0; i < wear.count; ++i) {
            if (!wear.items[i]) return false;
            items.wear[i] = *wear.items[i];
        }
        return true;
    }
    bool HasStock(const Item &item) override {
        for (unsigned i = 0; i < At<std::uint16_t>(owner, field::StockCount); ++i) {
            Item stock;
            if (StockItem(owner, i, stock) && stock == item) return true;
        }
        return false;
    }
    int Quote(const Item &item) override { return ItemPrice(item); }
    bool ReadSpace(game::Space &space) override {
        std::uint8_t player = At<std::uint8_t>(owner, field::Player);
        if (player >= 8) return false;
        space.storage = At<std::uint8_t>(owner, field::HasStorage)
            ? Fn<unsigned (*)(const void *)>(address::StorageFree)(&player) : 0;
        space.pockets = Fn<unsigned (*)(const void *)>(address::PocketsFree)(&player);
        return space.storage <= 20000 && space.pockets <= 40;
    }
};
class NativeSelection : public NativeSource<game::FittingSelection> {
    std::uintptr_t record;
public:
    NativeSelection(std::uintptr_t owner, std::uintptr_t record)
        : NativeSource(owner), record(record) {}
    bool SelectedItem(Item &item) override { return DisplayedItem(owner, session.focus, item); }
    void ShowSpaceError() override {
        At<unsigned>(owner, 0x1c4f0) = At<std::uint8_t>(owner, field::HasStorage) ? 3 : 2;
        const auto active = At<std::uint8_t>(owner, 0x200);
        if (active <= 1) At<int>(owner, 0x1f0 + active * 4) = 4;
    }
    void PlayCursorTap() override { TapFocusedCursor(owner); }
    void ConsumeToggle() override { At<unsigned>(record, 0x3c) |= 0x80; }
};
class NativeCheckoutContext : public NativeSource<game::Checkout> {
    std::uintptr_t output{};
    unsigned (*answerCallback)(std::uintptr_t, std::uintptr_t, unsigned){};
    void (*checkoutCallback)(std::uintptr_t, unsigned){};
public:
    NativeCheckoutContext(std::uintptr_t owner, std::uintptr_t output = 0,
        unsigned (*answer)(std::uintptr_t, std::uintptr_t, unsigned) = nullptr,
        void (*checkout)(std::uintptr_t, unsigned) = nullptr)
        : NativeSource(owner), output(output), answerCallback(answer), checkoutCallback(checkout) {}
    int TotalPrice() override { return Fn<PriceFn>(address::Price)(owner + field::PriceState); }
    unsigned AvailableBells() override { return Wallet(owner); }
    void StoreTotal(int total) override { At<int>(owner, field::NativeTotal) = total; }
    void OpenConfirmation(int total, unsigned funds, bool affordable) override {
        At<unsigned>(owner, 0x1c4c4) = funds;
        At<unsigned>(owner, 0x1c4f0) = affordable ? 4 : 1;
        const auto active = At<std::uint8_t>(owner, 0x200);
        if (active <= 1 && At<int>(owner, 0x1f0 + active * 4) == -1)
            At<int>(owner, 0x1f0 + active * 4) = 4;
        Logging.Debug("[QoL Enhancements/Fitting Room] CONFIRM total=%d wallet=%u\n", total, funds);
    }
    game::Question CurrentQuestion() override {
        switch (At<unsigned>(owner, 0x1c4f0)) {
        case 4: return game::Question::Purchase;
        case 5: return game::Question::WearOrSend;
        default: return game::Question::Other;
        }
    }
    bool IdleCommand() override { return At<int>(owner, field::Command) == -1; }
    bool OutfitSlotsEmpty() override {
        // Persistent stock indices, not failed/unresolved stock lookups.
        bool empty = true;
        for (unsigned i = 0; i < 10; ++i) empty &= At<int>(owner, field::Outfit + i * 12 + 4) == -1;
        return empty;
    }
    void SelectSendQuestion() override { At<unsigned>(owner, 0x1c4f0) = 5; }
    unsigned FundsError() override { return ErrorDialog(owner, output, 1); }
    unsigned SpaceError() override {
        return ErrorDialog(owner, output, At<std::uint8_t>(owner, field::HasStorage) ? 3 : 2);
    }
    unsigned ReturnToBrowsing() override { At<int>(owner, field::Command) = -1; return 0; }
    unsigned NativeAnswer(unsigned answer) override { return answerCallback(owner, output, answer); }
    void NativeCheckout(unsigned wear) override {
        if (purchase.active)
            Logging.Debug("[QoL Enhancements/Fitting Room] COMMIT_BEGIN wear=%u total=%d\n",
                          wear & 1, At<int>(owner, field::NativeTotal));
        checkoutCallback(owner, wear);
    }
    void CheckoutReturned() override {
        session.dirty = false;
        Logging.Debug("[QoL Enhancements/Fitting Room] COMMIT_RETURNED\n");
    }
};
void HandleInput(std::uintptr_t page, std::uintptr_t snapshot, unsigned blocked) {
    const auto owner = session.owner;
    if (!snapshot || !IsOwner(owner) || At<std::uintptr_t>(owner, field::Page) != page) return;
    const auto hint = At<std::uintptr_t>(page, 0x570);
    const auto input = hint ? At<std::uintptr_t>(hint, 0xa0) : 0;
    if (!input || At<int>(input, 0x14) != 5) return;
    const int row = At<int>(input, 0x10), count = At<int>(snapshot, 8);
    const auto rows = At<std::uintptr_t>(snapshot, 0x10);
    if (!rows || count < 1 || count > 8 || row < 0 || row >= count) return;
    if (row != session.inputRow) { session.edge.Reset(); session.inputRow = row; }
    const auto record = rows + row * 0x48;
    const auto buttons = At<unsigned>(record);
    const bool pressed = session.edge.Update(buttons & 0x80);
    const auto context = At<std::uintptr_t>(page, 0x210);
    const auto app = At<std::uintptr_t>(Main(address::Application));
    const auto masks = app ? At<std::uintptr_t>(app, 0x418) : 0;
    static unsigned inputReports{};
    if (pressed && inputReports++ < 64) {
        Logging.Debug("[QoL Enhancements/Fitting Room] INPUT state=%d command=%d focus=%d held=%x consumed=%x blocked=%u\n",
                    At<int>(owner, field::State), At<int>(owner, field::Command), session.focus,
                    buttons, At<unsigned>(record, 0x3c), blocked);
    }
    const bool allowed = !(blocked & 1) && context && masks &&
        !At<std::uint8_t>(context, 0x54) &&
        !(At<unsigned>(context, 0x58) & At<unsigned>(masks, 0x10));
    const bool browsing = At<int>(owner, field::State) == 3 &&
        At<int>(owner, field::Command) == -1 && At<int>(owner, 0x1f0) == -1 && At<int>(owner, 0x1f4) == -1;
    if (!BagInputEligible(pressed, allowed, At<unsigned>(record, 0x3c) & 0x80, buttons & 0x7f, browsing)) return;
    NativeSelection selection(owner, record);
    const auto outcome = ToggleBagSelection(session.bag, motions, nextMotion, selection);
    if (!outcome.handled) return;
    session.notice = outcome.result == ToggleResult::Full ? "No room in storage/pockets" :
                     outcome.result == ToggleResult::Invalid ? "Cannot add this item" : "Y: Add/remove bag (test)";
    session.dirty = true;
    Logging.Debug("[QoL Enhancements/Fitting Room] TOGGLE item=%u extra=%x result=%u count=%u price=%d\n",
                outcome.item.id, outcome.item.extra, unsigned(outcome.result), session.bag.count, outcome.price);
}
void Input(std::uintptr_t page, std::uintptr_t snapshot, unsigned blocked) {
    HandleInput(page, snapshot, blocked);
    originalInput(page, snapshot, blocked);
}
// The shared dialog's two-choice buttons are No (+578) and Yes (+588),
// with alternate-style copies eight bytes later. Native answer mapping sends
// button indices 0/1 as answer 0 (send), and 2/3 as answer 1 (wear).
// Only the fitting room's live wear-choice delegate may use this shortcut.
std::uintptr_t HandleWearChoiceBack(std::uintptr_t page, std::uintptr_t snapshot, unsigned blocked) {
    constexpr unsigned Back = 1u << 5, Accept = 1u << 4;
    // Live Eden snapshots accompany A, B and direction presses with bit 16.
    // It is not an additional choice button. Preserve it for native dispatch,
    // while still rejecting all other simultaneous input bits.
    constexpr unsigned AccompanyingInputFlag = 1u << 16;
    const auto owner = session.owner;
    const auto dialog = At<std::uintptr_t>(Main(address::DialogInstance));
    if (!snapshot || !IsOwner(owner) || !dialog ||
        At<std::uintptr_t>(dialog) != Main(address::DialogVtable) ||
        At<std::uintptr_t>(dialog, field::Page) != page ||
        At<std::uintptr_t>(dialog, 0x2388) != owner + 0x1c4d0 ||
        At<unsigned>(owner, 0x1c4f0) != 5 ||
        At<int>(dialog, field::State) != 3 ||
        At<int>(dialog, 0x1f0) != -1 || At<int>(dialog, 0x1f4) != -1 ||
        At<int>(dialog, 0x220) != -1) {
        session.backEdge.Reset(); session.backDialog = 0; session.backRow = -1;
        return 0;
    }
    const auto hint = At<std::uintptr_t>(page, 0x570);
    const auto input = hint ? At<std::uintptr_t>(hint, 0xa0) : 0;
    if (!input || At<int>(input, 0x14) != 5) return 0;
    const int row = At<int>(input, 0x10), count = At<int>(snapshot, 8);
    const auto rows = At<std::uintptr_t>(snapshot, 0x10);
    if (!rows || count < 1 || count > 8 || row < 0 || row >= count) return 0;
    if (session.backDialog != dialog || session.backRow != row) {
        session.backEdge.Reset(); session.backDialog = dialog; session.backRow = row;
    }
    const auto record = rows + row * 0x48;
    const unsigned buttons = At<unsigned>(record);
    const bool pressed = session.backEdge.Update(buttons & Back);
    const auto context = At<std::uintptr_t>(page, 0x210);
    const auto app = At<std::uintptr_t>(Main(address::Application));
    const auto masks = app ? At<std::uintptr_t>(app, 0x418) : 0;
    const bool allowed = !(blocked & 1) && context && masks &&
        !At<std::uint8_t>(context, 0x54) &&
        !(At<unsigned>(context, 0x58) & At<unsigned>(masks, 0x10));
    class NativeChoice : public game::WearChoice {
        std::uintptr_t page, dialog, context, record, no{};
        int row;
    public:
        NativeChoice(std::uintptr_t page, std::uintptr_t dialog, std::uintptr_t context,
                     std::uintptr_t record, int row)
            : page(page), dialog(dialog), context(context), record(record), row(row) {}
        game::ChoiceFocus CurrentFocus() override {
            const unsigned style = At<unsigned>(dialog, 0x22c);
            if (style > 2 || !At<std::uintptr_t>(dialog, 0x248) ||
                !At<std::uintptr_t>(dialog, 0x250) || At<std::uintptr_t>(dialog, 0x258)) return game::ChoiceFocus::Unavailable;
            no = At<std::uintptr_t>(page, 0x578 + (style == 2 ? 8 : 0));
            const auto yes = At<std::uintptr_t>(page, 0x588 + (style == 2 ? 8 : 0));
            const auto noCursor = no ? At<std::uintptr_t>(no, 0x218) : 0;
            const auto yesCursor = yes ? At<std::uintptr_t>(yes, 0x218) : 0;
            const auto manager = At<std::uintptr_t>(context, 0x48);
            if (!noCursor || !yesCursor || !manager ||
                At<unsigned>(manager, 0x18) <= unsigned(row) ||
                !(At<unsigned>(noCursor, 0x14b0) & (1u << row))) return game::ChoiceFocus::Unavailable;
            const auto cursors = At<std::uintptr_t>(manager, 0x20);
            const auto cursor = cursors ? At<std::uintptr_t>(cursors, row * 8) : 0;
            const auto focused = cursor ? At<std::uintptr_t>(cursor, 0x10) : 0;
            if (focused != noCursor && focused != yesCursor) return game::ChoiceFocus::Unavailable;
            return focused == noCursor ? game::ChoiceFocus::Send : game::ChoiceFocus::Wear;
        }
        void ConsumeBack() override { At<unsigned>(record, 0x3c) |= Back; }
        void FocusSend() override {
            struct FocusRequest { std::uintptr_t page, button; int row, padding; };
            const FocusRequest request{page, no, row, 0};
            Fn<void (*)(const FocusRequest *)>(address::FocusButton)(&request);
        }
    } choice(page, dialog, context, record, row);
    const auto action = HandleBackShortcut(pressed, allowed,
        At<unsigned>(record, 0x3c) & (Back | Accept),
        (buttons & ~AccompanyingInputFlag) == Back, choice);
    return action == BackAction::ConfirmSend ? record : 0;
}
void WearChoiceInput(std::uintptr_t page, std::uintptr_t snapshot, unsigned blocked) {
    const auto record = HandleWearChoiceBack(page, snapshot, blocked);
    if (!record) { originalInput(page, snapshot, blocked); return; }
    // Forward one native A press only when No is already focused. Restore the
    // shared snapshot immediately; other pages must never see a fabricated A.
    // Held B cannot generate another press: backEdge requires a release.
    constexpr unsigned Back = 1u << 5, Accept = 1u << 4;
    const auto buttons = At<unsigned>(record);
    const auto consumed = At<unsigned>(record, 0x3c);
    At<unsigned>(record) = (buttons & ~Back) | Accept;
    originalInput(page, snapshot, blocked);
    At<unsigned>(record) = buttons;
    At<unsigned>(record, 0x3c) = (At<unsigned>(record, 0x3c) & ~Accept) | (consumed & Accept);
}
HOOK_DEFINE_TRAMPOLINE(Begin) {
    static void Callback(std::uintptr_t owner) {
        session = {}; purchase = {}; purchaseRefs = {}; motions = {}; nextMotion = 0; session.owner = owner;
        Orig(owner);
        session.dirty = true;
        Logging.Debug("[QoL Enhancements/Fitting Room] SESSION_BEGIN player=%u\n", At<std::uint8_t>(owner, field::Player));
    }
};
HOOK_DEFINE_TRAMPOLINE(Destroy) {
    static void Callback(std::uintptr_t owner) {
        if (session.owner == owner) { session = {}; purchase = {}; purchaseRefs = {}; motions = {}; nextMotion = 0; }
        Orig(owner);
    }
};
HOOK_DEFINE_TRAMPOLINE(Focus) {
    static void Callback(std::uintptr_t owner, int index) {
        if (IsOwner(owner)) session.focus = index;
        Orig(owner, index);
    }
};
HOOK_DEFINE_TRAMPOLINE(Row) {
    static void Callback(std::uintptr_t owner, std::uintptr_t event) {
        Orig(owner, event);
        if (!IsOwner(owner) || !event) return;
        const int poolIndex = At<int>(event, 4);
        const auto descriptor = At<std::uintptr_t>(event, 0x10);
        const auto list = descriptor ? At<std::uintptr_t>(descriptor) : 0;
        if (!list || poolIndex < 0 || poolIndex >= At<int>(list, 0x208) || poolIndex >= 128) return;
        const auto rows = At<std::uintptr_t>(list, 0x210);
        const auto row = rows ? At<std::uintptr_t>(rows, poolIndex * 8) : 0;
        if (!row) return;
        Item item;
        const bool valid = DisplayedItem(owner, poolIndex + At<int>(descriptor, 0xc), item);
        UpdateBadge(row, valid ? &item : nullptr);
    }
};
HOOK_DEFINE_TRAMPOLINE(Price) {
    static int Callback(std::uintptr_t state) {
        const int native = Orig(state);
        if (!session.owner || state != session.owner + field::PriceState) return native;
        Item outfit[10]{};
        const unsigned count = OutfitItems(session.owner, outfit);
        return PriceWithBag(native, session.bag, outfit, count);
    }
};
HOOK_DEFINE_TRAMPOLINE(Collect) {
    static void Callback(std::uintptr_t owner, NativeList *list, unsigned wear) {
        if (!IsOwner(owner) || !purchase.active) { Orig(owner, list, wear); return; }
        // Replace only the list header. The native loops consume the pointer
        // array; they never append to or destroy these lists after collection.
        // Both arrays live through the entire synchronous checkout call.
        auto *items = (wear & 1) ? purchase.wear : purchase.send;
        auto *refs = (wear & 1) ? purchaseRefs.wear : purchaseRefs.send;
        const auto count = (wear & 1) ? purchase.wearCount : purchase.sendCount;
        EXL_ASSERT(list && list->count == 0 && list->capacity == 10 && count <= DeliveryLimit);
        for (unsigned i = 0; i < count; ++i) refs[i] = &items[i];
        list->items = refs;
        list->capacity = DeliveryLimit;
        list->count = count;
        list->free = list->tail = nullptr;
        Logging.Debug("[QoL Enhancements/Fitting Room] COMMIT_LIST wear=%u count=%u\n", wear & 1, count);
    }
};
HOOK_DEFINE_TRAMPOLINE(PurchaseRecord) {
    static void Callback(std::uintptr_t recorder, std::uintptr_t record) {
        if (!purchase.active || At<unsigned>(record, 0xc) <= 16) { Orig(recorder, record); return; }
        // Native record serialization has only 16 item-detail slots. Payment
        // totals stay complete; keep this diagnostic record a valid 16-item
        // preview. Catalog registration and per-item milestones use other loops.
        alignas(8) std::byte bounded[0x98];
        std::memcpy(bounded, reinterpret_cast<const void *>(record), sizeof(bounded));
        At<unsigned>(reinterpret_cast<std::uintptr_t>(bounded), 0xc) = 16;
        Orig(recorder, reinterpret_cast<std::uintptr_t>(bounded));
        Logging.Debug("[QoL Enhancements/Fitting Room] RECORD_PREVIEW items=%u details=16\n", purchase.sendCount);
    }
};
HOOK_DEFINE_TRAMPOLINE(GrossPrice) {
    static int Callback(std::uintptr_t state) {
        const int native = Orig(state);
        return GrossPriceWithBag(native, purchase, state == session.owner + field::PriceState);
    }
};
HOOK_DEFINE_TRAMPOLINE(Dialog) {
    static unsigned Callback(std::uintptr_t owner, std::uintptr_t output, unsigned answer) {
        if (!IsOwner(owner)) return Orig(owner, output, answer);
        NativeCheckoutContext checkout(owner, output,
            [](auto object, auto result, auto choice) { return Orig(object, result, choice); });
        return AnswerBagConfirmation(session.bag, purchase, checkout, answer, ReportInvalidPreflight);
    }
};
HOOK_DEFINE_TRAMPOLINE(Play) {
    static void Callback(std::uintptr_t owner) {
        if (IsOwner(owner)) {
            NativeCheckoutContext checkout(owner);
            if (OpenBagConfirmation(session.bag, purchase, checkout,
                                    At<int>(owner, field::Command) == 1, ReportInvalidPreflight)) return;
        }
        Orig(owner);
        if (IsOwner(owner) && (session.dirty || HasBadgeMotion())) {
            Refresh(owner);
            AdvanceBadgeMotion();
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(Checkout) {
    static void Callback(std::uintptr_t owner, unsigned wear) {
        if (!IsOwner(owner)) { Orig(owner, wear); return; }
        NativeCheckoutContext checkout(owner, 0, nullptr,
            [](auto object, auto choice) { Orig(object, choice); });
        const auto step = CommitBagCheckout(session.bag, purchase, checkout, wear, ReportInvalidPreflight);
        if (step == CheckoutStep::Rejected)
            Logging.Debug("[QoL Enhancements/Fitting Room] COMMIT_REJECTED\n");
    }
};
} // namespace
bool Install() {
    if (!ValidateFittingHint()) return false;
    struct Fingerprint { std::uintptr_t address; std::array<std::uint32_t, 4> words; };
    // Generated from the SHA256-checked local 3.0.3 mapped main; all sites must
    // pass before installing any hook. The page input body can already be hooked
    // by QoL Pockets; only our private vtable slot must still be original.
    const Fingerprint checks[]{
#include "program/fitting/game/fingerprints.inc"
    };
    const auto &module = exl::util::GetMainModuleInfo();
    for (const auto &check : checks) {
        if (check.address + sizeof(check.words) > module.m_Text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(Main(check.address)), check.words.data(), sizeof(check.words))) return false;
    }
    const auto slot = Main(address::InputSlot);
    // The vtable is in writable .data, outside Exlaunch's text/rodata alias.
    if (!module.m_Data.InRange(slot) ||
        !module.m_Data.InRange(slot + sizeof(std::uintptr_t) - 1) ||
        At<std::uintptr_t>(slot) != Main(address::OriginalInput)) return false;
    const auto dialogSlot = Main(address::DialogInputSlot);
    if (!module.m_Data.InRange(dialogSlot) ||
        !module.m_Data.InRange(dialogSlot + sizeof(std::uintptr_t) - 1) ||
        At<std::uintptr_t>(dialogSlot) != Main(address::OriginalInput)) return false;

    qol::core::HookPlan hookPlan{
        qol::core::Site<Begin>("Begin", address::Begin, {0xa9ba7bfd, 0xa9016ffc, 0x910003fd, 0xa90267fa, 0xa9035ff8}, true),
        qol::core::Site<Destroy>("Destroy", address::Destroy, {0xa9bd7bfd, 0xa90157f6, 0xa9024ff4, 0x910003fd, 0x91407008}, true),
        qol::core::Site<Focus>("Focus", address::Focus, {0xd10303ff, 0xa9087bfd, 0xf9004bf7, 0xa90a57f6, 0xa90b4ff4}, true),
        qol::core::Site<Row>("Row", address::Row, {0xa9ba7bfd, 0xf9000bfc, 0x910003fd, 0xa90267fa, 0xa9035ff8}, true),
        qol::core::Site<Price>("Price", address::Price, {0xd102c3ff, 0xa9057bfd, 0xa9066ffc, 0xa90767fa, 0xa9085ff8}, true),
        qol::core::Site<Play>("Play", address::Play, {0xd106c3ff, 0xa9157bfd, 0xa9166ffc, 0xa91767fa, 0xa9185ff8}, true),
        qol::core::Site<Checkout>("Checkout", address::Checkout, {0xa9ba7bfd, 0xa9016ffc, 0x910003fd, 0xa90267fa, 0xa9035ff8}, true),
        qol::core::Site<Collect>("Collect", address::Collect, {0xd10543ff, 0xa90f7bfd, 0xa9106ffc, 0xa91167fa, 0xa9125ff8}, true),
        qol::core::Site<Dialog>("Dialog", address::Dialog, {0xd10403ff, 0xa90c7bfd, 0xa90d5ff8, 0xa90e57f6, 0xa90f4ff4}, true),
        qol::core::Site<GrossPrice>("GrossPrice", address::GrossPrice, {0xd102c3ff, 0xa9057bfd, 0xa9066ffc, 0xa90767fa, 0xa9085ff8}, true),
        qol::core::Site<PurchaseRecord>("PurchaseRecord", address::PurchaseRecord, {0xd10103ff, 0xa9017bfd, 0xa90257f6, 0xa9034ff4, 0x910043fd}, true),
    };
    if (!hookPlan) return false;
    if (!InstallFittingHint()) return false;
    auto *inputSlot = reinterpret_cast<volatile std::uintptr_t *>(slot);
    const auto replacement = reinterpret_cast<std::uintptr_t>(&Input);
    qol::core::RecordMutation();
    *inputSlot = replacement;
    if (*inputSlot != replacement) {
        qol::core::ReportActivationFailure(qol::core::FeatureState::Failed, "fitting input slot readback failed");
        Logging.Error("[QoL Enhancements/Fitting Room] INPUT_HOOK_FAILED readback\n");
        return false;
    }
    Logging.Debug("[QoL Enhancements/Fitting Room] INPUT_HOOK_OK writable-data readback\n");
    originalInput = Fn<InputFn>(address::OriginalInput);
    qol::core::RecordMutation();
    At<std::uintptr_t>(dialogSlot) = reinterpret_cast<std::uintptr_t>(&WearChoiceInput);
    if (!hookPlan.Publish<Begin>(address::Begin)) return false;
    if (!hookPlan.Publish<Destroy>(address::Destroy)) return false;
    if (!hookPlan.Publish<Focus>(address::Focus)) return false;
    if (!hookPlan.Publish<Row>(address::Row)) return false;
    if (!hookPlan.Publish<Price>(address::Price)) return false;
    if (!hookPlan.Publish<Play>(address::Play)) return false;
    if (!hookPlan.Publish<Checkout>(address::Checkout)) return false;
    if (!hookPlan.Publish<Collect>(address::Collect)) return false;
    if (!hookPlan.Publish<Dialog>(address::Dialog)) return false;
    if (!hookPlan.Publish<GrossPrice>(address::GrossPrice)) return false;
    if (!hookPlan.Publish<PurchaseRecord>(address::PurchaseRecord)) return false;
    return true;
}
} // namespace able_bag
