#pragma once
#include "../game/recipe_preview_operations.hpp"
namespace qol::crafting::preview {
enum class Phase { Idle, Queued, Starting, Showing, Closing, DrainBack };
using ModelProgress = game::ModelProgress;
using NavigationProgress = game::NavigationProgress;
// Mod-owned UI policy; native objects and renderer leases stay in adapters.
class Session {
    struct Navigation {
        int sample{}, held{}, pending{};
        unsigned wait{};
        int Step();
    };
    static constexpr float MaximumTiltDegrees = 60.0f;
    Phase phase_ = Phase::Idle;
    unsigned frames_{};
    Navigation navigation_;
    float rotationX_{}, rotationY_{}, tilt_{};
public:
    struct RotationInput { float x{}, y{}; };
    Phase State() const;
    void Open();
    void RequestClose();
    void FinishClose();
    void ReleaseBack();
    void Navigate(int direction);
    void Rotate(float x, float y);
    void ClearFrameInput();
    void CancelPendingInput();
    void ResetNavigation();
    void CancelRotationInput();
    RotationInput TakeRotationInput();
    float TiltBy(float degrees);
    void ResetItemRotation();
    void Frame(game::PreviewRuntime &);
};
}
namespace qol::crafting::features {
float PreviewStickAxis(float);
float TiltWorkbench(float previous, float stickY);
bool ShowRecipePreviewHint(bool categoryFocus, unsigned count, unsigned selected, preview::Phase);
bool QueueRecipePreview(preview::Session &, game::RecipeOpening &, std::uint64_t &copiedItem);
bool HandleRecipePreviewInput(preview::Session &, bool &confirmHeld, bool &backHeld, game::PreviewInput &);
preview::NavigationProgress NavigateRecipePreview(preview::Session &, game::RecipeNavigation &, int direction);
void RotateRecipePreview(preview::Session &, game::PreviewRotation &);
void DeactivateRecipePreview(preview::Session &, bool &confirmHeld, bool &backHeld, game::PreviewRuntime &);
}
