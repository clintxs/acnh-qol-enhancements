#pragma once
#include "lib.hpp"
#include "preview_native.hpp"
#include "preview_publication.hpp"
#include "program/crafting/recipe_preview_layout.hpp"
#include "program/crafting/recipe_preview_banner.hpp"
#include "program/crafting/recipe_preview_hint.hpp"
#include "program/crafting/services/preview_session.hpp"
#include "program/core/page_input.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
namespace qol::crafting::preview_internal {
namespace profile = game::v303::preview;
namespace address = profile::address;
namespace field = profile::field;
namespace itemTable = profile::itemTable;
using profile::ModelMatrix;
using profile::GridRequest;
using profile::ItemRequest;
using game::preview_native::At;
constexpr unsigned DiyModelMode = 2;
// CreateItemModel accepts these profiles (032BD990); the current profile is
// owned by native UI scheduling. A background-page index is a separate enum.
constexpr unsigned ItemModelProfiles = 0x32decu;
inline bool IsItemModelProfile(unsigned mode) { return mode < 18 && (ItemModelProfiles & (1u << mode)); }
constexpr unsigned ConfirmMask = 0x10, BackMask = 0x20, PhoneShortcutKind = 5;
// Native 03C49700/03C49710 route kinds 2/3 to X, kinds 0/1 to Y.
// These are logical UI button bits, including synthesized stick directions.
constexpr unsigned LeftMask = 4, RightMask = 8, LeftStickMask = 1, RightStickMask = 2;
constexpr unsigned NavigationDelay = 12, NavigationRepeat = 4; // UI updates, normally 30 Hz.
constexpr float NavigationThreshold = 0.5f;
constexpr unsigned TransformPending = 1, MaterialPending = 6;
constexpr float StickDeadZone = 0.15f, DegreesPerUpdate = 6.0f, MaximumTilt = 60.0f;
// Native 032B74F0 converts radians to Angle32 (one turn = 2^32).
constexpr float AngleUnitsPerDegree = 4294967296.0f / 360.0f;
constexpr int PhoneModelMode = 26, PhoneModelState = 3;
constexpr unsigned ItemRenderLayer = 2;
constexpr unsigned PreviewActorKind = 0x42, ItemDrawableKind = 0x6f;
// Scene clear colors pass through the 3D color pipeline before UI capture.
// An sRGB-normalized E5DFC7 clear measured CCC9C0 in the 0.3.42 screenshot.
// These HDR values fit an inverse filmic curve to that observation. This is
// an approximate display-color correction, pending live visual validation.
constexpr std::array<float, 4> PreviewBackground{1.5950871f, 1.3879482f, 0.8751216f, 1.0f};
// Layer drawing runs on a render worker. Publish only the layer identity;
// that worker must not inspect the UI thread's mutable Session objects.
extern std::atomic<const void *> PreviewClearColor;
extern std::atomic<void *> PreviewController;
extern std::atomic<bool> PreviewFixedPose, PreviewRequestedFixedPose, PreviewScenePose;
extern std::atomic<unsigned> PreviewHandle, PreviewAngleZ, MatrixCalls, MatrixMatches, BackgroundApplied;
extern std::atomic<std::uint64_t> PreviewAngleXY;
extern std::atomic<std::uint64_t> PreviewOriginXY, PreviewItem;
extern std::atomic<unsigned> PreviewOriginZ, PreviewPoseEpoch;

// Only the owned actor's update accesses these caches. UI changes publish an
// epoch, rather than resetting scene-thread storage while it may be in use.
struct CarrierPose {
    unsigned epoch{}, handle{};
    void *component{};
    std::array<unsigned, 3> angles{}, displayed{};
};
extern std::array<CarrierPose, 4> CarrierPoses;
// Type 42 creates up to four type-6 item carriers; each carrier creates the
// type-6F drawable. Publish full generation-bearing handles on the native
// update thread instead of traversing the entity registry on a render worker.
extern std::array<std::atomic<unsigned>, 4> PreviewCarriers;
// A command buffer is serially recorded. Scope the downstream color override
// to this layer's synchronous call, using atomic slots for parallel workers.
struct LayerDrawScope {
    std::atomic<void *> command{}, renderer{}, info{};
    std::atomic<unsigned> view{};
};
extern std::array<LayerDrawScope, 8> LayerDrawScopes;
extern std::atomic<unsigned> TransformCalls, TransformAttempts, TransformApplied, ClearCalls, ClearMatches;

extern std::atomic<void *> WorkbenchController;
extern std::atomic<unsigned> WorkbenchHandle, WorkbenchTiltAngle, WorkbenchMode;
extern std::atomic<unsigned> WorkbenchAppliedReports;
extern std::atomic<bool> WorkbenchTiltEnabled;
// Input state belongs to the UI dispatcher; carrier state belongs to the scene.
struct WorkbenchInputState {
    void *controller{}, *page{};
    unsigned handle{}, mode{};
    std::uint64_t item{};
    float tilt{};
    unsigned rotationReports{};
};
extern struct WorkbenchInputState WorkbenchInputState;
struct WorkbenchCarrierPose {
    void *component{};
    unsigned handle{};
    std::array<unsigned, 3> primary{}, secondary{}, applied{};
};
extern std::array<WorkbenchCarrierPose, 4> WorkbenchPoses;
extern std::uintptr_t Main;

bool UsesFixedPreviewPose(std::uint16_t);
void *UiContext();
void *Find(void *, const char *);
void *PhoneRoot(void *);
void Visible(void *, bool);
const void *ItemLayerClearColor();
void PublishPose(void *);
extern PublicationClock PosePublication, WorkbenchPublication;
void ResetPoseOrigin(void *, std::uint64_t, bool requestedFixed);
void ClearPublishedPose();
std::optional<PublishedPose> ReadPublishedPose();
std::optional<WorkbenchPoseSnapshot> ReadWorkbenchPose();
std::array<unsigned,3> NativePoseAngles(void *);
void PublishCarriers(void *);
std::optional<WorkbenchPoseSnapshot> WorkbenchActor(void *);
void TraceWorkbenchModel(void *);
void *PreviewCarrier(void *, unsigned);
void RestoreWorkbenchPose(void *);
void HoldWorkbenchPose(void *);
void ApplyWorkbenchTilt(void *);
void ReportRendererTrace();
bool InstallPhonePreview();
bool InstallWorkbenchTilt();
HOOK_DEFINE_TRAMPOLINE(DrawPreviewLayer) { static void Callback(void *renderer, void *command, void *context, const void *options); };
HOOK_DEFINE_TRAMPOLINE(DrawPreviewContents) { static void Callback(void *renderer, void *command, unsigned view, unsigned flags,
                         const void *color, void *info); };
HOOK_DEFINE_TRAMPOLINE(UpdatePreviewActor) { static void Callback(void *actor); };
HOOK_DEFINE_TRAMPOLINE(BuildPreviewMatrix) { static ModelMatrix Callback(void *component); };
HOOK_DEFINE_TRAMPOLINE(SetPreviewRotation) { static bool Callback(void *controller, const void *angles); };
HOOK_DEFINE_TRAMPOLINE(TraceModelTransform) { static void Callback(void *controller); };
HOOK_DEFINE_TRAMPOLINE(BuildRecipeLayout) { static bool Callback(void *layout, void *result, void *device, void *accessor,
                         void *factory, void *parts, const void *resource, void *options, bool check); };
HOOK_DEFINE_TRAMPOLINE(BuildPhoneRecipeHints) { static void Callback(void *phone, unsigned kind); };
HOOK_DEFINE_TRAMPOLINE(BindPhoneRecipes) { static void Callback(void *phone); };
HOOK_DEFINE_TRAMPOLINE(UpdatePhoneRecipes) { static void Callback(void *phone); };
HOOK_DEFINE_TRAMPOLINE(DeactivatePhoneRecipes) { static void Callback(void *phone); };
} // namespace qol::crafting::preview_internal
