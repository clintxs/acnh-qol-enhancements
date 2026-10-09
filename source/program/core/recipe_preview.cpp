#include "lib.hpp"
#include "recipe_preview.hpp"
#include <array>
#include <cstring>

namespace qol::core {
namespace {
namespace address {
constexpr std::uintptr_t PhoneBind = 0x02e9cde0;
constexpr std::uintptr_t PhoneFrame = 0x02e9fbb0;
constexpr std::uintptr_t PhoneCleanup = 0x02e9d538;
constexpr std::uintptr_t PhoneConfirm = 0x02e99d90;
constexpr std::uintptr_t PhoneRender = 0x02e96b80;
constexpr std::uintptr_t GridRefresh = 0x0335b620;
constexpr std::uintptr_t PreviewConstructor = 0x03492b50;
constexpr std::uintptr_t PreviewBind = 0x034930b0;
constexpr std::uintptr_t PreviewFrame = 0x034940e0;
constexpr std::uintptr_t PreviewInput = 0x03492fec;
constexpr std::uintptr_t PreviewVtable = 0x04a3b778;
constexpr std::uintptr_t InitializePage = 0x0376890c;
constexpr std::uintptr_t ActivateChild = 0x03769260;
constexpr std::uintptr_t RemoveChild = 0x03768c7c;
constexpr std::uintptr_t Allocate = 0x002478c0;
constexpr std::uintptr_t GetTls = 0x03f033a0;
constexpr std::uintptr_t Lock = 0x03f03880;
constexpr std::uintptr_t Unlock = 0x03f038c0;
constexpr std::uintptr_t LayoutVisible = 0x02af0d30;
constexpr std::uintptr_t StringVtable = 0x04ab2020;
constexpr std::uintptr_t PageParamsVtable = 0x04ac39b0;
constexpr std::uintptr_t ThreadManager = 0x04ab3350;
constexpr std::uintptr_t Game = 0x04ab2718;
constexpr std::uintptr_t PreviewName = 0x03f6ecaf; // WorkCraftInfoSeq (page name).
}
namespace field {
constexpr std::size_t Parent = 0x48, PendingChildren = 0x50, PendingCount = 0x60;
constexpr std::size_t PendingLinkOffset = 0x64, Manager = 0x98, Lifecycle = 0xa8;
constexpr std::size_t Mutex = 0x188, State = 0x1b0, Requests = 0x1f0;
constexpr std::size_t Resident = 0x1f8, RequestSlot = 0x200;
constexpr std::size_t PhoneLayout = 0x210, PhoneSelection = 0x218, PhoneRecipes = 0x8a350;
constexpr std::size_t PreviewCount = 0x260, PreviewSelection = 0x264;
constexpr std::size_t PreviewAction = 0x1c1cc, PreviewLayout = 0x1c1f8;
constexpr std::size_t PreviewRecipes = 0x1c200;
constexpr std::size_t Input = 0x210, InputSuspended = 0x54, Grid = 0x580;
constexpr std::size_t CraftButton = 0x598;
constexpr std::size_t LayoutVisibilityController = 0x1b0;
}
constexpr std::size_t PreviewSize = 0x1c210, PreviewVtableSize = 0x2c0;
std::uintptr_t Main{};

template<class T> T &At(void *object, std::size_t offset) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(object) + offset);
}
template<class Function> Function Native(std::uintptr_t offset) {
    return reinterpret_cast<Function>(Main + offset);
}
template<class Result = void, class... Args>
Result Virtual(void *object, std::size_t slot, Args... args) {
    const auto table = At<std::uintptr_t *>(object, 0);
    return reinterpret_cast<Result (*)(void *, Args...)>(table[slot / 8])(object, args...);
}
void *Global(std::uintptr_t offset) { return *Native<void **>(offset); }

// The phone keeps its list alive and unchanged while the preview borrows it.
// Each resident gets a separate session; no native workbench object is modified.
enum class Phase { Idle, Loading, Appearing, Selecting, Showing, Closing, Restoring };
struct Session {
    void *phone{};
    void *preview{};
    Phase phase{};
    bool closeRequested{};
};
std::array<Session, 8> Sessions{};
// Include the two ABI header words so native RTTI/header access stays valid.
std::array<std::uintptr_t, PreviewVtableSize / 8 + 2> PreviewTable{};

Session *PhoneSession(void *phone) {
    const auto resident = At<unsigned char>(phone, field::Resident);
    if (resident >= Sessions.size()) return nullptr;
    auto &session = Sessions[resident];
    return session.phone == phone ? &session : nullptr;
}
Session *PreviewSession(void *preview) {
    for (auto &session : Sessions) if (session.preview == preview) return &session;
    return nullptr;
}
bool Settled(void *page, unsigned state) {
    return At<unsigned>(page, field::State) == state &&
           At<int>(page, field::Requests) == -1 && At<int>(page, field::Requests + 4) == -1;
}
bool Request(void *page, int state) {
    const auto slot = At<unsigned char>(page, field::RequestSlot);
    if (slot > 1) return false;
    auto &request = At<int>(page, field::Requests + slot * 4);
    if (request != -1) return false;
    request = state;
    return true;
}
void SuspendPhone(Session &session, bool suspend) {
    auto layout = At<void *>(session.phone, field::PhoneLayout);
    if (!layout) return;
    if (auto input = At<void *>(layout, field::Input))
        At<unsigned char>(input, field::InputSuspended) = suspend;
    // This native method changes the visibility of layout roots, preserving
    // the category, selected cell, scroll position and individual pane states.
    Native<void (*)(void *, bool)>(address::LayoutVisible)(layout, !suspend);
}
void HideCraftButton(void *preview) {
    auto layout = At<void *>(preview, field::PreviewLayout);
    if (!layout) return;
    auto button = At<void *>(layout, field::CraftButton);
    if (!button) return;
    Virtual(button, 0x140, false, false); // Disable input/selection animation.
    // The button is a nested layout root. Hide that root through the same
    // controller used by LayoutVisible, rather than merely greying it out.
    if (auto visibility = At<void *>(button, field::LayoutVisibilityController))
        Virtual(visibility, 0x20, false);
}
void *NoCraftButton(void *) { return nullptr; }
bool CannotCraft(void *, int) { return false; }

bool PreviewInput(void *preview, void *event) {
    // Native command 0 is B; 3 is favorite. The remaining native callbacks
    // retain shoulder navigation and model rotation. Never close the phone.
    if (At<unsigned>(event, 0x10) == 0) {
        if (auto session = PreviewSession(preview)) session->closeRequested = true;
        return true;
    }
    return Native<bool (*)(void *, void *)>(address::PreviewInput)(preview, event);
}
void ReplaceCallback(void *layout, std::size_t rows, std::uintptr_t original,
                     std::uintptr_t replacement) {
    const auto columns = At<unsigned>(layout, rows);
    const auto count = At<unsigned>(layout, rows + 4);
    auto entries = At<std::byte *>(layout, rows + 8);
    if (!entries || columns > 64 || count > 32) return;
    for (unsigned i = 0; i < columns * count; ++i) {
        auto &callback = At<std::uintptr_t>(entries + i * 0x20, 0x10);
        if (callback == Main + original) callback = replacement;
    }
}
void BindPreview(void *preview) {
    Native<void (*)(void *)>(address::PreviewBind)(preview);
    auto layout = At<void *>(preview, field::PreviewLayout);
    if (layout) ReplaceCallback(layout, 0x468, address::PreviewInput,
                                reinterpret_cast<std::uintptr_t>(&PreviewInput));
    HideCraftButton(preview);
}
void UpdatePreview(void *preview) {
    Native<void (*)(void *)>(address::PreviewFrame)(preview);
    // Native recipe changes can refresh button visibility. Hide it after
    // every update as well as removing the focus target and craftability gate.
    HideCraftButton(preview);
    if (At<int>(preview, field::PreviewAction) == 2)
        At<int>(preview, field::PreviewAction) = -1;
}

// Native page construction allocates from its UI manager's heap, including
// nested model/layout resources. Match its TLS heap scope and restore it.
class UiHeapScope {
    void *tls{};
    void *previous{};
public:
    explicit UiHeapScope(void *manager) {
        auto heap = At<void *>(manager, 0xb8);
        if (!heap) return;
        auto managerSlot = Global(address::ThreadManager);
        auto threadManager = managerSlot ? At<void *>(managerSlot, 0) : nullptr;
        if (!threadManager) return;
        tls = Native<void *(*)(unsigned)>(address::GetTls)(At<unsigned>(threadManager, 0x68));
        if (tls) {
            previous = At<void *>(tls, 0xa8);
            At<void *>(tls, 0xa8) = heap;
        }
    }
    ~UiHeapScope() { if (tls) At<void *>(tls, 0xa8) = previous; }
};
struct StringRef { void *vtable; const char *text; };
struct PageParams {
    void *vtable;
    unsigned resource;
    unsigned short flags;
    unsigned char resident;
    unsigned char padding;
    int argument;
};
static_assert(offsetof(PageParams, resident) == 0xe);
static_assert(offsetof(PageParams, argument) == 0x10);

bool CreatePreview(Session &session) {
    auto manager = At<void *>(session.phone, field::Manager);
    if (!manager) return false;
    UiHeapScope heap(manager);
    auto preview = Native<void *(*)(std::size_t)>(address::Allocate)(PreviewSize);
    if (!preview) return false;
    const StringRef name{static_cast<std::byte *>(Global(address::StringVtable)) + 0x10,
                         Native<const char *>(address::PreviewName)};
    auto game = At<void *>(Global(address::Game), 0);
    const PageParams params{static_cast<std::byte *>(Global(address::PageParamsVtable)) + 0x10,
                            At<unsigned>(At<void *>(game, 0x418), 0xc), 1,
                            At<unsigned char>(session.phone, field::Resident), 0, -1};
    Native<void (*)(void *, const StringRef *, const PageParams *)>(address::PreviewConstructor)(preview, &name, &params);
    At<void *>(preview, 0) = PreviewTable.data() + 2;
    At<void *>(preview, field::PreviewLayout) = nullptr;
    At<void *>(preview, field::PreviewRecipes) = nullptr;
    At<unsigned short>(preview, field::PreviewRecipes + 8) = 0;
    session.preview = preview;

    // Register as a real child, using the intrusive-list offset initialized
    // by the native parent constructor. The parent's teardown owns all memory.
    auto phone = session.phone;
    auto mutex = static_cast<std::byte *>(phone) + field::Mutex;
    Native<void (*)(void *)>(address::Lock)(mutex);
    auto head = static_cast<std::byte *>(phone) + field::PendingChildren;
    auto node = static_cast<std::byte *>(preview) + At<int>(phone, field::PendingLinkOffset);
    auto first = At<void *>(head, 0);
    At<void *>(head, 0) = node;
    At<void *>(node, 0) = first;
    At<void *>(node, 8) = head;
    if (first) At<void *>(first, 8) = node;
    ++At<unsigned>(phone, field::PendingCount);
    At<void *>(preview, field::Parent) = phone;
    Native<void (*)(void *)>(address::Unlock)(mutex);
    At<void *>(preview, field::Manager) = manager;
    At<unsigned>(preview, field::Lifecycle) = 1;
    Native<void (*)(void *, const PageParams *)>(address::InitializePage)(preview, &params);
    At<unsigned>(preview, field::Lifecycle) = 2;
    Virtual(preview, 0x48);
    Native<void (*)(void *, void *)>(address::ActivateChild)(phone, preview);
    return true;
}

void ConfirmRecipe(void *phone, void *) {
    auto session = PhoneSession(phone);
    if (!session || session->phase != Phase::Idle || !Settled(phone, 3)) return;
    const auto count = At<unsigned>(phone, field::PhoneRecipes);
    const auto selected = At<unsigned>(phone, field::PhoneSelection);
    auto recipes = At<void **>(phone, field::PhoneRecipes + 8);
    if (!count || selected >= count || !recipes || !recipes[selected] ||
        At<short>(recipes[selected], 8) == -2) return;
    session->closeRequested = false;
    session->phase = Phase::Loading;
    SuspendPhone(*session, true);
    if (!session->preview && !CreatePreview(*session)) {
        session->phase = Phase::Idle;
        SuspendPhone(*session, false);
        Logging.Log("[QoL Enhancements/Core] DIY preview: could not create native page\n");
    }
}

HOOK_DEFINE_TRAMPOLINE(BindPhoneRecipes) {
    static void Callback(void *phone) {
        Orig(phone);
        const auto resident = At<unsigned char>(phone, field::Resident);
        if (resident >= Sessions.size()) return;
        auto &session = Sessions[resident];
        if (session.phone && session.phone != phone) return;
        session.phone = phone;
        auto layout = At<void *>(phone, field::PhoneLayout);
        if (layout) ReplaceCallback(layout, 0x430, address::PhoneConfirm,
                                    reinterpret_cast<std::uintptr_t>(&ConfirmRecipe));
    }
};
HOOK_DEFINE_TRAMPOLINE(UpdatePhoneRecipes) {
    static void Callback(void *phone) {
        Orig(phone);
        auto session = PhoneSession(phone);
        if (!session || session->phase == Phase::Idle || !session->preview) return;
        auto preview = session->preview;
        auto layout = At<void *>(preview, field::PreviewLayout);
        switch (session->phase) {
        case Phase::Loading:
            if (Settled(preview, 1) && layout) {
                At<void *>(preview, field::PreviewRecipes) = static_cast<std::byte *>(phone) + field::PhoneRecipes;
                Virtual(preview, 0xa8);
                session->phase = Phase::Appearing;
            }
            break;
        case Phase::Appearing:
            if (Settled(preview, 3)) {
                At<unsigned>(preview, field::PreviewSelection) = At<unsigned>(phone, field::PhoneSelection);
                if (Request(preview, 4)) session->phase = Phase::Selecting;
            }
            break;
        case Phase::Selecting:
            if (Settled(preview, 3)) session->phase = Phase::Showing;
            break;
        case Phase::Showing:
            if (session->closeRequested && Settled(preview, 3) && Request(preview, 5))
                session->phase = Phase::Closing;
            break;
        case Phase::Closing:
            if (Settled(preview, 5) && layout && Virtual<bool>(layout, 0x140, 0) && Request(preview, 1))
                session->phase = Phase::Restoring;
            break;
        case Phase::Restoring:
            if (Settled(preview, 1)) {
                session->phase = Phase::Idle;
                SuspendPhone(*session, false);
                Native<void (*)(void *, unsigned)>(address::PhoneRender)(phone, At<unsigned>(phone, field::PhoneSelection));
                auto phoneLayout = At<void *>(phone, field::PhoneLayout);
                if (auto grid = At<void *>(phoneLayout, field::Grid))
                    Native<void (*)(void *, unsigned)>(address::GridRefresh)(grid, 2);
            }
            break;
        case Phase::Idle: break;
        }
    }
};
HOOK_DEFINE_TRAMPOLINE(CleanupPhoneRecipes) {
    static void Callback(void *phone) {
        if (auto session = PhoneSession(phone)) {
            if (session->preview) {
                const auto lifecycle = At<unsigned>(session->preview, field::Lifecycle);
                if (lifecycle == 3 || lifecycle == 4 || lifecycle == 6)
                    Native<void (*)(void *, void *)>(address::RemoveChild)(phone, session->preview);
            }
            *session = {};
        }
        Orig(phone);
    }
};
} // namespace

bool InstallRecipePreview() {
    struct Fingerprint { std::uintptr_t offset; std::uint32_t words[5]; };
    constexpr Fingerprint fingerprints[]{
        // Filled from unmodified ACNH 3.0.3; behavior lives in C++ above.
        {0x02e9cde0, {0xd10243ff, 0xa9037bfd, 0xa9046ffc, 0xa90567fa, 0xa9065ff8}},
        {0x02e9fbb0, {0xa9bb7bfd, 0xa90167fa, 0xa9025ff8, 0xa90357f6, 0xa9044ff4}},
        {0x02e9d538, {0xa9bd7bfd, 0xf9000bf5, 0xa9024ff4, 0x910003fd, 0xf9410814}},
        {0x02e9cf2c, {0xb94003e8, 0xd503201f, 0x10fe72eb, 0xb9443289, 0xb944368a}},
        {0x02e9cfe8, {0xb94003e8, 0xd503201f, 0x10fe6d2b, 0xb9443289, 0xb944368a}},
        {0x03492b50, {0xa9ba7bfd, 0xf9000bfb, 0xa90267fa, 0xa9035ff8, 0xa90457f6}},
        {0x034930b0, {0xd101c3ff, 0xa9037bfd, 0xa9045ff8, 0xa90557f6, 0xa9064ff4}},
        {0x03493430, {0x10ffdde9, 0x93407d08, 0x9a9f8108, 0x8b081548, 0xa9017d09}},
        {0x034940e0, {0xa9bb7bfd, 0xa90167fa, 0xa9025ff8, 0xa90357f6, 0xa9044ff4}},
        {0x03492fec, {0xb9401029, 0x91407008, 0x91072108, 0x71000d3f, 0x540000e0}},
        {0x03494d8c, {0xf9400268, 0xaa1303e0, 0xb9426661, 0xf9414508, 0xd63f0100}},
        {0x034947d0, {0xa9bc7bfd, 0xf9000bf7, 0xa90257f6, 0xa9034ff4, 0x910003fd}},
        {0x03495408, {0xb941b808, 0x71000d1f, 0x54000941, 0xd10103ff, 0xa9017bfd}},
        {0x0346fdb0, {0xfc190fe8, 0xa9017bfd, 0x910043fd, 0xa9026ffc, 0xa90367fa}},
        {0x03473ac8, {0x52984008, 0x72a00028, 0xf8686808, 0xf9400508, 0xf861d908}},
        {0x0376890c, {0xd10283ff, 0xa9067bfd, 0xf9003bf7, 0xa90857f6, 0xa9094ff4}},
        {0x03769260, {0xd10103ff, 0xa9017bfd, 0xa90257f6, 0xa9034ff4, 0x910043fd}},
        {0x03768c7c, {0xb940a828, 0x321f0108, 0x71001d1f, 0x54000041, 0xd65f03c0}},
        {0x02af0d30, {0xa9bc7bfd, 0xa9015ff8, 0xa90257f6, 0xa9034ff4, 0x910003fd}},
    };
    const auto &main = exl::util::GetMainModuleInfo();
    for (const auto &expected : fingerprints) {
        if (expected.offset + sizeof(expected.words) > main.m_Text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + expected.offset), expected.words, sizeof(expected.words))) {
            Logging.Log("[QoL Enhancements/Core] DIY preview: DISABLED; instruction mismatch at main+0x%lx\n", expected.offset);
            return false;
        }
    }
    Main = main.m_Text.m_Start;
    std::memcpy(PreviewTable.data(), Native<const void *>(address::PreviewVtable - 16), sizeof(PreviewTable));
    const auto replace = [](std::size_t slot, auto callback) {
        PreviewTable[2 + slot / 8] = reinterpret_cast<std::uintptr_t>(callback);
    };
    replace(0xd0, &BindPreview);
    replace(0x1a0, &UpdatePreview);
    replace(0x220, &NoCraftButton);
    replace(0x288, &CannotCraft);
    BindPhoneRecipes::InstallAtOffset(address::PhoneBind);
    UpdatePhoneRecipes::InstallAtOffset(address::PhoneFrame);
    CleanupPhoneRecipes::InstallAtOffset(address::PhoneCleanup);
    Logging.Log("[QoL Enhancements/Core] DIY preview: READY; native view-only page; app list navigation\n");
    return true;
}
} // namespace qol::core
