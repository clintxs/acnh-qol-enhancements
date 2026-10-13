#pragma once
#include <cstdint>
namespace qol::crafting::game {
enum class ModelProgress { Waiting, Created, Failed };
enum class NavigationProgress { Retry, Moved, End };
struct Stick { float x{}, y{}; };
// All operations are synchronous borrows. Begin acquires the supported native
// renderer lease; model creation/replacement may remain pending in the engine.
// No runtime/context object is retained or deleted through these interfaces.
class PreviewRuntime {
public:
    virtual void RefreshHints() = 0;
    virtual bool AcquireRenderer() = 0;
    virtual void OpenDeclined() = 0;
    virtual void ReleaseRenderer() = 0;
    virtual ModelProgress CreateItemModel() = 0;
    virtual void ShowPreview() = 0;
    virtual NavigationProgress Navigate(int direction) = 0;
    virtual void PlaceBanner() = 0;
    virtual void Rotate() = 0;
    virtual void ReportShowing() = 0;
protected: ~PreviewRuntime() = default;
};
// List rows/items are queried only after the feature validates count/index
// and list presence; CopyItem returns an owned full eight-byte value.
class RecipeOpening {
public:
    virtual unsigned Count() = 0;
    virtual unsigned Selected() = 0;
    virtual bool FocusedRecipeRow() = 0;
    virtual bool PhoneReady() = 0;
    virtual bool HasRecipeList() = 0;
    virtual bool HasRecipe(unsigned) = 0;
    virtual bool HasValidItem(unsigned) = 0;
    virtual bool HasSurface() = 0;
    virtual bool HasGrid() = 0;
    virtual std::uint64_t CopyItem(unsigned) = 0;
    virtual void ReportDeclined(bool row, unsigned selected, unsigned count) = 0;
    virtual void ReportQueued(unsigned selected) = 0;
protected: ~RecipeOpening() = default;
};
// Held buttons come from the original sample; consumption queries observe the
// current mask, preserving repeated-dispatch and per-axis ownership behavior.
class PreviewInput {
public:
    virtual void RefreshInputRow() = 0;
    virtual bool ValidSnapshot() = 0;
    virtual bool ConfirmHeld() = 0;
    virtual bool BackHeld() = 0;
    virtual bool ConfirmConsumed() = 0;
    virtual bool BackConsumed() = 0;
    virtual bool PageBlocked() = 0;
    virtual bool DispatchBlocked() = 0;
    virtual void ReportConfirm() = 0;
    virtual bool QueueSelectedPreview() = 0;
    virtual void ConsumeConfirm() = 0;
    virtual void ConsumeBack() = 0;
    virtual bool LeftUnconsumed() = 0;
    virtual bool RightUnconsumed() = 0;
    virtual bool LeftStickConsumed() = 0;
    virtual bool RightStickConsumed() = 0;
    virtual Stick LeftStick() = 0;
    virtual Stick RightStick() = 0;
    virtual void ConsumeNavigation() = 0;
    virtual void ConsumeRotation() = 0;
    virtual void ReportRotationSample(float x, float y, bool blocked) = 0;
protected: ~PreviewInput() = default;
};
class RecipeNavigation {
public:
    virtual bool RendererReady() = 0;
    virtual unsigned Count() = 0;
    virtual unsigned Selected() = 0;
    virtual bool HasRecipeList() = 0;
    virtual bool HasValidItem(unsigned) = 0;
    virtual std::uint64_t CopyItem(unsigned) = 0;
    virtual void RestoreBanner() = 0;
    // Native grid reset/activate/focus, including material panel updates. False
    // is retryable; it does not promise the engine made no intermediate change.
    virtual bool SelectRecipe(unsigned selected, unsigned count) = 0;
    virtual void SetItem(std::uint64_t) = 0;
    virtual void ReplaceModel(unsigned selected, unsigned count) = 0;
protected: ~RecipeNavigation() = default;
};
class PreviewRotation {
public:
    virtual bool FixedPose() = 0;
    virtual bool RendererReady() = 0;
    virtual void QueueRotation(float yawDelta, float tilt) = 0;
protected: ~PreviewRotation() = default;
};
}
