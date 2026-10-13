#include "program/core/hook_plan.hpp"
#include "catalog_variation_labels.hpp"
#include "program/catalog/game/bindings.hpp"
#include "program/catalog/game/catalog_ownership.hpp"
#include <array>
#include <cstring>

namespace catalog_aware_shops::features {
namespace {
namespace address {
constexpr std::uintptr_t NookUpdateColorLabel = 0x02b69630;
constexpr std::uintptr_t HhpUpdateColorLabel = 0x02f98930;
constexpr std::uintptr_t SetPaneText = 0x03c1b588, SetGroupText = 0x03c47b90;
} // namespace address
namespace layout {
constexpr std::size_t Player = 0x1f8, NookPage = 0x210, HhpPage = 0x220;
constexpr std::size_t NookColorInfo = 0x5d8, HhpColorInfo = 0x580;
constexpr std::size_t HhpDisplayedBody = 0x574f8;
constexpr std::size_t TextBindings = 0x220, BindingAdapter = 0x48;
constexpr std::size_t AdapterPayload = 8, AdapterMethods = 0x18, SetTextMethod = 0x5e0;
} // namespace layout

template <class T> T Read(std::uintptr_t object, std::size_t offset = 0) {
    T value;
    std::memcpy(&value, reinterpret_cast<const void *>(object + offset), sizeof(value));
    return value;
}

enum class Catalog : unsigned { Nook, Hhp };
constexpr std::array<const char *, 2> CatalogNames{"Nook", "HHP"};
constexpr std::size_t MaximumLabelUnits = 256;

struct LabelContext {
    std::uintptr_t payload{}, setter{};
    game::CatalogItem item{};
    std::uint8_t player{0xff};
    Catalog catalog{};
};

// These synchronous catalog UI update functions call the native formatter and
// typed text setters on the UI thread. Borrow the exact label binding only for
// that call. A nested update restores its parent's scope; no UI pointer or
// ownership result survives the update. Other labels using these setters do
// not match this payload and pass through unchanged.
LabelContext *activeLabel{};
class LabelScope {
    LabelContext *previous;
public:
    explicit LabelScope(LabelContext &context) : previous(activeLabel) { activeLabel = &context; }
    ~LabelScope() { activeLabel = previous; }
};

LabelContext CaptureLabel(std::uintptr_t controller, const game::CatalogItem *item, Catalog catalog) {
    LabelContext context{};
    context.catalog = catalog;
    if (!controller || !item) return context;
    context.item = *item;
    context.player = Read<std::uint8_t>(controller, layout::Player);
    if (catalog == Catalog::Hhp) {
        // 02f98d84-02f98d98 passes this selected body index to 01a003d0
        // when building the displayed color name. Match that label even if the
        // incoming preview item still contains a different body value.
        const auto body = Read<unsigned>(controller, layout::HhpDisplayedBody);
        if (body >= 8) return context;
        context.item.extra = (context.item.extra & ~7u) | body;
    }
    const auto page = Read<std::uintptr_t>(controller,
        catalog == Catalog::Nook ? layout::NookPage : layout::HhpPage);
    const auto info = page ? Read<std::uintptr_t>(page,
        catalog == Catalog::Nook ? layout::NookColorInfo : layout::HhpColorInfo) : 0;
    const auto bindings = info ? Read<std::uintptr_t>(info, layout::TextBindings) : 0;
    const auto binding = bindings ? Read<std::uintptr_t>(bindings) : 0;
    const auto adapter = binding ? Read<std::uintptr_t>(binding, layout::BindingAdapter) : 0;
    const auto methods = adapter ? Read<std::uintptr_t>(adapter, layout::AdapterMethods) : 0;
    if (!methods) return context;
    const auto setter = Read<std::uintptr_t>(methods, layout::SetTextMethod);
    if (setter != game::MainAddress(address::SetPaneText) &&
        setter != game::MainAddress(address::SetGroupText)) return context;
    context.payload = Read<std::uintptr_t>(adapter, layout::AdapterPayload);
    context.setter = setter;
    return context;
}

// Work on a temporary copy after native localization/formatting. Never edit
// MSBT resources, formatter buffers, or their cached lengths. The native setter
// copies the text synchronously, and native binding notification/sizing follows.
// Tagged text, unexpected lengths and oversized labels are left unchanged.
bool CopyMarkedLabel(char16_t *output, const char16_t *text, unsigned length) {
    if (!text || !length || length + 3ull > MaximumLabelUnits) return false;
    if (text[0] == u'☆' || text[0] == u'★') return false;
    for (unsigned i = 0; i < length; ++i) {
        if (text[i] < 0x20 || text[i] == 0x7f) return false;
    }
    if (text[length] != 0) return false;
    output[0] = u'☆';
    output[1] = u' ';
    std::memcpy(output + 2, text, (length + 1) * sizeof(char16_t));
    return true;
}

template <class Original>
void SetLabelText(Original original, std::uintptr_t setter, std::uintptr_t payload,
                  const char16_t *text, unsigned length) {
    const auto context = activeLabel;
    if (!context || !payload || payload != context->payload || setter != context->setter || !length) {
        original(payload, text, length);
        return;
    }
    const auto status = game::QueryFurnitureCatalog(context->player, context->item);
    const bool missing = status == game::CatalogStatus::Uncataloged ||
                         status == game::CatalogStatus::VariantUncataloged;
    char16_t marked[MaximumLabelUnits];
    const bool changed = missing && CopyMarkedLabel(marked, text, length);
    original(payload, changed ? marked : text, changed ? length + 2 : length);
    static std::array<unsigned, CatalogNames.size()> reports{};
    const auto index = static_cast<unsigned>(context->catalog);
    if (reports[index] < 48) {
        ++reports[index];
        Logging.Debug("[QoL Enhancements/Catalog] CATALOG_VARIATION shop=%s item=%u body=%u player=%u status=%u marked=%d\n",
                    CatalogNames[index], context->item.id, context->item.extra & 7u,
                    context->player, static_cast<unsigned>(status), changed);
    }
}

HOOK_DEFINE_TRAMPOLINE(NookVariationLabel) {
    static void Callback(std::uintptr_t controller, const game::CatalogItem *item) {
        auto context = CaptureLabel(controller, item, Catalog::Nook);
        LabelScope scope(context);
        Orig(controller, item);
    }
};
HOOK_DEFINE_TRAMPOLINE(HhpVariationLabel) {
    static void Callback(std::uintptr_t controller, const game::CatalogItem *item) {
        auto context = CaptureLabel(controller, item, Catalog::Hhp);
        LabelScope scope(context);
        Orig(controller, item);
    }
};
HOOK_DEFINE_TRAMPOLINE(VariationPaneText) {
    static void Callback(std::uintptr_t pane, const char16_t *text, unsigned length) {
        SetLabelText([](auto target, auto value, auto count) { Orig(target, value, count); },
                     game::MainAddress(address::SetPaneText), pane, text, length);
    }
};
HOOK_DEFINE_TRAMPOLINE(VariationGroupText) {
    static void Callback(std::uintptr_t group, const char16_t *text, unsigned length) {
        SetLabelText([](auto target, auto value, auto count) { Orig(target, value, count); },
                     game::MainAddress(address::SetGroupText), group, text, length);
    }
};
} // namespace

bool InstallCatalogVariationLabels() {
    struct Check { std::uintptr_t offset; std::array<std::uint32_t, 5> words; };
    constexpr Check checks[]{
        // Entry contracts and label-binding/body-index accesses in ACNH 3.0.3.
        {0x02b69630, {0xa9ba7bfd, 0xa9016ffc, 0x910003fd, 0xa90267fa, 0xa9035ff8}},
        {0x02f98930, {0xa9bb7bfd, 0xf9000bfc, 0x910003fd, 0xa9025ff8, 0xa90357f6}},
        {0x02b696a8, {0xf9411108, 0xf9400114, 0xb4002d94, 0xf9402696, 0xf94006c8}},
        {0x02f98a98, {0xf9411268, 0xf942c108, 0xb40019c8, 0xf9411108, 0xf9400116}},
        {0x02f98d84, {0x528e9f08, 0x72a000a8, 0x910063e0, 0xb8686a62, 0x9100c3e1}},
        {0x03c1b588, {0xa9be7bfd, 0xf9000bf3, 0x910003fd, 0xf9400008, 0xaa0003f3}},
        {0x03c47b90, {0xf9400408, 0xeb00011f, 0x54000380, 0xa9bc7bfd, 0xa9015ff8}},
    };
    for (const auto &check : checks) {
        if (std::memcmp(reinterpret_cast<const void *>(game::MainAddress(check.offset)),
                        check.words.data(), sizeof(check.words))) {
            Logging.Error("[QoL Enhancements/Catalog] CATALOG_VARIATIONS_DISABLED fingerprint mismatch\n");
            return false;
        }
    }
    if (!game::ValidateCatalogBindings()) return false;
    // Install after badges: their validation also inspects the unpatched typed
    // setter prologues. No existing hook targets these four addresses.

    qol::core::HookPlan hookPlan{
        qol::core::Site<NookVariationLabel>("NookVariationLabel", address::NookUpdateColorLabel, {0xa9ba7bfd, 0xa9016ffc, 0x910003fd, 0xa90267fa, 0xa9035ff8}, true),
        qol::core::Site<HhpVariationLabel>("HhpVariationLabel", address::HhpUpdateColorLabel, {0xa9bb7bfd, 0xf9000bfc, 0x910003fd, 0xa9025ff8, 0xa90357f6}, true),
        qol::core::Site<VariationPaneText>("VariationPaneText", address::SetPaneText, {0xa9be7bfd, 0xf9000bf3, 0x910003fd, 0xf9400008, 0xaa0003f3}, true),
        qol::core::Site<VariationGroupText>("VariationGroupText", address::SetGroupText, {0xf9400408, 0xeb00011f, 0x54000380, 0xa9bc7bfd, 0xa9015ff8}, true),
    };
    if (!hookPlan) return false;
    if (!hookPlan.Publish<NookVariationLabel>(address::NookUpdateColorLabel)) return false;
    if (!hookPlan.Publish<HhpVariationLabel>(address::HhpUpdateColorLabel)) return false;
    if (!hookPlan.Publish<VariationPaneText>(address::SetPaneText)) return false;
    if (!hookPlan.Publish<VariationGroupText>(address::SetGroupText)) return false;
    Logging.Debug("[QoL Enhancements/Catalog] CATALOG_VARIATIONS_READY Nook/HHP hollow color-label stars\n");
    return true;
}
} // namespace catalog_aware_shops::features
