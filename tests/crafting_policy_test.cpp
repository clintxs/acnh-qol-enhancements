#include "program/crafting/features/crafting.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using namespace qol::crafting;
using namespace qol::crafting::features;
struct List : game::QuantityList {
    std::uint64_t now{};
    std::vector<std::string> events;
    std::uint64_t Tick() override { events.push_back("tick"); return now; }
    void StepAcrossBoundary(bool up) override { events.push_back(up ? "up" : "down"); }
    void NeutralizeVerticalInput() override { events.push_back("neutral"); }
};
struct Shortcut : game::QuantityShortcut {
    bool ready = true, y{}, x{}, consumedY{}, consumedX{}, competing{}, result{}, favorite = true;
    bool multiple = true, transition = true, child = true;
    std::vector<std::string> events;
    bool InputReady() override { return ready; }
    bool QuantityHeld() override { return y; }
    bool FavoriteHeld() override { return x; }
    bool QuantityConsumed() override { return consumedY; }
    bool FavoriteConsumed() override { return consumedX; }
    bool ConfirmOrBackHeld() override { return competing; }
    bool HasResult() override { return result; }
    bool FavoriteAvailable() override { return favorite; }
    void RequestFavorite() override { events.push_back("favorite"); }
    void ConsumeQuantity() override { if (y) events.push_back("consume Y"); }
    void ConsumeFavorite() override { events.push_back("consume X"); }
    bool SupportsQuantity() override { events.push_back("eligible"); return multiple; }
    bool TransitionAvailable() override { return transition; }
    bool QuantityChildReady() override { events.push_back("child"); return child; }
    void RequestQuantitySelector() override { events.push_back("quantity"); }
    void ReportQuantityOpened() override {}
};
struct Opening : game::RecipeOpening {
    unsigned count = 2, selected{};
    bool row = true, ready = true, list = true, recipe = true, item = true, surface = true, grid = true;
    unsigned reads{}, queued{}, declined{};
    unsigned Count() override { return count; }
    unsigned Selected() override { return selected; }
    bool FocusedRecipeRow() override { return row; }
    bool PhoneReady() override { return ready; }
    bool HasRecipeList() override { assert(ready && row && selected < count && count <= 4096); return list; }
    bool HasRecipe(unsigned index) override { assert(list && index == selected); return recipe; }
    bool HasValidItem(unsigned index) override { assert(recipe && index == selected); return item; }
    bool HasSurface() override { return surface; }
    bool HasGrid() override { return grid; }
    std::uint64_t CopyItem(unsigned) override { ++reads; return 0x1122334455660088ull; }
    void ReportDeclined(bool, unsigned, unsigned) override { ++declined; }
    void ReportQueued(unsigned) override { ++queued; }
};
struct Navigation : game::RecipeNavigation {
    bool ready = true, list = true, validItem = true, select = true;
    unsigned count = 2, selected{};
    std::vector<std::string> events;
    bool RendererReady() override { events.push_back("ready"); return ready; }
    unsigned Count() override { assert(ready); return count; }
    unsigned Selected() override { return selected; }
    bool HasRecipeList() override { assert(count && count <= 4096 && selected < count); return list; }
    bool HasValidItem(unsigned index) override { assert(list && index < count); return validItem; }
    std::uint64_t CopyItem(unsigned) override { events.push_back("copy"); return 0x1122334455660088ull; }
    void RestoreBanner() override { events.push_back("banner"); }
    bool SelectRecipe(unsigned index, unsigned total) override { assert(index == selected+1 && total == count); events.push_back("select"); return select; }
    void SetItem(std::uint64_t item) override { assert(item == 0x1122334455660088ull); events.push_back("item"); }
    void ReplaceModel(unsigned, unsigned) override { events.push_back("replace"); }
};
struct Rotation : game::PreviewRotation {
    bool fixed{}, ready = true;
    unsigned checked{}, queued{}; float yaw{}, tilt{};
    bool FixedPose() override { return fixed; }
    bool RendererReady() override { ++checked; return ready; }
    void QueueRotation(float x, float y) override { ++queued; yaw = x; tilt = y; }
};
struct Runtime : game::PreviewRuntime {
    void RefreshHints() override {}
    bool AcquireRenderer() override { return true; }
    void OpenDeclined() override { assert(false); }
    void ReleaseRenderer() override {}
    game::ModelProgress CreateItemModel() override { return game::ModelProgress::Created; }
    void ShowPreview() override {}
    game::NavigationProgress Navigate(int) override { return game::NavigationProgress::Moved; }
    void PlaceBanner() override {}
    void Rotate() override {}
    void ReportShowing() override {}
};
int main() {
    for (unsigned maximum = 1; maximum <= 30; ++maximum) {
        assert(CraftingRate(0,maximum) == 1.f);
        assert(CraftingRate(60,maximum) == float(maximum));
        assert(CraftingRate(0xffffffff,maximum) == float(maximum));
        for (unsigned frame = 1; frame <= 60; ++frame) {
            assert(CraftingRate(frame,maximum) >= CraftingRate(frame-1,maximum));
            const auto expected = 1. + (maximum-1.) * std::log1p(9.*frame/60.) / std::log(10.);
            assert(std::abs(CraftingRate(frame,maximum)-expected) < maximum*2e-7);
        }
    }
    List list; QuantityHold hold; QuantityInput input;
    input.count = 10; input.selected = 9; input.cyclicLayoutReady = true; input.buttonDown = true;
    assert(HandleQuantityEndpoints(hold,input,list) == QuantityMotion::Native); // Held on open cannot wrap.
    assert((list.events == std::vector<std::string>{"neutral"}));
    list.events.clear(); input.buttonDown = false;
    HandleQuantityEndpoints(hold,input,list); assert(hold.armed);
    input.buttonDown = true; list.now = 100;
    assert(HandleQuantityEndpoints(hold,input,list) == QuantityMotion::WrappedDown);
    assert((list.events == std::vector<std::string>{"down","tick","neutral"}));
    assert(hold.wrapPaused && !hold.armed && hold.pauseStarted == 100);
    list.events.clear(); list.now += 5'760'000 - 1;
    assert(HandleQuantityEndpoints(hold,input,list) == QuantityMotion::Native);
    assert((list.events == std::vector<std::string>{"tick","neutral"}));
    list.events.clear(); ++list.now;
    HandleQuantityEndpoints(hold,input,list); assert(!hold.wrapPaused && !hold.armed);
    assert((list.events == std::vector<std::string>{"tick"}));
    list.events.clear(); HandleQuantityEndpoints(hold,input,list);
    assert((list.events == std::vector<std::string>{"neutral"})); // No repeated endpoint wrap.
    input.buttonDown = false; input.x = .2f;
    HandleQuantityEndpoints(hold,input,list); assert(!hold.armed); // Smaller release zone.
    input.x = 0; HandleQuantityEndpoints(hold,input,list); assert(hold.armed);
    input.buttonUp = true; input.selected = 0; list.events.clear();
    assert(HandleQuantityEndpoints(hold,input,list) == QuantityMotion::WrappedUp);
    assert(list.events.front() == "up");
    CancelQuantityHold(hold); assert(!hold.armed && !hold.wrapPaused && hold.pauseStarted == list.now);
    ResetQuantityHold(hold); assert(hold.pauseStarted == 0);
    for (bool stickConsumed : {false,true}) {
        hold = {true,false,0}; list.events.clear(); input = {};
        input.count = 10; input.selected = 0; input.cyclicLayoutReady = true;
        input.y = 1; input.stickConsumed = stickConsumed;
        assert((HandleQuantityEndpoints(hold,input,list) == QuantityMotion::WrappedUp) == !stickConsumed);
    }
    hold = {true,false,0}; list.events.clear(); input.y = std::numeric_limits<float>::quiet_NaN();
    HandleQuantityEndpoints(hold,input,list); assert(!hold.armed);
    Shortcut shortcut; ShortcutHold buttons;
    HandleQuantityShortcut(buttons,shortcut); assert(buttons.armed && buttons.favoriteArmed);
    shortcut.y = true;
    assert(HandleQuantityShortcut(buttons,shortcut));
    assert((shortcut.events == std::vector<std::string>{"consume Y","eligible","child","quantity","consume Y"}));
    shortcut.events.clear(); assert(!HandleQuantityShortcut(buttons,shortcut));
    assert((shortcut.events == std::vector<std::string>{"consume Y"}));
    shortcut = {}; buttons = {true,true}; shortcut.y = true; shortcut.x = true; shortcut.multiple = false;
    assert(!HandleQuantityShortcut(buttons,shortcut));
    assert((shortcut.events == std::vector<std::string>{"consume Y","eligible"})); // Y cannot favorite.
    shortcut = {}; buttons = {true,true}; shortcut.x = true;
    assert(!HandleQuantityShortcut(buttons,shortcut));
    assert((shortcut.events == std::vector<std::string>{"favorite","consume X"}));
    shortcut = {}; shortcut.ready = false; buttons = {true,true};
    assert(!HandleQuantityShortcut(buttons,shortcut) && !buttons.armed && !buttons.favoriteArmed);
    Opening opening; preview::Session session; std::uint64_t copied{};
    for (unsigned invalid : {0u,4097u}) {
        opening.count = invalid;
        assert(!QueueRecipePreview(session,opening,copied));
        assert(session.State() == preview::Phase::Idle && !opening.reads);
    }
    opening = {}; opening.surface = false;
    assert(!QueueRecipePreview(session,opening,copied) && !opening.reads);
    opening = {}; assert(QueueRecipePreview(session,opening,copied));
    assert(copied == 0x1122334455660088ull && opening.queued == 1 && session.State() == preview::Phase::Queued);
    Runtime runtime; session.Frame(runtime); session.Frame(runtime);
    assert(session.State() == preview::Phase::Showing);
    session.TiltBy(12); Navigation recipes; recipes.ready = false;
    assert(NavigateRecipePreview(session,recipes,1) == game::NavigationProgress::Retry);
    assert((recipes.events == std::vector<std::string>{"ready"}));
    recipes = {}; assert(NavigateRecipePreview(session,recipes,-1) == game::NavigationProgress::End);
    assert((recipes.events == std::vector<std::string>{"ready"}));
    recipes = {}; recipes.select = false;
    assert(NavigateRecipePreview(session,recipes,1) == game::NavigationProgress::Retry);
    assert(session.TiltBy(0) == 12);
    recipes = {}; assert(NavigateRecipePreview(session,recipes,1) == game::NavigationProgress::Moved);
    assert(session.TiltBy(0) == 0);
    assert((recipes.events == std::vector<std::string>{"ready","copy","banner","select","item","replace"}));
    Rotation renderer; renderer.fixed = true; session.Rotate(1,1);
    RotateRecipePreview(session,renderer); assert(!renderer.checked && !renderer.queued && !session.TakeRotationInput().x);
    renderer.fixed = false; renderer.ready = false; session.Rotate(1,1);
    RotateRecipePreview(session,renderer); assert(renderer.checked == 1 && !renderer.queued && !session.TakeRotationInput().x);
    renderer.ready = true; session.Rotate(1,-1); RotateRecipePreview(session,renderer);
    assert(renderer.queued == 1 && renderer.yaw == 6 && renderer.tilt == 6);
    RotateRecipePreview(session,renderer); assert(renderer.queued == 1); // Drained once.
    assert(PreviewStickAxis(std::numeric_limits<float>::infinity()) == 0);
    assert(TiltWorkbench(59,-1) == 60 && TiltWorkbench(-59,1) == -60);
    std::cout << "PASS: Crafting speed, endpoint timing/native step order, shortcut priority/consumption, lazy selection bounds, navigation retry and rotation ownership\n";
}
