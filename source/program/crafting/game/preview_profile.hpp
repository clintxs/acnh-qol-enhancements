#pragma once
#include "program/core/native/executable_view.hpp"
#include "program/crafting/recipe_preview_hint.hpp"
namespace qol::crafting::game::v303::preview {
namespace address {
constexpr std::uintptr_t BuildLayout = 0x00267d64, FindPane = 0x0027d1d0;
constexpr std::uintptr_t PhoneBind = 0x02e9cde0, PhoneFrame = 0x02e9fbb0;
constexpr std::uintptr_t PhoneDeactivate = 0x02e9d538;
constexpr std::uintptr_t PhoneRendererModeSlot = 0x049e8378, PhoneRendererMode = 0x02ea0754;
constexpr std::uintptr_t RecipeFocus = 0x02e96a80, RecipeLeave = 0x02e99d90;
constexpr std::uintptr_t PageInput = 0x03c58d24, UiContext = 0x04ab2230;
constexpr std::uintptr_t Application = 0x059f83f0;
constexpr std::uintptr_t CreateItemModel = 0x032bd990, ReleaseModel = 0x032ba5e4;
constexpr std::uintptr_t ChangeItemModel = 0x032ba168;
constexpr std::uintptr_t ResetRecipeGrid = 0x03764290, ActivateRecipeGrid = 0x0335b4a0;
constexpr std::uintptr_t FocusRecipeGrid = 0x03763e14, GridRowForIndex = 0x03765104;
constexpr std::uintptr_t GridRequestVtable = 0x04a2b020;
constexpr std::uintptr_t ChangeState = 0x03dbb160;
constexpr std::uintptr_t DrawModelLayer = 0x039392b0, RenderLayers = 0x04abdb78;
constexpr std::uintptr_t SetModelRotation = 0x032b9e08;
constexpr std::uintptr_t BuildModelMatrix = 0x006021a4, DrawLayerContents = 0x039f63f0;
constexpr std::uintptr_t Atan2 = 0x03f03370; // Main's atan2f import, no module math-library dependency.
constexpr std::uintptr_t ItemParamGlobalSlot = 0x04ac0428;
constexpr std::uintptr_t UpdateItemActor = 0x0192a590;
constexpr std::uintptr_t UpdateModelTransform = 0x032b9fcc, EntityManager = 0x04ab27c0;
constexpr std::uintptr_t CapturePicture = 0x03a17390, AlternateCapturePicture = 0x03a174f0;
constexpr std::uintptr_t WorkbenchPageVtable = 0x04a3b3e0;
}
namespace field {
constexpr std::size_t Resident = 0x1f8, PhoneLayout = 0x210, Selection = 0x218;
constexpr std::size_t Recipes = 0x8a350, State = 0x1b0, Requests = 0x1f0;
constexpr std::size_t LayoutInstances = 0x3f8, LayoutAccessor = 0x58;
constexpr std::size_t GetNativeLayout = 0x28, LayoutRoot = 0x18, Grid = 0x580;
constexpr std::size_t Rows = 0x210, RowCount = 0x208, PaneFlags = 0x58;
constexpr std::size_t ShortcutHint = 0x570, HintInput = 0xa0;
constexpr std::size_t InputRow = 0x10, InputKind = 0x14;
constexpr std::size_t PageInputContext = 0x210, InputDisabled = 0x54, InputBlockMask = 0x58;
constexpr std::size_t ApplicationMasks = 0x418, GlobalBlockMask = 0x10;
constexpr std::size_t Controller = 0x30, Parameters = 0x28;
constexpr std::size_t ModelMode = 0x50, ModelHandle = 0x6c;
constexpr std::size_t ModelState = 0x10, BackgroundPage = 0x58, VisibilityPage = 0x60;
constexpr std::size_t ModelReady = 0x69, ModelFlags = 0x74, ModelAngles = 0x80;
constexpr std::size_t GridColumns = 0x2e8, GridVisibleItems = 0x2f4, GridTopRow = 0x430;
constexpr std::size_t GridItems = 0x3cc, GridState = 0x298, LeftStick = 4;
constexpr std::size_t RightStick = 0x0c, ConsumedAxes = 0x40;
constexpr std::size_t LayerCount = 0x20, Layers = 0x28, LayerClearColor = 0x9c;
constexpr std::size_t DrawLayer = 0x3d0, RendererState = 0x248, DrawInfo = 0x18, DrawView = 8;
constexpr std::size_t ComponentOwner = 0x48, ComponentAngles = 0x2bc, ComponentPoseSize = 0x2c8;
constexpr std::size_t SetComponentAngles = 0x150;
constexpr std::size_t ComponentScale = 0x2a0, PrimaryAngles = 0x2b0;
constexpr std::size_t CompletedItem = 0x94;
constexpr std::size_t EntityHandle = 0x230, ParentHandle = 0x234, EntityComponent = 0x68;
constexpr std::size_t EntityRegistry = 0x1c0, RegistryEntries = 8, EntityFlags = 0x248;
constexpr std::size_t EntityKind = 0x15c, ItemChildren = 0x274, ItemChildCount = 0x394;
constexpr std::size_t EntityPhase = 0x288, EntityStage = 0x5e8;
constexpr std::size_t ItemPoseState = 0x39c, ItemData = 0x360;
}
namespace itemTable {
constexpr std::size_t FirstId = 0x1f8, RowCount = 0x1d8, Rows = 0x1e0;
constexpr std::size_t HeaderV0 = 12, HeaderV1 = 28, DescriptorSize = 8;
constexpr unsigned MaxRows = 65536, MaxRowBytes = 4096, MaxFields = 512;
constexpr unsigned MaxHeaderDistance = 64 * 1024 * 1024;
constexpr unsigned KindField = 0x5270eb75, IdField = 0x54706054;
// ItemKind CRC32 values: English and Japanese resource spellings.
constexpr std::array<unsigned, 8> FixedKinds{
    0x055c8679, 0xa661cd8e, // RoomWall / 内装：壁紙
    0xc354e1f6, 0x8b19cf5d, // RoomFloor / 内装：床板
    0x11e75a84, 0x8e4773fe, // Rug / ラグ
    0x2c28165c, 0x1ffca4d6  // Fence / 柵
};
} // namespace itemTable
struct ModelMatrix { float values[12]; }; // ARM64 returns this 48-byte object through x8.
struct GridRequest {
    std::uintptr_t vtable{};
    int count{}, first{}, selected{};
    unsigned char padMinimum{}, padRow{}, enabled{1}, loop{}, reverse{}, alternateAxis{1};
    unsigned char cursor{}, animate{}, flags{}, callback{};
    unsigned short padding{};
    int pageRows{}, exitFrames{12}, entryFrames{4};
    unsigned resident{};
};
static_assert(sizeof(GridRequest) == 0x30 && offsetof(GridRequest, resident) == 0x2c);
struct ItemRequest {
            unsigned display{};
            std::uint32_t itemLow{}, itemHigh{};
            std::uint16_t recipe{0xfffe}, padding{};
            void *visibilityPage{};
};
static_assert(sizeof(ItemRequest) == 24 && offsetof(ItemRequest, visibilityPage) == 16);
inline constexpr native::Fingerprint<> DiyChecks[]{
        {address::RecipeLeave, {0xd65f03c0, 0xd65f03c0, 0xb9401028, 0x34000108, 0x7100051f}, "crafting.preview.RecipeLeave"},
        {recipe_hint::BuildFooter, {0xd10383ff, 0xa9097bfd, 0xf90053f9, 0xa90b5ff8, 0xa90c57f6}, "crafting.preview.BuildFooter"},
        {recipe_hint::AppendHint, {0xb941f008, 0x3100051f, 0xb941f409, 0xb941b00a, 0x3a410920}, "crafting.preview.AppendHint"},
        {address::BuildLayout, {0xa9ba7bfd, 0xa9016ffc, 0x910003fd, 0xa90267fa, 0xa9035ff8}, "crafting.preview.BuildLayout"},
        {address::PhoneBind, {0xd10243ff, 0xa9037bfd, 0xa9046ffc, 0xa90567fa, 0xa9065ff8}, "crafting.preview.PhoneBind"},
        {address::PhoneFrame, {0xa9bb7bfd, 0xa90167fa, 0xa9025ff8, 0xa90357f6, 0xa9044ff4}, "crafting.preview.PhoneFrame"},
        {address::PhoneDeactivate, {0xa9bd7bfd, 0xf9000bf5, 0xa9024ff4, 0x910003fd, 0xf9410814}, "crafting.preview.PhoneDeactivate"},
        {address::CreateItemModel, {0x6db923e9, 0xa9017bfd, 0x910043fd, 0xf90013fc, 0xa90367fa}, "crafting.preview.CreateItemModel"},
        {address::ReleaseModel, {0xd100c3ff, 0xa9017bfd, 0xf90013f3, 0x910043fd, 0xb9405008}, "crafting.preview.ReleaseModel"},
        {address::ChangeState, {0xa9be7bfd, 0xa9014ff4, 0x910003fd, 0xb9400808, 0x2a0103f3}, "crafting.preview.ChangeState"},
        {address::CapturePicture, {0xa9bd7bfd, 0xa90157f6, 0xa9024ff4, 0x910003fd, 0xd00085c8}, "crafting.preview.CapturePicture"},
        {address::AlternateCapturePicture, {0xa9bd7bfd, 0xa90157f6, 0xa9024ff4, 0x910003fd, 0xd00085c8}, "crafting.preview.AlternateCapturePicture"},
        {address::FindPane, {0xa9bd7bfd, 0xf9000bf5, 0xa9024ff4, 0x910003fd, 0xaa0003f4}, "crafting.preview.FindPane"},
        // Hotel overlay uses Pane geometry, the matrix dirty flag and ordered children.
        {0x0027bfb4, {0xbd4002c0, 0xbd003260, 0xbd4006c0, 0xbd003660, 0xbd400ac0}, "crafting.preview.Code_0027bfb4"},
        {0x0027bfe4, {0xbd400380, 0xbd004a60, 0xbd400780, 0xbd004e60, 0xf9401e88}, "crafting.preview.Code_0027bfe4"},
        {0x0027d550, {0x121c0109, 0x2a491069, 0x36000d89, 0x121b7908, 0xaa1303e0}, "crafting.preview.Code_0027d550"},
        {0x0027e208, {0x910082b6, 0xf94016b5, 0xeb1602bf, 0x54ffff20, 0xd503201f}, "crafting.preview.Code_0027e208"},
        {0x03c66298, {0xf9402c00, 0xaa0203f5, 0xaa0103f4, 0xf9000fbf, 0xb40000a0}, "crafting.preview.Code_03c66298"},
        {0x02e93798, {0x528000a8, 0x9a9f1102, 0x94231d66, 0x395ed2a8, 0xf902ba60}, "crafting.preview.Code_02e93798"},
        {address::RecipeFocus, {0xf9410808, 0xf942c109, 0xb4000789, 0xf940042b, 0xb400074b}, "crafting.preview.RecipeFocus"},
        {0x03c4b880, {0xd10203ff, 0xa9027bfd, 0xa9036ffc, 0xa90467fa, 0xa9055ff8}, "crafting.preview.Code_03c4b880"},
        {0x02b0c810, {0xf9400268, 0xaa1303e0, 0xf940d108, 0xd63f0100, 0xf9400268}, "crafting.preview.Code_02b0c810"},
        {address::PhoneRendererMode, {0x52800340, 0xd65f03c0, 0xf9400408, 0xb4000148, 0xa9412802}, "crafting.preview.PhoneRendererMode"},
        {address::SetModelRotation, {0xa9bc7bfd, 0xf9000bf7, 0xa90257f6, 0xa9034ff4, 0x910003fd}, "crafting.preview.SetModelRotation"},
        {0x032ba000, {0x91020261, 0xaa1303e0, 0x97ffff80, 0x36000200, 0xb9405268}, "crafting.preview.Code_032ba000"},
        {0x03c4bac4, {0xbd400100, 0xbc038c20, 0xbd400500, 0xf8418c08, 0xf90003fb}, "crafting.preview.Code_03c4bac4"},
        {address::UpdateModelTransform, {0xd100c3ff, 0xa9017bfd, 0xa9024ff4, 0x910043fd, 0x3941d008}, "crafting.preview.UpdateModelTransform"},
        {0x02b0e20c, {0xf0015948, 0x91064108, 0xf9400108, 0xf9401900, 0x141eaf6c}, "crafting.preview.Code_02b0e20c"},
        {address::DrawModelLayer, {0xd10203ff, 0x6d032beb, 0x6d0423e9, 0xa9057bfd, 0xa90657f6}, "crafting.preview.DrawModelLayer"},
        {address::BuildModelMatrix, {0xb942b00c, 0x12005d8f, 0xb942bc09, 0xb942c00b, 0xb942c40a}, "crafting.preview.BuildModelMatrix"},
        {address::DrawLayerContents, {0x6db63bef, 0x6d0133ed, 0x6d022beb, 0x6d0323e9, 0xa9047bfd}, "crafting.preview.DrawLayerContents"},
        {0x032b9750, {0xaa1403e1, 0x940001ad, 0x9000f3c8, 0x91240108, 0xf9400108}, "crafting.preview.Code_032b9750"},
        {0x01513a48, {0xd101c3ff, 0x6d012beb, 0x6d0223e9, 0xa9037bfd, 0xa9045ff8}, "crafting.preview.Code_01513a48"},
        {0x032be2b0, {0xb9406c09, 0x34000629, 0xa9be7bfd, 0xa9014ff4, 0x910003fd}, "crafting.preview.Code_032be2b0"},
        {0x032ba034, {0xaa1303e0, 0x9400109e, 0x36000080, 0x3941d268, 0x121f1908}, "crafting.preview.Code_032ba034"},
        {0x0192a1f0, {0xa9bb7bfd, 0xf9000bf9, 0xa9025ff8, 0xa90357f6, 0xa9044ff4}, "crafting.preview.Code_0192a1f0"},
        {0x019362f4, {0xd101c3ff, 0xa9027bfd, 0xf9001bf9, 0xa9045ff8, 0xa90557f6}, "crafting.preview.Code_019362f4"},
        // Native grid restoration, focus mapping and existing-actor item changes.
        {address::ChangeItemModel, {0xb9406c08, 0x34000d68, 0xd101c3ff, 0xa9037bfd, 0xf90023f7}, "crafting.preview.ChangeItemModel"},
        {address::ResetRecipeGrid, {0xd10203ff, 0xfd001be8, 0xf90027f9, 0xa903fbfd, 0xa9055ff8}, "crafting.preview.ResetRecipeGrid"},
        {address::ActivateRecipeGrid, {0xb40003a0, 0xb9429808, 0x7100091f, 0x54000063, 0xb4000341}, "crafting.preview.ActivateRecipeGrid"},
        {address::FocusRecipeGrid, {0xf9416c08, 0xf100011f, 0xb9429809, 0x7a421920, 0x54000d61}, "crafting.preview.FocusRecipeGrid"},
        {address::GridRowForIndex, {0xb943d408, 0x6b01011f, 0x5400038d, 0xb942e808, 0x1ac80c29}, "crafting.preview.GridRowForIndex"},
        {0x03c49700, {0xbd54a260, 0xb901233c, 0xbd000720, 0x1400000a, 0xbd54a660}, "crafting.preview.Code_03c49700"},
        {0x03c49710, {0xbd54a660, 0xb901233c, 0x5280002e, 0x14000005, 0xbd54a260}, "crafting.preview.Code_03c49710"},
        {address::UpdateItemActor, {0xd104c3ff, 0x6d093bef, 0x6d0a33ed, 0x6d0b2beb, 0x6d0c23e9}, "crafting.preview.UpdateItemActor"},
        {0x019282fc, {0xd10103ff, 0xa9017bfd, 0xf90013f5, 0xa9034ff4, 0x910043fd}, "crafting.preview.Code_019282fc"},
        {0x005f7b4c, {0xb9400028, 0xb902b008, 0xb9400428, 0xb902b408, 0xb9400828}, "crafting.preview.Code_005f7b4c"},
        {0x00690504, {0xbd42b120, 0xbd42b521, 0xbd42b922, 0x7e21d800, 0x7e21d821}, "crafting.preview.Code_00690504"},
        {address::Atan2, {0xb0005ef0, 0xf940ba11, 0x9105c210, 0xd61f0220, 0xb0005ef0}, "crafting.preview.Atan2"},
    };
inline constexpr native::Fingerprint<> WorkbenchChecks[]{
        {address::UpdateItemActor, {0xd104c3ff, 0x6d093bef, 0x6d0a33ed, 0x6d0b2beb, 0x6d0c23e9}, "crafting.preview.UpdateItemActor"},
        {address::BuildModelMatrix, {0xb942b00c, 0x12005d8f, 0xb942bc09, 0xb942c00b, 0xb942c40a}, "crafting.preview.BuildModelMatrix"},
        {address::Atan2, {0xb0005ef0, 0xf940ba11, 0x9105c210, 0xd61f0220, 0xb0005ef0}, "crafting.preview.Atan2"},
        {0x005f7b4c, {0xb9400028, 0xb902b008, 0xb9400428, 0xb902b408, 0xb9400828}, "crafting.preview.Code_005f7b4c"},
        {0x019282fc, {0xd10103ff, 0xa9017bfd, 0xf90013f5, 0xa9034ff4, 0x910043fd}, "crafting.preview.Code_019282fc"},
        {0x032bd9cc, {0x1ac82128, 0x5285bd89, 0x72a00069, 0x0a090108, 0x7a409904}, "crafting.preview.Code_032bd9cc"},
        {0x0346e7d8, {0xf902ce9f, 0x9100414a, 0xf902d29f, 0xb9004a88, 0xf900028a}, "crafting.preview.Code_0346e7d8"},
    };
inline constexpr native::FunctionBinding<void *(*)(void *, const char *, bool)> FindPane{"preview.FindPane", address::FindPane};
inline constexpr native::FunctionBinding<ModelMatrix (*)(void *)> BuildModelMatrix{"preview.BuildModelMatrix", address::BuildModelMatrix};
inline constexpr native::FunctionBinding<float (*)(float, float)> Atan2{"preview.Atan2", address::Atan2};
inline constexpr native::FunctionBinding<void (*)(void *, void *)> RecipeFocus{"preview.RecipeFocus", address::RecipeFocus};
inline constexpr native::FunctionBinding<void (*)(void *, void *)> RecipeLeave{"preview.RecipeLeave", address::RecipeLeave};
inline constexpr native::FunctionBinding<void (*)(void *, unsigned)> ReleaseModel{"preview.ReleaseModel", address::ReleaseModel};
inline constexpr native::FunctionBinding<void (*)(void *, unsigned)> ChangeState{"preview.ChangeState", address::ChangeState};
inline constexpr native::FunctionBinding<void *(*)(void *)> CapturePicture{"preview.CapturePicture", address::CapturePicture};
inline constexpr native::FunctionBinding<void *(*)(void *)> AlternateCapturePicture{"preview.AlternateCapturePicture", address::AlternateCapturePicture};
inline constexpr native::FunctionBinding<void (*)(void *, const GridRequest *)> ResetRecipeGrid{"preview.ResetRecipeGrid", address::ResetRecipeGrid};
inline constexpr native::FunctionBinding<void (*)(void *, void *)> ActivateRecipeGrid{"preview.ActivateRecipeGrid", address::ActivateRecipeGrid};
inline constexpr native::FunctionBinding<void (*)(void *, bool)> FocusRecipeGrid{"preview.FocusRecipeGrid", address::FocusRecipeGrid};
inline constexpr native::FunctionBinding<void *(*)(void *, unsigned)> GridRowForIndex{"preview.GridRowForIndex", address::GridRowForIndex};
inline constexpr native::FunctionBinding<void (*)(void *, const void *)> ChangeItemModel{"preview.ChangeItemModel", address::ChangeItemModel};
inline constexpr native::FunctionBinding<void (*)(void *, const ItemRequest *)> CreateItemModel{"preview.CreateItemModel", address::CreateItemModel};
} // namespace qol::crafting::game::v303::preview
