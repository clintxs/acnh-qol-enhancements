#pragma once
#include "program/core/native/executable_view.hpp"
namespace qol::game::v303::inventory {
using Item = std::uint64_t;
// Nontrivial native Item return: AArch64 hidden X8 result, never X0.
struct NativeItem { Item value; ~NativeItem() {} };
static_assert(sizeof(NativeItem) == 8);
inline constexpr native::FunctionBinding<unsigned (*)(const unsigned char *)> Capacity{"inventory.Capacity", 0x02572e00};
inline constexpr native::FunctionBinding<const Item *(*)(const unsigned char *, unsigned)> ItemAt{"inventory.ItemAt", 0x02577ea0};
inline constexpr native::FunctionBinding<bool (*)(const unsigned char *, const Item *, unsigned)> IsEquipped{"inventory.IsEquipped", 0x0256a0c0};
inline constexpr native::FunctionBinding<NativeItem (*)(const unsigned char *)> Held{"inventory.Held", 0x0242d8f0};
inline constexpr native::FunctionBinding<void (*)(void *, const Item *)> ItemParameter{"inventory.ItemParameter", 0x019d5d04};
inline constexpr native::FunctionBinding<unsigned (*)(const void *)> ToolKind{"inventory.ToolKind", 0x019d78cc};
inline constexpr native::Fingerprint<> CapacityCode{Capacity.offset, {0xd100c3ff,0xa9017bfd,0xa9024ff4,0x910043fd,0x39400008}, Capacity.name};
inline constexpr native::Fingerprint<> ItemAtCode{ItemAt.offset, {0xa9bc7bfd,0xf9000bf7,0xa90257f6,0xa9034ff4,0x910003fd}, ItemAt.name};
inline constexpr native::Fingerprint<> IsEquippedCode{IsEquipped.offset, {0xd10103ff,0xa9017bfd,0xa90257f6,0xa9034ff4,0x910043fd}, IsEquipped.name};
inline constexpr native::Fingerprint<> HeldCode{Held.offset, {0xa9bd7bfd,0xf9000bf5,0xa9024ff4,0x910003fd,0xaa0803f3}, Held.name};
inline constexpr native::Fingerprint<> ItemParameterCode{ItemParameter.offset, {0xa9bd7bfd,0xa90157f6,0xa9024ff4,0x910003fd,0xb0017b08}, ItemParameter.name};
inline constexpr native::Fingerprint<> ToolKindCode{ToolKind.offset, {0xf9400808,0xb4000488,0xb9400109,0x7100013f,0xfa491104}, ToolKind.name};
} // namespace qol::game::v303::inventory
