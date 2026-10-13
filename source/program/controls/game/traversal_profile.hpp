#pragma once
#include "program/core/native/executable_view.hpp"
#include "program/game/v303/inventory.hpp"
namespace qol::controls::game::v303::traversal {
namespace address {
constexpr std::uintptr_t ActionEntryFinished = 0x02277260;
constexpr std::uintptr_t ToolChangeEntry = 0x023758a0, ToolChangeUpdate = 0x02375ed0;
constexpr std::uintptr_t ToolChangeAnimationBranch = 0x02376018;
constexpr std::uintptr_t StableActions = 0x02222950, RequestAction = 0x02272704;
constexpr std::uintptr_t ContactNormals = 0x015f3040;
constexpr std::uintptr_t HopGeometry = 0x0230c470;
constexpr std::uintptr_t HopEntry = 0x0230b8d0, HopDirection = 0x0230bf18;
constexpr std::uintptr_t TryLadder = 0x02396154, LadderGeometry = 0x0239630c;
constexpr std::uintptr_t TryPlacedLadder = 0x022d9bd8;
constexpr std::uintptr_t PlacedObjectKind = 0x0113db50, PlacedObjectCoordinates = 0x015e49e4;
constexpr std::uintptr_t ValidLadderLanding = 0x02396bfc;
constexpr std::uintptr_t TryVault = 0x0239a340, PrepareVault = 0x0239aff4;
constexpr std::uintptr_t EquippedToolKind = 0x015f5c40, PlayerParameters = 0x0241e98c;
constexpr std::uintptr_t VaultPositionSelection = 0x0239a398;
constexpr std::uintptr_t VaultCandidateRejected = 0x0239a6c0, VaultSearchExit = 0x0239a5cc;
constexpr std::uintptr_t MovementGuards = 0x02219fc0;
constexpr std::uintptr_t MovementProbe = 0x0221a094, HopCommit = 0x02223454;
}
namespace field {
constexpr std::size_t Actor = 0x68, PlayerContext = 0x60, ActionController = 0x70;
constexpr std::size_t Position = 0x278, AlternatePosition = 0x284, Facing = 0x2b4;
constexpr std::size_t Action = 0x280, MovementState = 0x1810, InputValid = 0x12f8;
constexpr std::size_t StickX = 0xeec, StickY = 0xef0, Speed = 0x2e0;
constexpr std::size_t HopTimer = 0x6a0, JudgeTimerParameter = 0xed0;
constexpr std::size_t CurrentActionData = 0x18e0;
constexpr std::size_t Resident = 0x1ee8, PlayerHandle = 0x230;
constexpr std::size_t ParameterValueMethod = 0xf0;
}
// Feature bits retain independent pole/ladder activation; stage 2 owns hook composition.
struct Fingerprint {
    std::uintptr_t offset;
    std::array<std::uint32_t, 5> words;
    unsigned features;
    const char *name;
};
inline constexpr Fingerprint Checks[]{
#if CAS_AUTO_VAULT_DIAGNOSTIC
        {address::VaultCandidateRejected, {0x1100077b,0x71000f7f,0x54fffd40,0xf9403688,0xb4000428}, 1, "controls.traversal.VaultCandidateRejected"},
        {address::VaultSearchExit, {0x6d4a23e9,0x6d492beb,0x6d4833ed,0x6d473bef,0xa9504ff4}, 1, "controls.traversal.VaultSearchExit"},
        {address::MovementGuards, {0xb00144e8,0xf9414d08,0x79800108,0xb100051f,0x54000740}, 3, "controls.traversal.MovementGuards"},
#endif
        {address::ContactNormals, {0xd2e7f009,0x52a7f008,0xf900003f,0xa900a428,0xf9405008}, 2, "controls.traversal.ContactNormals"},
        {address::TryLadder, {0xb9417808,0x34000068,0x120003e0,0xd65f03c0,0xd101c3ff}, 2, "controls.traversal.TryLadder"},
        {address::LadderGeometry, {0xfc170fee,0x6d0133ed,0x6d022beb,0x6d0323e9,0xa9047bfd}, 2, "controls.traversal.LadderGeometry"},
        {address::ValidLadderLanding, {0x36000075,0x52800028,0x17fffff2,0x528e1488,0x72a7efa8}, 2, "controls.traversal.ValidLadderLanding"},
        {address::HopGeometry, {0xd10783ff,0x6d143bef,0x6d1533ed,0x6d162beb,0x6d1723e9}, 3, "controls.traversal.HopGeometry"},
        {address::HopEntry, {0xd10243ff,0x6d0523e9,0xa9067bfd,0xa90757f6,0xa9084ff4}, 3, "controls.traversal.HopEntry"},
        {address::HopDirection, {0xd10643ff,0xa9137bfd,0xa9146ffc,0xa91567fa,0xa9165ff8}, 3, "controls.traversal.HopDirection"},
        {address::VaultPositionSelection, {0x7200027f,0xbd400008,0x52804f0a,0x5280508b,0x9a8a1178}, 1, "controls.traversal.VaultPositionSelection"},
        {address::TryVault, {0xd10443ff,0x6d073bef,0x6d0833ed,0x6d092beb,0x6d0a23e9}, 1, "controls.traversal.TryVault"},
        {address::PrepareVault, {0x36000062,0x52800020,0xd65f03c0,0xd10343ff,0xfd0033e8}, 1, "controls.traversal.PrepareVault"},
        {address::EquippedToolKind, {0xd10183ff,0xa9037bfd,0xa90457f6,0xa9054ff4,0x9100c3fd}, 3, "controls.traversal.EquippedToolKind"},
        {address::PlayerParameters, {0xd100c3ff,0xa9017bfd,0xf90013f3,0x910043fd,0x7103ac1f}, 3, "controls.traversal.PlayerParameters"},
        {address::MovementProbe, {0xaa1303e0,0x52800021,0x52800022,0x9403c60c,0xb90003e0}, 3, "controls.traversal.MovementProbe"},
        {address::HopCommit, {0xaa1303e0,0x2a1f03e1,0x52800022,0x52800035,0x9403a11b}, 3, "controls.traversal.HopCommit"},
};
inline constexpr Fingerprint PlacedChecks[]{
        {address::TryPlacedLadder, {0xb9417808,0x34000068,0x120003e0,0xd65f03c0,0xd10483ff}, 3, "controls.traversal.TryPlacedLadder"},
        {address::PlacedObjectKind, {0xa9be7bfd,0xf9000bf3,0x910003fd,0xb4000481,0xb9400028}, 3, "controls.traversal.PlacedObjectKind"},
        {address::PlacedObjectCoordinates, {0xd10143ff,0xa9027bfd,0xa90357f6,0xa9044ff4,0x910083fd}, 3, "controls.traversal.PlacedObjectCoordinates"},
};
inline constexpr Fingerprint SwapChecks[]{
        {address::ActionEntryFinished, {0xd10203ff,0xa9037bfd,0xa90467fa,0xa9055ff8,0xa90657f6}, 3, "controls.traversal.ActionEntryFinished"},
        {address::ToolChangeEntry, {0xd101c3ff,0xa9037bfd,0xa9045ff8,0xa90557f6,0xa9064ff4}, 3, "controls.traversal.ToolChangeEntry"},
        {address::ToolChangeUpdate, {0xd10683ff,0xfd00abe8,0xf900b7fc,0xa915fbfd,0xa9175ff8}, 3, "controls.traversal.ToolChangeUpdate"},
        {address::ToolChangeAnimationBranch, {0x39400ee8,0x34000b08,0x52800020,0x9402a25a,0xf949d809}, 3, "controls.traversal.ToolChangeAnimationBranch"},
        {address::StableActions, {0xd103c3ff,0xfd003bea,0x6d0823e9,0xa9097bfd,0xa90a6ffc}, 3, "controls.traversal.StableActions"},
        {address::RequestAction, {0xa9be7bfd,0xf9000bf3,0x910003fd,0xb9001fa1,0xb9401fb3}, 3, "controls.traversal.RequestAction"},
        {qol::game::v303::inventory::CapacityCode.offset, qol::game::v303::inventory::CapacityCode.words, 3, qol::game::v303::inventory::CapacityCode.name},
        {qol::game::v303::inventory::ItemAtCode.offset, qol::game::v303::inventory::ItemAtCode.words, 3, qol::game::v303::inventory::ItemAtCode.name},
        {qol::game::v303::inventory::IsEquippedCode.offset, qol::game::v303::inventory::IsEquippedCode.words, 3, qol::game::v303::inventory::IsEquippedCode.name},
        {qol::game::v303::inventory::HeldCode.offset, qol::game::v303::inventory::HeldCode.words, 3, qol::game::v303::inventory::HeldCode.name},
        {qol::game::v303::inventory::ItemParameterCode.offset, qol::game::v303::inventory::ItemParameterCode.words, 3, qol::game::v303::inventory::ItemParameterCode.name},
        {qol::game::v303::inventory::ToolKindCode.offset, qol::game::v303::inventory::ToolKindCode.words, 3, qol::game::v303::inventory::ToolKindCode.name},
};
inline constexpr native::FunctionBinding<bool (*)(void *, unsigned, const void *)> RequestAction{"traversal.RequestAction", address::RequestAction};
inline constexpr native::FunctionBinding<unsigned (*)(void *)> EquippedToolKind{"traversal.EquippedToolKind", address::EquippedToolKind};
inline constexpr native::FunctionBinding<unsigned (*)(void *, unsigned)> TryVault{"traversal.TryVault", address::TryVault};
inline constexpr native::FunctionBinding<unsigned (*)(void *, unsigned)> TryLadder{"traversal.TryLadder", address::TryLadder};
inline constexpr native::FunctionBinding<unsigned (*)(void *, unsigned)> TryPlacedLadder{"traversal.TryPlacedLadder", address::TryPlacedLadder};
inline constexpr native::FunctionBinding<void *(*)(unsigned)> PlayerParameters{"traversal.PlayerParameters", address::PlayerParameters};
constexpr std::array<std::uint32_t, 5> HookWords(std::uintptr_t offset) {
    for (const auto &check : Checks) if (check.offset == offset) return check.words;
    for (const auto &check : PlacedChecks) if (check.offset == offset) return check.words;
    for (const auto &check : SwapChecks) if (check.offset == offset) return check.words;
    return {}; // Unknown sites fail compatibility checks; never infer code.
}
} // namespace qol::controls::game::v303::traversal
