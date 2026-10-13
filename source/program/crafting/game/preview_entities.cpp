#include "preview_context.hpp"
namespace qol::crafting::preview_internal {
bool UsesFixedPreviewPose(std::uint16_t itemId) {
    // Read the same live ItemParam table as native item-root update 0192A590.
    // Keep this independent of Catalog, which may be disabled. Validate row identity and
    // runtime header fixups before classifying; do not guess from recipe IDs.
    auto slot = game::preview_native::ItemParamGlobalSlot(Main);
    auto table = slot ? At<void *>(slot) : nullptr;
    if (!table) return false;
    const auto firstId = At<std::uint16_t>(table, itemTable::FirstId);
    const auto count = At<unsigned>(table, itemTable::RowCount);
    auto rows = At<void **>(table, itemTable::Rows);
    if (!rows || count > itemTable::MaxRows || itemId < firstId || unsigned(itemId - firstId) >= count)
        return false;
    auto row = rows[itemId - firstId];
    if (!row) return false;
    const auto distance = At<unsigned>(row);
    if (distance < itemTable::HeaderV1 || distance > itemTable::MaxHeaderDistance ||
        reinterpret_cast<std::uintptr_t>(row) < distance) return false;
    auto header = static_cast<std::byte *>(row) - distance;
    const auto entries = At<unsigned>(header), stride = At<unsigned>(header, 4);
    const auto fields = At<std::uint16_t>(header, 8);
    const auto version = At<unsigned char>(header, 10);
    if (!entries || entries > itemTable::MaxRows || stride < 4 || stride > itemTable::MaxRowBytes ||
        !fields || fields > itemTable::MaxFields || version > 1) return false;
    auto descriptors = header + (version ? itemTable::HeaderV1 : itemTable::HeaderV0);
    auto firstRow = descriptors + fields * itemTable::DescriptorSize;
    if (static_cast<std::byte *>(row) < firstRow) return false;
    const auto rowOffset = static_cast<std::byte *>(row) - firstRow;
    if (rowOffset % stride || rowOffset / stride >= entries) return false;
    unsigned kind{};
    bool identityMatches{};
    for (unsigned i = 0; i < fields; ++i) {
        auto descriptor = descriptors + i * itemTable::DescriptorSize;
        const auto hash = At<unsigned>(descriptor), offset = At<unsigned>(descriptor, 4);
        if (hash == itemTable::KindField && offset <= stride - sizeof(unsigned))
            kind = At<unsigned>(row, offset);
        if (hash == itemTable::IdField && offset <= stride - sizeof(std::uint16_t))
            identityMatches = At<std::uint16_t>(row, offset) == itemId;
    }
    return identityMatches && std::find(itemTable::FixedKinds.begin(), itemTable::FixedKinds.end(), kind) !=
        itemTable::FixedKinds.end();
}
std::array<unsigned, 3> NativePoseAngles(void *component) {
    // 0192A590 selects per-item workbench poses, stored as secondary X-Z-Y
    // angles. Particles use primary Y-X-Z angles. Convert the native matrix
    // instead of copying Euler values or guessing a backpack-specific yaw.
    alignas(16) std::array<std::byte, field::ComponentPoseSize> pose{};
    std::memcpy(pose.data(), component, pose.size());
    At<void *>(pose.data(), field::ComponentOwner) = nullptr; // Bypass our render fallback.
    for (unsigned i = 0; i < 3; ++i) At<float>(pose.data(), field::ComponentScale + i * sizeof(float)) = 1;
    const auto matrix = profile::BuildModelMatrix.Resolve(Main)(pose.data());
    const auto &m = matrix.values;
    auto atan2 = profile::Atan2.Resolve(Main);
    const float horizontal = __builtin_sqrtf(m[4]*m[4] + m[5]*m[5]);
    const float pitch = atan2(-m[6], horizontal);
    // At a pole, choose zero roll and the equivalent yaw. Avoid undefined
    // atan2(0,0) and preserve a valid pose for all native preset angles.
    const float yaw = horizontal > 0.00001f ? atan2(m[2], m[10]) : atan2(-m[8], m[0]);
    const float roll = horizontal > 0.00001f ? atan2(m[4], m[5]) : 0;
    constexpr float AngleUnitsPerRadian = 683565275.5764316f;
    auto angle = [](float radians) { return static_cast<unsigned>(static_cast<std::int64_t>(radians * AngleUnitsPerRadian)); };
    return {angle(pitch), angle(yaw), angle(roll)};
}
void PublishCarriers(void *controller) {
    const auto handle = PreviewHandle.load(std::memory_order_acquire);
    if (!handle || At<unsigned>(controller, field::ModelHandle) != handle) {
        for (auto &carrier : PreviewCarriers) carrier.store(0, std::memory_order_release);
        return;
    }
    auto storage = game::preview_native::EntityManager(Main);
    auto manager = storage ? At<void *>(storage) : nullptr;
    auto registry = manager ? At<void *>(manager, field::EntityRegistry) : nullptr;
    auto entries = registry ? At<void *>(registry, field::RegistryEntries) : nullptr;
    auto actor = entries ? At<void *>(entries, (handle >> 17) & 0x7ff8u) : nullptr;
    const bool valid = actor && At<unsigned>(actor, field::EntityHandle) == handle &&
        !(At<unsigned>(actor, field::EntityFlags) & 2) && At<unsigned>(actor, field::EntityKind) == PreviewActorKind;
    const auto count = valid ? At<unsigned>(actor, field::ItemChildCount) : 0;
    for (unsigned i = 0; i < PreviewCarriers.size(); ++i) {
        const auto child = count <= PreviewCarriers.size() && i < count ?
            At<unsigned>(actor, field::ItemChildren + i * sizeof(unsigned)) : 0;
        const auto previous = PreviewCarriers[i].exchange(child, std::memory_order_release);
        if (child && child != previous)
            Logging.Debug("[QoL Enhancements/Crafting] DIY preview: mesh carrier published; slot=%u handle=%u\n", i, child);
    }
}
void *WorkbenchActor(void *actor) {
    auto controller = WorkbenchController.load(std::memory_order_acquire);
    const auto handle = WorkbenchHandle.load(std::memory_order_acquire);
    if (!actor || !controller || !handle || At<unsigned>(actor, field::EntityHandle) != handle ||
        At<unsigned>(actor, field::EntityKind) != PreviewActorKind ||
        (At<unsigned>(actor, field::EntityFlags) & 2) ||
        At<unsigned>(controller, field::ModelHandle) != handle ||
        At<unsigned>(controller, field::ModelMode) != WorkbenchMode.load(std::memory_order_acquire) ||
        At<int>(controller, field::ModelState) != 1 ||
        controller == PreviewController.load(std::memory_order_acquire)) return nullptr;
    return controller;
}
void TraceWorkbenchModel(void *actor) {
    if (!WorkbenchTiltEnabled.load(std::memory_order_acquire) || !actor ||
        At<unsigned>(actor, field::EntityKind) != PreviewActorKind) return;
    auto context = UiContext();
    auto controller = context ? At<void *>(context, field::Controller) : nullptr;
    const auto handle = At<unsigned>(actor, field::EntityHandle);
    if (!controller || controller == PreviewController.load(std::memory_order_acquire) ||
        !handle || At<unsigned>(controller, field::ModelHandle) != handle) return;
    // Scene-thread-only, bounded evidence even if no page-input binding matches.
    static std::array<unsigned, 8> seen{};
    static unsigned count{};
    if (count == seen.size() || std::find(seen.begin(), seen.end(), handle) != seen.end()) return;
    seen[count++] = handle;
    Logging.Debug("[QoL Enhancements/Crafting] workbench tilt: model probe; mode=%u state=%d root=%u pose=%u children=%u\n",
                At<unsigned>(controller, field::ModelMode), At<int>(controller, field::ModelState), handle,
                At<unsigned>(actor, field::ItemPoseState), At<unsigned>(actor, field::ItemChildCount));
}
void *PreviewCarrier(void *actor, unsigned index) {
    const auto count = At<unsigned>(actor, field::ItemChildCount);
    if (count > WorkbenchPoses.size() || index >= count) return nullptr;
    const auto handle = At<unsigned>(actor, field::ItemChildren + index * sizeof(unsigned));
    auto storage = game::preview_native::EntityManager(Main);
    auto manager = storage ? At<void *>(storage) : nullptr;
    auto registry = manager ? At<void *>(manager, field::EntityRegistry) : nullptr;
    auto entries = registry ? At<void *>(registry, field::RegistryEntries) : nullptr;
    auto child = entries && handle ? At<void *>(entries, (handle >> 17) & 0x7ff8u) : nullptr;
    if (!child || At<unsigned>(child, field::EntityHandle) != handle ||
        At<unsigned>(child, field::ParentHandle) != At<unsigned>(actor, field::EntityHandle) ||
        At<unsigned>(child, field::EntityKind) != 6 || (At<unsigned>(child, field::EntityFlags) & 2)) return nullptr;
    auto component = At<void *>(child, field::EntityComponent);
    return component && At<void *>(component, field::ComponentOwner) == child ? component : nullptr;
}
void RestoreWorkbenchPose(void *actor) {
    auto controller = WorkbenchActor(actor);
    if (!controller || (At<unsigned char>(controller, field::ModelFlags) & MaterialPending)) return;
    for (unsigned i = 0; i < WorkbenchPoses.size(); ++i) {
        auto &pose = WorkbenchPoses[i];
        auto component = PreviewCarrier(actor, i);
        if (!pose.component || component != pose.component ||
            At<unsigned>(At<void *>(component, field::ComponentOwner), field::EntityHandle) != pose.handle) continue;
        // Undo only our last contribution before native yaw/framing runs.
        // Preserve any native primary rotation written since the scene tick.
        for (unsigned axis = 0; axis < 3; ++axis)
            At<unsigned>(component, field::PrimaryAngles + axis*4) += pose.primary[axis] - pose.applied[axis];
        if (!At<std::uint64_t>(component, field::ComponentAngles) &&
            !At<unsigned>(component, field::ComponentAngles + 8))
            std::memcpy(static_cast<std::byte *>(component) + field::ComponentAngles,
                        pose.secondary.data(), sizeof(pose.secondary));
        pose.component = nullptr;
    }
}
void HoldWorkbenchPose(void *actor) {
    // A queued recipe resets the controller before the outgoing mesh vanishes.
    // Preserve the last displayed pose until native framing replaces it. Full
    // child handles prevent carrying the old tilt into newly allocated meshes.
    if (!WorkbenchActor(actor)) return;
    for (unsigned i = 0; i < WorkbenchPoses.size(); ++i) {
        const auto &pose = WorkbenchPoses[i];
        auto component = PreviewCarrier(actor, i);
        if (!pose.component || component != pose.component ||
            At<unsigned>(At<void *>(component, field::ComponentOwner), field::EntityHandle) != pose.handle) continue;
        auto methods = At<void *>(component);
        auto setAngles = methods ? At<void (*)(void *, const void *)>(methods, field::SetComponentAngles) : nullptr;
        if (!setAngles) continue;
        setAngles(component, pose.applied.data());
        At<std::uint64_t>(component, field::ComponentAngles) = 0;
        At<unsigned>(component, field::ComponentAngles + 8) = 0;
    }
}
void ApplyWorkbenchTilt(void *actor) {
    auto controller = WorkbenchActor(actor);
    if (!controller || At<unsigned>(actor, field::ItemPoseState) != 0 ||
        (At<unsigned char>(controller, field::ModelFlags) & MaterialPending)) return;
    const auto tilt = WorkbenchTiltAngle.load(std::memory_order_acquire);
    if (!tilt || UsesFixedPreviewPose(At<std::uint16_t>(actor, field::ItemData))) return;
    for (unsigned i = 0; i < WorkbenchPoses.size(); ++i) {
        auto component = PreviewCarrier(actor, i);
        auto methods = component ? At<void *>(component) : nullptr;
        auto setAngles = methods ? At<void (*)(void *, const void *)>(methods, field::SetComponentAngles) : nullptr;
        if (!setAngles) continue;
        auto &pose = WorkbenchPoses[i];
        pose.component = component;
        pose.handle = At<unsigned>(At<void *>(component, field::ComponentOwner), field::EntityHandle);
        std::memcpy(pose.primary.data(), static_cast<std::byte *>(component) + field::PrimaryAngles, sizeof(pose.primary));
        std::memcpy(pose.secondary.data(), static_cast<std::byte *>(component) + field::ComponentAngles, sizeof(pose.secondary));
        pose.applied = NativePoseAngles(component);
        pose.applied[0] += tilt;
        setAngles(component, pose.applied.data());
        At<std::uint64_t>(component, field::ComponentAngles) = 0;
        At<unsigned>(component, field::ComponentAngles + 8) = 0;
        if (WorkbenchAppliedReports.fetch_add(1, std::memory_order_relaxed) < 4)
            Logging.Debug("[QoL Enhancements/Crafting] workbench tilt: scene applied; root=%u carrier=%u pitch=%08x\n",
                        At<unsigned>(actor, field::EntityHandle), pose.handle, pose.applied[0]);
    }
}
void UpdatePreviewActor::Callback(void *actor){
        TraceWorkbenchModel(actor);
        const bool framing = actor && At<unsigned>(actor, field::ItemPoseState) == 2;
        auto workbench = WorkbenchActor(actor);
        const bool holdWorkbench = workbench && !framing &&
            (At<unsigned char>(workbench, field::ModelFlags) & MaterialPending);
        RestoreWorkbenchPose(actor);
        Orig(actor); // Finish native item/category framing and child creation first.
        if (holdWorkbench) HoldWorkbenchPose(actor);
        else ApplyWorkbenchTilt(actor);
        const auto handle = PreviewHandle.load(std::memory_order_acquire);
        if (!handle || !actor ||
            At<unsigned>(actor, field::EntityHandle) != handle ||
            At<unsigned>(actor, field::EntityKind) != PreviewActorKind ||
            (At<unsigned>(actor, field::EntityFlags) & 2)) return;
        const auto count = At<unsigned>(actor, field::ItemChildCount);
        if (count > PreviewCarriers.size()) return;
        const auto epoch = PreviewPoseEpoch.load(std::memory_order_acquire);
        // State 1 loads children; state 2 applies category poses/bounds, then
        // becomes 0. Never capture an unframed child or the previous recipe.
        if (At<unsigned>(actor, field::ItemPoseState) != 0 ||
            At<std::uint64_t>(actor, field::ItemData) != PreviewItem.load(std::memory_order_relaxed)) {
            // Native replacement may reset a still-visible carrier before its
            // old capture disappears. Keep that carrier's last displayed pose;
            // generation/parent/component checks exclude newly allocated items.
            if (!PreviewFixedPose.load(std::memory_order_acquire))
                for (unsigned i = 0; i < count; ++i) {
                    auto component = PreviewCarrier(actor, i);
                    auto &pose = CarrierPoses[i];
                    if (!component || pose.component != component ||
                        At<unsigned>(At<void *>(component, field::ComponentOwner), field::EntityHandle) != pose.handle) continue;
                    auto methods = At<void *>(component);
                    auto setAngles = methods ? At<void (*)(void *, const void *)>(methods, field::SetComponentAngles) : nullptr;
                    if (!setAngles) continue;
                    setAngles(component, pose.displayed.data());
                    At<std::uint64_t>(component, field::ComponentAngles) = 0;
                    At<unsigned>(component, field::ComponentAngles + 8) = 0;
                }
            return;
        }
        // Navigation publishes a request while the previous mesh can still be
        // drawn. Switch its pose policy only after native framing catches up.
        const bool fixed = PreviewRequestedFixedPose.load(std::memory_order_acquire);
        PreviewFixedPose.store(fixed, std::memory_order_release);
        if (fixed) return;
        auto storage = game::preview_native::EntityManager(Main);
        auto manager = storage ? At<void *>(storage) : nullptr;
        auto registry = manager ? At<void *>(manager, field::EntityRegistry) : nullptr;
        auto entries = registry ? At<void *>(registry, field::RegistryEntries) : nullptr;
        if (!entries) return;
        const auto delta = PoseDelta();
        bool applied = false;
        for (unsigned i = 0; i < count; ++i) {
            const auto childHandle = At<unsigned>(actor, field::ItemChildren + i * sizeof(unsigned));
            auto child = childHandle ? At<void *>(entries, (childHandle >> 17) & 0x7ff8u) : nullptr;
            if (!child || At<unsigned>(child, field::EntityHandle) != childHandle ||
                At<unsigned>(child, field::ParentHandle) != handle ||
                At<unsigned>(child, field::EntityKind) != 6 ||
                (At<unsigned>(child, field::EntityFlags) & 2)) continue;
            auto component = At<void *>(child, field::EntityComponent);
            if (!component || At<void *>(component, field::ComponentOwner) != child) continue;
            auto methods = At<void *>(component);
            auto setAngles = methods ? At<void (*)(void *, const void *)>(methods, field::SetComponentAngles) : nullptr;
            if (!setAngles) continue;
            auto &pose = CarrierPoses[i];
            if (framing || pose.epoch != epoch || pose.handle != childHandle || pose.component != component) {
                pose = {epoch, childHandle, component, NativePoseAngles(component)};
                Logging.Debug("[QoL Enhancements/Crafting] DIY preview: native initial pose; item=%u slot=%u xyz=%08x/%08x/%08x\n",
                            unsigned(At<std::uint64_t>(actor, field::ItemData) & 0xffff), i,
                            pose.angles[0], pose.angles[1], pose.angles[2]);
            }
            auto angles = pose.angles;
            for (unsigned axis = 0; axis < angles.size(); ++axis) angles[axis] += delta[axis];
            setAngles(component, angles.data()); // Native primary XYZ setter.
            pose.displayed = angles;
            // Apply the viewing pose exactly once. Leaving it in both fields
            // would rotate drawing twice while effects used only the primary.
            At<std::uint64_t>(component, field::ComponentAngles) = 0;
            At<unsigned>(component, field::ComponentAngles + 8) = 0;
            applied = true;
        }
        if (applied && !PreviewScenePose.exchange(true, std::memory_order_acq_rel))
            Logging.Debug("[QoL Enhancements/Crafting] DIY preview: primary scene rotation active; handle=%u\n", handle);
    }

}
