#include "lib.hpp"
#include "bag_policy.hpp"
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
};
Session session;
// Freeze both native delivery alternatives before any payment or clothing swap.
enum class Preflight { Invalid, Ready, NoSpace };
constexpr unsigned DeliveryLimit = Bag::Limit + 10;
struct PurchasePlan {
    bool active{}, done{};
    unsigned sendCount{}, wearCount{};
    Item send[DeliveryLimit]{}, wear[DeliveryLimit]{};
    Item *sendRefs[DeliveryLimit]{}, *wearRefs[DeliveryLimit]{};
    int extraPrice{};
};
PurchasePlan purchase;
struct BadgeMotion { Item item{}; unsigned frame{}; bool added{}, active{}; };
std::array<BadgeMotion, 16> motions{};
unsigned nextMotion{};
void StartBadgeMotion(const Item &item, bool added) {
    for (auto &motion : motions) if (motion.active && motion.item == item) {
        motion = {item, 0, added, true}; return;
    }
    motions[nextMotion++ % motions.size()] = {item, 0, added, true};
}
bool HasBadgeMotion() {
    for (const auto &motion : motions) if (motion.active) return true;
    return false;
}
void AdvanceBadgeMotion() {
    for (auto &motion : motions) if (motion.active && ++motion.frame > (motion.added ? 7u : 1u)) motion.active = false;
}
float BadgeScale(unsigned frame) {
    // Native EquipIn BFLAN Hermite keys: (frame,value,slope).
    constexpr float keys[][3]{{0,.5f,.26666667f},{3,1.4f,0},{6,.9f,0},{7,1,0}};
    for (unsigned i=0; i<3; ++i) if (float(frame)<=keys[i+1][0]) {
        const float span=keys[i+1][0]-keys[i][0], t=(float(frame)-keys[i][0])/span;
        const float t2=t*t, t3=t2*t;
        return (2*t3-3*t2+1)*keys[i][1]+(t3-2*t2+t)*span*keys[i][2]
             +(-2*t3+3*t2)*keys[i+1][1]+(t3-t2)*span*keys[i+1][2];
    }
    return 1;
}
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
Preflight CanAdd(std::uintptr_t owner, const Item &item) {
    Item outfit[10]{};
    const auto count = OutfitItems(owner, outfit);
    unsigned extras = session.bag.ExtraCount(outfit, count);
    if (!Bag::Contains(outfit, count, item)) ++extras;
    const int purchaseCount = DeliveryCount(owner, false), oldClothes = DeliveryCount(owner, true);
    if (purchaseCount < 0 || oldClothes < 0) return Preflight::Invalid;
    std::uint8_t player = At<std::uint8_t>(owner, field::Player);
    if (player >= 8) return Preflight::Invalid;
    const unsigned storage = At<std::uint8_t>(owner, field::HasStorage)
        ? Fn<unsigned (*)(const void *)>(address::StorageFree)(&player) : 0;
    const unsigned pockets = Fn<unsigned (*)(const void *)>(address::PocketsFree)(&player);
    if (storage > 20000 || pockets > 40) return Preflight::Invalid;
    // Deliberately do not assume payment frees Bell/ticket pocket slots. Those
    // optimizations are deferred until checkout itself is implemented.
    const bool fits = Fits(storage, pockets, oldClothes, purchaseCount, extras);
    Logging.Log("[QoL Enhancements/Fitting Room] CAPACITY storage=%u pockets=%u old=%d purchases=%d extras=%u fits=%d\n",
                storage, pockets, oldClothes, purchaseCount, extras, fits);
    return fits ? Preflight::Ready : Preflight::NoSpace;
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

// Build an immutable, bounded list while the transaction is still read-only.
Preflight InvalidPreflight(unsigned line) {
    Logging.Log("[QoL Enhancements/Fitting Room] PREFLIGHT_INVALID line=%u\n", line);
    return Preflight::Invalid;
}
Preflight PreparePurchase(std::uintptr_t owner) {
    if (purchase.active || purchase.done) return InvalidPreflight(__LINE__);
    purchase = {};
    NativeList send, wear;
    Fn<void (*)(std::uintptr_t, NativeList *, unsigned)>(address::Collect)(owner, &send, 0);
    Fn<void (*)(std::uintptr_t, NativeList *, unsigned)>(address::Collect)(owner, &wear, 1);
    if (send.count < 0 || send.count > 10 || wear.count < 0 || wear.count > 10) return InvalidPreflight(__LINE__);
    purchase.sendCount = send.count; purchase.wearCount = wear.count;
    for (int i = 0; i < send.count; ++i) {
        if (!send.items[i]) return InvalidPreflight(__LINE__);
        purchase.send[i] = *send.items[i];
    }
    for (int i = 0; i < wear.count; ++i) {
        if (!wear.items[i]) return InvalidPreflight(__LINE__);
        purchase.wear[i] = *wear.items[i];
    }
    Item outfit[10]{};
    const auto outfitCount = OutfitItems(owner, outfit);
    // Require the native purchase collector to agree with our outfit view.
    if (outfitCount != unsigned(send.count)) return InvalidPreflight(__LINE__);
    for (unsigned i = 0; i < outfitCount; ++i)
        if (!Bag::Contains(purchase.send, purchase.sendCount, outfit[i])) return InvalidPreflight(__LINE__);
    for (unsigned i = 0; i < session.bag.count; ++i) {
        auto &entry = session.bag.entries[i];
        bool inStock = false;
        for (unsigned j = 0; j < At<std::uint16_t>(owner, field::StockCount); ++j) {
            Item stock;
            if (StockItem(owner, j, stock) && stock == entry.item) { inStock = true; break; }
        }
        if (!inStock) return InvalidPreflight(__LINE__);
        const int quote = ItemPrice(entry.item);
        if (quote < 0 || quote > 1000000) return InvalidPreflight(__LINE__);
        entry.price = quote;
        if (Bag::Contains(outfit, outfitCount, entry.item)) continue;
        if (purchase.sendCount == DeliveryLimit || purchase.wearCount == DeliveryLimit) return InvalidPreflight(__LINE__);
        purchase.send[purchase.sendCount++] = entry.item;
        purchase.wear[purchase.wearCount++] = entry.item;
        purchase.extraPrice += quote;
    }
    std::uint8_t player = At<std::uint8_t>(owner, field::Player);
    if (player >= 8) return InvalidPreflight(__LINE__);
    const unsigned storage = At<std::uint8_t>(owner, field::HasStorage)
        ? Fn<unsigned (*)(const void *)>(address::StorageFree)(&player) : 0;
    const unsigned pockets = Fn<unsigned (*)(const void *)>(address::PocketsFree)(&player);
    if (storage > 20000 || pockets > 40) return InvalidPreflight(__LINE__);
    const unsigned reserve = purchase.sendCount > purchase.wearCount ? purchase.sendCount : purchase.wearCount;
    const bool fits = reserve <= storage + pockets;
    Logging.Log("[QoL Enhancements/Fitting Room] PREFLIGHT send=%u wear=%u storage=%u pockets=%u fits=%d\n",
                purchase.sendCount, purchase.wearCount, storage, pockets, fits);
    return fits ? Preflight::Ready : Preflight::NoSpace;
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
        bool shown = visible;
        float scale = 1;
        unsigned alpha = 255;
        for (const auto &motion : motions) if (motion.active && item && motion.item == *item) {
            if (motion.added) { scale = BadgeScale(motion.frame); alpha = motion.frame ? 255 : 0; }
            else { shown = motion.frame == 0; }
            break;
        }
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
        Logging.Log("[QoL Enhancements/Fitting Room] BADGE item=%u visible=%d found=%d\n", item ? item->id : 65535, visible, pane != 0);
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
void PlayCursorTap(std::uintptr_t owner) {
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
        Logging.Log("[QoL Enhancements/Fitting Room] INPUT state=%d command=%d focus=%d held=%x consumed=%x blocked=%u\n",
                    At<int>(owner, field::State), At<int>(owner, field::Command), session.focus,
                    buttons, At<unsigned>(record, 0x3c), blocked);
    }
    if (!pressed || (blocked & 1) || !context || !masks ||
        At<std::uint8_t>(context, 0x54) || (At<unsigned>(context, 0x58) & At<unsigned>(masks, 0x10)) ||
        (At<unsigned>(record, 0x3c) & 0x80) || (buttons & 0x7f) ||
        At<int>(owner, field::State) != 3 || At<int>(owner, field::Command) != -1 ||
        At<int>(owner, 0x1f0) != -1 || At<int>(owner, 0x1f4) != -1) return;
    Item item;
    if (!DisplayedItem(owner, session.focus, item)) return;
    const bool removing = session.bag.Find(item) >= 0;
    const int price = removing ? 0 : ItemPrice(item);
    const auto capacity = removing ? Preflight::Ready : CanAdd(owner, item);
    const bool limit = !removing && session.bag.count == Bag::Limit;
    const auto result = capacity == Preflight::Invalid || limit ? ToggleResult::Invalid :
        session.bag.Toggle(item, price, capacity == Preflight::Ready);
    if (capacity == Preflight::NoSpace && !limit) {
        // Open the same native error dialog used at checkout, without adding UI.
        At<unsigned>(owner, 0x1c4f0) = At<std::uint8_t>(owner, field::HasStorage) ? 3 : 2;
        const auto active = At<std::uint8_t>(owner, 0x200);
        if (active <= 1) At<int>(owner, 0x1f0 + active * 4) = 4;
    }
    if (result == ToggleResult::Added || result == ToggleResult::Removed) {
        StartBadgeMotion(item, result == ToggleResult::Added);
        PlayCursorTap(owner);
    }
    At<unsigned>(record, 0x3c) |= 0x80;
    session.notice = result == ToggleResult::Full ? "No room in storage/pockets" :
                     result == ToggleResult::Invalid ? "Cannot add this item" : "Y: Add/remove bag (test)";
    session.dirty = true;
    Logging.Log("[QoL Enhancements/Fitting Room] TOGGLE item=%u extra=%x result=%u count=%u price=%d\n",
                item.id, item.extra, unsigned(result), session.bag.count, price);
}
void Input(std::uintptr_t page, std::uintptr_t snapshot, unsigned blocked) {
    HandleInput(page, snapshot, blocked);
    originalInput(page, snapshot, blocked);
}
HOOK_DEFINE_TRAMPOLINE(Begin) {
    static void Callback(std::uintptr_t owner) {
        session = {}; purchase = {}; motions = {}; nextMotion = 0; session.owner = owner;
        Orig(owner);
        session.dirty = true;
        Logging.Log("[QoL Enhancements/Fitting Room] SESSION_BEGIN player=%u\n", At<std::uint8_t>(owner, field::Player));
    }
};
HOOK_DEFINE_TRAMPOLINE(Destroy) {
    static void Callback(std::uintptr_t owner) {
        if (session.owner == owner) { session = {}; purchase = {}; motions = {}; nextMotion = 0; }
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
        return native + session.bag.ExtraPrice(outfit, count);
    }
};
HOOK_DEFINE_TRAMPOLINE(Collect) {
    static void Callback(std::uintptr_t owner, NativeList *list, unsigned wear) {
        if (!IsOwner(owner) || !purchase.active) { Orig(owner, list, wear); return; }
        // Replace only the list header. The native loops consume the pointer
        // array; they never append to or destroy these lists after collection.
        // Both arrays live through the entire synchronous checkout call.
        auto *items = (wear & 1) ? purchase.wear : purchase.send;
        auto *refs = (wear & 1) ? purchase.wearRefs : purchase.sendRefs;
        const auto count = (wear & 1) ? purchase.wearCount : purchase.sendCount;
        EXL_ASSERT(list && list->count == 0 && list->capacity == 10 && count <= DeliveryLimit);
        for (unsigned i = 0; i < count; ++i) refs[i] = &items[i];
        list->items = refs;
        list->capacity = DeliveryLimit;
        list->count = count;
        list->free = list->tail = nullptr;
        Logging.Log("[QoL Enhancements/Fitting Room] COMMIT_LIST wear=%u count=%u\n", wear & 1, count);
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
        Logging.Log("[QoL Enhancements/Fitting Room] RECORD_PREVIEW items=%u details=16\n", purchase.sendCount);
    }
};
HOOK_DEFINE_TRAMPOLINE(GrossPrice) {
    static int Callback(std::uintptr_t state) {
        const int native = Orig(state);
        return purchase.active && state == session.owner + field::PriceState
            ? native + purchase.extraPrice : native;
    }
};
HOOK_DEFINE_TRAMPOLINE(Dialog) {
    static unsigned Callback(std::uintptr_t owner, std::uintptr_t output, unsigned answer) {
        // State 5 is the native wear/send choice, after payment confirmation.
        if (IsOwner(owner) && session.bag.count && At<unsigned>(owner, 0x1c4f0) == 5) {
            const auto result = PreparePurchase(owner);
            const int total = Fn<PriceFn>(address::Price)(owner + field::PriceState);
            At<int>(owner, field::NativeTotal) = total;
            if (total < 0 || unsigned(total) > Wallet(owner)) return ErrorDialog(owner, output, 1);
            if (result == Preflight::Invalid) {
                At<int>(owner, field::Command) = -1;
                return 0; // Close confirmation, return to browsing; no false capacity error.
            }
            if (result == Preflight::NoSpace) return ErrorDialog(owner, output, At<std::uint8_t>(owner, field::HasStorage) ? 3 : 2);
        }
        return Orig(owner, output, answer);
    }
};
HOOK_DEFINE_TRAMPOLINE(Play) {
    static void Callback(std::uintptr_t owner) {
        if (IsOwner(owner) && session.bag.count && At<int>(owner, field::Command) == 1) {
            // Native code tests only outfit slots before opening confirmation.
            // Mirror that transition for bag-only purchases as well.
            PreparePurchase(owner);
            const int total = Fn<PriceFn>(address::Price)(owner + field::PriceState);
            const auto wallet = Wallet(owner);
            At<int>(owner, field::NativeTotal) = total;
            At<unsigned>(owner, 0x1c4c4) = wallet;
            At<unsigned>(owner, 0x1c4f0) = total < 0 || unsigned(total) > wallet ? 1 : 4;
            const auto active = At<std::uint8_t>(owner, 0x200);
            if (active <= 1 && At<int>(owner, 0x1f0 + active * 4) == -1)
                At<int>(owner, 0x1f0 + active * 4) = 4;
            Logging.Log("[QoL Enhancements/Fitting Room] CONFIRM total=%d wallet=%u\n", total, wallet);
            return;
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
        if (purchase.active || purchase.done) return; // No reentrant or repeated payment.
        if (!session.bag.count) { Orig(owner, wear); return; }
        const auto result = PreparePurchase(owner);
        const int total = Fn<PriceFn>(address::Price)(owner + field::PriceState);
        if (result != Preflight::Ready || total < 0 || unsigned(total) > Wallet(owner)) {
            Logging.Log("[QoL Enhancements/Fitting Room] COMMIT_REJECTED result=%u total=%d\n", unsigned(result), total);
            return;
        }
        At<int>(owner, field::NativeTotal) = total;
        purchase.active = true;
        Logging.Log("[QoL Enhancements/Fitting Room] COMMIT_BEGIN wear=%u total=%d\n", wear & 1, total);
        // Native code performs payment, ticket use, storage/pocket delivery,
        // clothing swap, result dialog routing, purchase records and milestones.
        Orig(owner, wear);
        purchase.active = false;
        purchase.done = true;
        session.bag.Clear(); session.dirty = false;
        Logging.Log("[QoL Enhancements/Fitting Room] COMMIT_RETURNED\n");
    }
};
} // namespace
bool Install() {
    struct Fingerprint { std::uintptr_t address; std::array<std::uint32_t, 4> words; };
    // Generated from the SHA256-checked local 3.0.3 mapped main; all sites must
    // pass before installing any hook. The page input body can already be hooked
    // by QoL Pockets; only our private vtable slot must still be original.
    const Fingerprint checks[]{
#include "fingerprints.inc"
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
    auto *inputSlot = reinterpret_cast<volatile std::uintptr_t *>(slot);
    const auto replacement = reinterpret_cast<std::uintptr_t>(&Input);
    *inputSlot = replacement;
    if (*inputSlot != replacement) {
        Logging.Log("[QoL Enhancements/Fitting Room] INPUT_HOOK_FAILED readback\n");
        return false;
    }
    Logging.Log("[QoL Enhancements/Fitting Room] INPUT_HOOK_OK writable-data readback\n");
    originalInput = Fn<InputFn>(address::OriginalInput);
    Begin::InstallAtOffset(address::Begin);
    Destroy::InstallAtOffset(address::Destroy);
    Focus::InstallAtOffset(address::Focus);
    Row::InstallAtOffset(address::Row);
    Price::InstallAtOffset(address::Price);
    Play::InstallAtOffset(address::Play);
    Checkout::InstallAtOffset(address::Checkout);
    Collect::InstallAtOffset(address::Collect);
    Dialog::InstallAtOffset(address::Dialog);
    GrossPrice::InstallAtOffset(address::GrossPrice);
    PurchaseRecord::InstallAtOffset(address::PurchaseRecord);
    return true;
}
} // namespace able_bag
