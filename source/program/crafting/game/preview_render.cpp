#include "preview_context.hpp"
namespace qol::crafting::preview_internal {
std::atomic<const void *> PreviewClearColor{};
std::atomic<void *> PreviewController{};
std::atomic<bool> PreviewFixedPose{}, PreviewRequestedFixedPose{}, PreviewScenePose{};
std::atomic<unsigned> PreviewHandle{}, PreviewAngleZ{}, MatrixCalls{}, MatrixMatches{}, BackgroundApplied{};
std::atomic<std::uint64_t> PreviewAngleXY{};
std::atomic<std::uint64_t> PreviewOriginXY{}, PreviewItem{};
std::atomic<unsigned> PreviewOriginZ{}, PreviewPoseEpoch{};
std::array<CarrierPose, 4> CarrierPoses{};
std::array<std::atomic<unsigned>, 4> PreviewCarriers{};
std::array<LayerDrawScope, 8> LayerDrawScopes{};
std::atomic<unsigned> TransformCalls{}, TransformAttempts{}, TransformApplied{}, ClearCalls{}, ClearMatches{};
std::atomic<void *> WorkbenchController{};
std::atomic<unsigned> WorkbenchHandle{}, WorkbenchTiltAngle{}, WorkbenchMode{};
std::atomic<unsigned> WorkbenchAppliedReports{};
std::atomic<bool> WorkbenchTiltEnabled{};
std::array<WorkbenchCarrierPose, 4> WorkbenchPoses{};
std::uintptr_t Main{};
struct WorkbenchInputState WorkbenchInputState;
void *UiContext() {
    auto storage = game::preview_native::UiContext(Main);
    return storage ? At<void *>(storage) : nullptr;
}
const void *ItemLayerClearColor() {
    auto storage = game::preview_native::RenderLayers(Main);
    auto manager = storage ? At<void *>(storage) : nullptr;
    if (!manager) return nullptr;
    const auto count = At<unsigned>(manager, field::LayerCount);
    auto layers = At<void **>(manager, field::Layers);
    if (count <= ItemRenderLayer || count > 64 || !layers || !layers[ItemRenderLayer]) return nullptr;
    return static_cast<std::byte *>(layers[ItemRenderLayer]) + field::LayerClearColor;
}
PublicationClock PosePublication, WorkbenchPublication;
namespace {
void StorePose(void *controller) {
    const auto angles = static_cast<std::byte *>(controller) + field::ModelAngles;
    PreviewAngleZ.store(At<unsigned>(angles, 8));
    PreviewAngleXY.store(At<std::uint64_t>(angles));
    PreviewHandle.store(At<unsigned>(controller, field::ModelHandle));
}
}
void PublishPose(void *controller) {
    PublicationClock::Write publish{PosePublication};
    StorePose(controller);
}
void ResetPoseOrigin(void *controller, std::uint64_t item, bool requestedFixed) {
    PublicationClock::Write publish{PosePublication};
    PreviewOriginXY.store(At<std::uint64_t>(controller, field::ModelAngles));
    PreviewOriginZ.store(At<unsigned>(controller, field::ModelAngles + 8));
    PreviewItem.store(item);
    PreviewPoseEpoch.fetch_add(1);
    PreviewRequestedFixedPose.store(requestedFixed);
    StorePose(controller);
}
void ClearPublishedPose() {
    PublicationClock::Write publish{PosePublication};
    PreviewHandle.store(0);
}
std::optional<PublishedPose> ReadPublishedPose() {
    return PosePublication.Read([] {
        return PublishedPose{PreviewHandle.load(), PreviewAngleZ.load(), PreviewOriginZ.load(), PreviewPoseEpoch.load(),
                             PreviewAngleXY.load(), PreviewOriginXY.load(), PreviewItem.load(), PreviewRequestedFixedPose.load()};
    });
}
std::optional<WorkbenchPoseSnapshot> ReadWorkbenchPose() {
    return WorkbenchPublication.Read([] {
        return WorkbenchPoseSnapshot{WorkbenchController.load(), WorkbenchHandle.load(), WorkbenchMode.load(), WorkbenchTiltAngle.load()};
    });
}
void ReportRendererTrace() {
    Logging.Debug("[QoL Enhancements/Crafting] DIY preview: render trace; native_updates=%u pending_attempts=%u applied=%u layer_draws=%u matched_layers=%u matrices=%u matched_matrices=%u backgrounds=%u\n",
                TransformCalls.load(std::memory_order_relaxed), TransformAttempts.load(std::memory_order_relaxed),
                TransformApplied.load(std::memory_order_relaxed), ClearCalls.load(std::memory_order_relaxed),
                ClearMatches.load(std::memory_order_relaxed), MatrixCalls.load(std::memory_order_relaxed),
                MatrixMatches.load(std::memory_order_relaxed), BackgroundApplied.load(std::memory_order_relaxed));
}
void DrawPreviewLayer::Callback(void *renderer, void *command, void *context, const void *options){
        const auto published = PreviewClearColor.load(std::memory_order_acquire);
        const auto layer = context ? At<void *>(context, field::DrawLayer) : nullptr;
        const bool matches = published && layer &&
            static_cast<std::byte *>(layer) + field::LayerClearColor == published;
        if (published) ClearCalls.fetch_add(1, std::memory_order_relaxed);
        LayerDrawScope *scope = nullptr;
        if (matches && command) for (auto &entry : LayerDrawScopes) {
            void *empty = nullptr;
            if (!entry.command.compare_exchange_strong(empty, reinterpret_cast<void *>(1), std::memory_order_acquire)) continue;
            entry.renderer.store(At<void *>(renderer, field::RendererState), std::memory_order_relaxed);
            auto info = At<void *>(context, field::DrawInfo);
            entry.info.store(info ? static_cast<std::byte *>(info) + 0x60 : nullptr, std::memory_order_relaxed);
            entry.view.store(At<unsigned char>(context, field::DrawView), std::memory_order_relaxed);
            entry.command.store(command, std::memory_order_release);
            scope = &entry; break;
        }
        if (matches) ClearMatches.fetch_add(1, std::memory_order_relaxed);
        Orig(renderer, command, context, options); // Preserve native draw/clear flags.
        if (scope) scope->command.store(nullptr, std::memory_order_release);
    }

void DrawPreviewContents::Callback(void *renderer, void *command, unsigned view, unsigned flags,
                         const void *color, void *info){
        for (auto &entry : LayerDrawScopes) {
            if (entry.command.load(std::memory_order_acquire) != command || !command ||
                entry.renderer.load(std::memory_order_relaxed) != renderer ||
                entry.info.load(std::memory_order_relaxed) != info ||
                entry.view.load(std::memory_order_relaxed) != view) continue;
            if (BackgroundApplied.fetch_add(1, std::memory_order_relaxed) == 0)
                Logging.Debug("[QoL Enhancements/Crafting] DIY preview: HDR background applied for #E5DFC7 target\n");
            color = PreviewBackground.data(); break;
        }
        Orig(renderer, command, view, flags, color, info);
    }

ModelMatrix BuildPreviewMatrix::Callback(void *component){
        // Native 0192A590 derives carrier angles/framing from item preview
        // parameters (including mRemakeFence). Preserve those matrices for
        // flat surfaces/fences instead of replacing them with controller XYZ.
        // Once the scene owns the pose, render that exact pose too. Using a
        // newer UI angle here would put the mesh one update ahead of its bones.
        if (!component || PreviewFixedPose.load(std::memory_order_acquire) ||
            PreviewScenePose.load(std::memory_order_acquire)) return Orig(component);
        const auto published = ReadPublishedPose();
        if (!published || !published->handle) return Orig(component);
        const auto handle = published->handle;
        MatrixCalls.fetch_add(1, std::memory_order_relaxed);
        auto owner = At<void *>(component, field::ComponentOwner);
        if (!owner) return Orig(component);
        // The root already receives the controller pose from native code.
        if (At<unsigned>(owner, field::EntityHandle) == handle) return Orig(component);
        const auto parent = At<unsigned>(owner, field::ParentHandle);
        bool matches = At<unsigned>(owner, field::EntityHandle) == handle || parent == handle;
        if (!matches && parent && At<unsigned>(owner, field::EntityKind) == ItemDrawableKind)
            for (auto &carrier : PreviewCarriers)
                matches |= parent == carrier.load(std::memory_order_acquire);
        if (!matches) return Orig(component);
        // 0192A1F0 -> type 6 -> 019362F4 -> type 6F. Its 019282FC
        // transform copies the carrier, not the controller actor. Retain its
        // native starting pose and add only user movement until scene capture.
        alignas(16) std::array<std::byte, field::ComponentPoseSize> pose{};
        std::memcpy(pose.data(), component, pose.size());
        const auto delta = published->Delta();
        for (unsigned axis = 0; axis < delta.size(); ++axis)
            At<unsigned>(pose.data(), field::ComponentAngles + axis * sizeof(unsigned)) += delta[axis];
        if (PreviewHandle.load(std::memory_order_acquire) != handle) return Orig(component);
        if (MatrixMatches.fetch_add(1, std::memory_order_relaxed) == 0)
            Logging.Debug("[QoL Enhancements/Crafting] DIY preview: drawable matrix matched; entity=%u parent=%u\n",
                        At<unsigned>(owner, field::EntityHandle), At<unsigned>(owner, field::ParentHandle));
        return Orig(pose.data());
    }

bool SetPreviewRotation::Callback(void *controller, const void *angles){
        // 032B9510 can reapply the native parameter animation through this
        // setter AFTER a queued transform succeeds. Keep the preview pose at
        // that final boundary too; other controllers retain the supplied pose.
        if (controller && !PreviewFixedPose.load(std::memory_order_acquire) &&
            controller == PreviewController.load(std::memory_order_acquire) &&
            At<unsigned>(controller, field::ModelMode) == DiyModelMode &&
            At<int>(controller, field::ModelState) == 1 &&
            At<unsigned>(controller, field::ModelHandle))
            angles = static_cast<std::byte *>(controller) + field::ModelAngles;
        return Orig(controller, angles); // Preserve native entity/phase/handle validation.
    }

void TraceModelTransform::Callback(void *controller){
        const bool owned = controller && controller == PreviewController.load(std::memory_order_acquire);
        const auto before = owned ? At<unsigned char>(controller, field::ModelFlags) : 0;
        if (owned) TransformCalls.fetch_add(1, std::memory_order_relaxed);
        Orig(controller); // Observe native scheduling; never force an entity phase or retry.
        if (!owned) return;
        PublishCarriers(controller); // Child allocation can complete after model creation.
        if (!(before & TransformPending)) return;
        const auto after = At<unsigned char>(controller, field::ModelFlags);
        if (!(after & TransformPending)) TransformApplied.fetch_add(1, std::memory_order_relaxed);
        if (TransformAttempts.fetch_add(1, std::memory_order_relaxed) >= 8) return;
        auto storage = game::preview_native::EntityManager(Main);
        auto manager = storage ? At<void *>(storage) : nullptr;
        Logging.Debug("[QoL Enhancements/Crafting] DIY preview: native transform; flags=%u->%u ready=%u entity_phase=%d entity_stage=%u\n",
                    unsigned(before), unsigned(after), At<unsigned char>(controller, field::ModelReady),
                    manager ? At<int>(manager, field::EntityPhase) : -99,
                    manager ? At<unsigned>(manager, field::EntityStage) : 0xffffffffu);
    }

}
