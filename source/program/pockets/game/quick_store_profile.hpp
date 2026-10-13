#pragma once
#include "program/core/native/executable_view.hpp"
#include "program/game/v303/inventory.hpp"
namespace qol_pockets::game::v303::quick_store {
// Module-relative addresses and live object offsets are separate namespaces.
namespace address {
constexpr std::uintptr_t PocketControllerVtable = 0x04a06e28;
constexpr std::uintptr_t MenuBuilderVtable = 0x04a071c8;
constexpr std::uintptr_t MenuDescriptionVtable = 0x049c7208;
constexpr std::uintptr_t EmptyCallbackVtable = 0x049c7230;
constexpr std::uintptr_t PocketPageVtable = 0x04a06b00;
constexpr std::uintptr_t ApplicationGlobal = 0x059f83f0;
constexpr std::uintptr_t Play = 0x0308aa80, Command = 0x0308c5f8;
constexpr std::uintptr_t EnterMenu = 0x0308c4c4, UpdateMenu = 0x0308c590;
constexpr std::uintptr_t BuildMenu = 0x03088df0, OwnHome = 0x02815598;
constexpr std::uintptr_t ButtonFeedback = 0x03c66950;
constexpr std::uintptr_t ItemFeedbackValidity = 0x02fbfe5c;
constexpr std::uintptr_t UiApplicationGlobalGot = 0x04ab2230;
constexpr std::uintptr_t CursorVtable = 0x04a0ea80;
constexpr std::uintptr_t AnimationOperationVtableGot = 0x04ac4e68;
}
namespace layout {
constexpr std::size_t PocketPage = 0x210;
constexpr std::size_t FeedbackValidityParameter = 0x2a0;
constexpr std::size_t FeedbackParameterValue = 0x40;
constexpr std::size_t FeedbackRowParameter = 0x2a8;
constexpr std::size_t FeedbackContext = 0x298;
constexpr std::size_t SnapshotRowCount = 8;
constexpr std::size_t SnapshotRows = 0x10;
constexpr std::size_t TransitionA = 0x1f0;
constexpr std::size_t TransitionB = 0x1f4;
constexpr std::size_t TransitionSlot = 0x200;
constexpr std::size_t SelectedSlot = 0x1c514;
constexpr std::size_t PocketMode = 0x1c4fc;
constexpr std::size_t CurrentState = 0x1b0;
constexpr std::size_t PocketError = 0x1c50c;
constexpr std::size_t BlockedSlots = 0x1c4f0;
constexpr std::size_t PocketMenu = 0x218;
constexpr std::size_t MenuBuilderVtable = 0x218;
constexpr std::size_t MenuBuilderOwner = 0x220;
constexpr std::size_t MenuBuilderFunction = 0x228;
constexpr std::size_t MenuBuilderAdjustment = 0x230;
constexpr std::size_t BindingColumnCount = 0x468;
constexpr std::size_t InputBindings = 0x470;
constexpr std::size_t BindingRowCount = 0x46c;
constexpr std::size_t BindingFunction = 0x10;
constexpr std::size_t BindingAdjustment = 0x18;
constexpr std::size_t InputContext = 0x210;
constexpr std::size_t FocusState = 0x48;
constexpr std::size_t ItemPane = 0x218;
constexpr std::size_t HintButtonId = 0x14;
constexpr std::size_t HintInputRow = 0x10;
constexpr std::size_t EquippedSlot = 0x1c516;
constexpr std::size_t SelectedSlots = 0x1c4e8;
constexpr std::size_t TransientHoverSlot = 0x1c518;
constexpr std::size_t LogicalInput = 0x1c51a;
constexpr std::size_t BackButtonHint = 0x570;
constexpr std::size_t HintInputUnit = 0xa0;
constexpr std::size_t GlobalInputMasks = 0x418;
constexpr std::size_t ConsumedButtons = 0x3c;
constexpr std::size_t InputBlocked = 0x54;
constexpr std::size_t GlobalInputMask = 0x10;
constexpr std::size_t InputMask = 0x58;
constexpr std::size_t MenuRowCount = 0x26d8;
constexpr std::size_t MenuFirstAction = 0x3a0;
constexpr std::size_t MenuSelection = 0x26c9;
constexpr std::size_t ItemWidgets = 0x590;
constexpr std::size_t InputRowBytes = 0x48;
constexpr std::size_t PocketInventory = 0x1f8;
}
namespace cursor_layout {
constexpr std::size_t ApplicationCursor = 0xd0;
constexpr std::size_t Page = 0x210, FocusedPane = 0x218, OverrideLayout = 0x220;
constexpr std::size_t HideRequested = 0x26c, PageDefaultLayout = 0x570;
constexpr std::size_t ClickAnimation = 0x208;
constexpr std::size_t AnimationTarget = 8, AnimationImplementation = 0x18;
constexpr std::size_t PrepareOperationVirtual = 0x18, PlayVirtual = 0x190;
}
struct MenuRow {
    const char *bank{};
    const char *key{};
    int action{};
    unsigned flags{}, extra{}, padding{};
};
struct MenuCallback {
    std::uintptr_t vtable{}, fields[3]{};
};
struct MenuDescription {
    std::uintptr_t vtable{}, bankVtable{};
    const char *bank{};
    std::byte reserved[0x28]{};
    int error{}, count{};
    MenuRow rows[7]{};
    MenuCallback callbacks[7]{};
};
static_assert(sizeof(MenuDescription) == 0x208 && offsetof(MenuDescription, rows) == 0x48);
struct SafeString { std::uintptr_t vtable; const char *text; };
inline constexpr qol::native::FunctionBinding<int (*)(void *)> FeedbackValidityBinding{
    "pockets.ItemFeedbackValidity", address::ItemFeedbackValidity};
inline constexpr qol::native::FunctionBinding<void (*)(void *, const SafeString *, void *)> FeedbackBinding{
    "pockets.ButtonFeedback", address::ButtonFeedback};
inline constexpr qol::native::FunctionBinding<void (*)(void *, MenuDescription *)> MenuBinding{
    "pockets.BuildMenu", address::BuildMenu};
inline constexpr qol::native::FunctionBinding<unsigned (*)(const unsigned char *)> HomeBinding{
    "pockets.OwnHome", address::OwnHome};
inline constexpr qol::native::Fingerprint<> Checks[]{
        {address::Play, {0xa9ba7bfd, 0xa9016ffc, 0x910003fd, 0xa90267fa, 0xa9035ff8}, "pockets.quick_store.Play"},
        {address::Command, {0xa9bd7bfd, 0xf9000bf5, 0xa9024ff4, 0x910003fd, 0x91407008}, "pockets.quick_store.Command"},
        {address::EnterMenu, {0xd10103ff, 0xa9027bfd, 0xa9034ff4, 0x910083fd, 0xf9410808}, "pockets.quick_store.EnterMenu"},
        {address::UpdateMenu, {0xf9410c08, 0xb941b108, 0x7100051f, 0x540000c1, 0x39480008}, "pockets.quick_store.UpdateMenu"},
        {address::BuildMenu, {0xd10383ff, 0xa9087bfd, 0xa9096ffc, 0xa90a67fa, 0xa90b5ff8}, "pockets.quick_store.BuildMenu"},
        {address::OwnHome, {0xf0011988, 0x798fa908, 0xb100051f, 0x540007a0, 0xd100c3ff}, "pockets.quick_store.OwnHome"},
        {0x03076d78, {0xd10103ff, 0xa9017bfd, 0xa90257f6, 0xa9034ff4, 0x910043fd}, "pockets.quick_store.Code_03076d78"},
        {address::ButtonFeedback, {0xd102c3ff, 0xa9077bfd, 0xf90043f7, 0xa90957f6, 0xa90a4ff4}, "pockets.quick_store.ButtonFeedback"},
        {address::ItemFeedbackValidity, {0x394b0c09, 0x7200013f, 0x394c5408, 0x7a401900, 0x1a9f07e0}, "pockets.quick_store.ItemFeedbackValidity"},
        {0x031007dc, {0xf9411268, 0xb5000088, 0xf9410a68, 0xf942b908, 0xb4000268}, "pockets.quick_store.Code_031007dc"},
        {0x0310080c, {0xf90013e8, 0x52800028, 0xb9002be8, 0xf9400288, 0xf9400d08}, "pockets.quick_store.Code_0310080c"},
        {0x03100824, {0xf9400e88, 0x1e2e1000, 0xf9400680, 0xf940c908, 0xd63f0100}, "pockets.quick_store.Code_03100824"},
        qol::game::v303::inventory::CapacityCode,
        qol::game::v303::inventory::ItemAtCode,
};
constexpr std::array<std::uint32_t, 5> HookWords(std::uintptr_t offset) {
    for (const auto &check : Checks) if (check.offset == offset) return check.words;
    return {}; // Unknown sites fail compatibility checks; never infer code.
}
} // namespace qol_pockets::game::v303::quick_store
