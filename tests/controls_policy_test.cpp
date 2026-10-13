#include "program/controls/features/controls.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace qol::controls;
using namespace qol::controls::features;
using game::Crossing;
using game::TraversalKind;
struct Check { bool transferred{}, landing{}; bool Valid() const { return transferred || landing; } };
struct Selection { unsigned long long item{}; int slot = -1; };
struct Borrow { BorrowStage stage = BorrowStage::None; bool equipEntered{}, equipFinished{}, traversalEntered{}; };
struct Equipment {
    std::vector<std::string> events;
    bool owns = true, intent = true, valid = true, accepts = true, sameItem = true;
    bool CanBeginBorrow() const { return true; }
    Selection EmptySelection() const { return {}; }
    Selection FindEquippedItem() { events.push_back("find equipped"); return {42, 1}; }
    void RecordBorrow(Selection, Selection) { events.push_back("record"); }
    bool RequestToolChange(Selection s) { events.push_back("change " + std::to_string(s.slot)); return accepts; }
    void ClearBorrow() { events.push_back("clear"); }
    void TraceBorrow() { events.push_back("borrow"); }
    void TraceRestore(Selection) { events.push_back("restore"); }
    bool SameItemAtSlot(Selection) const { return sameItem; }
    Selection FindExactItem(unsigned long long) { events.push_back("find exact"); return {}; }
    bool StillOwnsBorrowedTool() const { return owns; }
    void RestorePreviousTool() { events.push_back("restore"); }
    void ReleaseOwnership() { events.push_back("release"); }
    struct Vector { float x, y; };
    Vector Stick() const { return {intent ? 1.f : 0.f, 0.f}; }
    bool FacingMatchesBorrow() const { return true; }
    bool AtBorrowedBank() const { return true; }
    Check CheckLanding() { events.push_back("check"); return {false, valid}; }
    bool RequestTraversal() { events.push_back("traverse"); return accepts; }
    void TraceCancellation(bool, Check) {}
    void TraceTraversal() {}
};
struct Traversal {
    std::vector<std::string> events;
    bool intent = true, walls{}, active{}, complete = true, placed{}, ladder{}, pole = true;
    TraversalKind held = TraversalKind::None;
    Crossing hop = Crossing::None;
    struct Proof { TraversalKind tool = TraversalKind::Pole; bool valid{}; };
    Proof ConsumeMovementProof() { events.push_back("consume"); return {}; }
    bool ToolBorrowActive() const { return active; }
    Crossing OrdinaryHop() { events.push_back("ordinary"); return hop; }
    Crossing SearchNativeHop(bool wall) { events.push_back(wall ? "wall hop" : "hop"); walls = wall; return hop; }
    bool IntentObserved() const { return intent; }
    bool IncludesWalls() const { return walls; }
    TraversalKind HeldTool() const { return held; }
    TraversalKind ProvenTool(Proof p) const { return p.tool; }
    Selection EmptySelection() const { return {}; }
    Check EmptyCheck() const { return {}; }
    Check TransferProof(Proof p, TraversalKind) { events.push_back("proof"); return {p.valid, false}; }
    Check CheckLanding(TraversalKind t) {
        const auto name = t == TraversalKind::PlacedLadder ? "placed" : t == TraversalKind::Ladder ? "ladder" : "pole";
        events.push_back(std::string("check ") + name);
        return {false, t == TraversalKind::PlacedLadder ? placed : t == TraversalKind::Ladder ? ladder : pole};
    }
    bool HoldComplete() const { return complete; }
    void RememberBank(TraversalKind) { events.push_back("remember"); }
    Selection FindTool(TraversalKind t) { events.push_back(t == TraversalKind::Ladder ? "find ladder" : "find pole"); return {1, 1}; }
    bool BorrowTool(TraversalKind, Selection, TraversalKind) { events.push_back("borrow"); return true; }
    Crossing RequestTraversal(TraversalKind) { events.push_back("request"); return Crossing::Ready; }
    void TraceRequestedTraversal(TraversalKind, Crossing) {}
    void TraceHop(Crossing) {}
    void TraceCheck(Crossing, bool, Crossing, Check) {}
};
// Fail at every ownership/phase boundary and prove no later borrowed object
// is inspected. This is the new lazy context contract, not a native layout mock.
struct HurryContext {
    int blocked = -1, next{};
    bool Fact() { return next++ != blocked; }
    bool HasOwners() { return Fact(); }
    bool ManagerOwnsText() { return Fact(); }
    bool TextOwnsManager() { return Fact(); }
    bool InRequestedPhase() { return Fact(); }
    bool HasContext() { return Fact(); }
    bool TextSharesContext() { return Fact(); }
    bool ContextReady() { return Fact(); }
    bool SupportedWindow() { return Fact(); }
    bool HasChoices() { return Fact(); }
    bool ChoicesInactive() { return Fact(); }
    bool AtPageEndOrInputPause() { return Fact(); }
    bool WaitingForManualAdvance() { return Fact(); }
    bool HasMessage() { return Fact(); }
    bool MessageReady() { return Fact(); }
    bool NewCatchAnnouncement() { return !Fact(); }
};
int main() {
    constexpr const char *reasons[]{"missing-owner", "manager-text", "text-manager", "manager-state",
        "missing-context", "text-context", "context-phase", "window", "missing-choices", "choice-active",
        "text-not-ready", "scripted-timer", "missing-message", "message-transition", "new-catch"};
    for (int blocked = 0; blocked < 15; ++blocked) {
        HurryContext context{blocked};
        const auto reason = DialogueAdvanceBlock(context);
        assert(reason && !std::strcmp(reason, reasons[blocked]));
        assert(context.next == blocked + 1);
    }
    HurryContext context;
    assert(!DialogueAdvanceBlock(context) && context.next == 15);
    PickupCategory locked = PickupCategory::Item; bool first = false;
    ObservePickupHold(locked, first, false, false, false);
    assert(locked == PickupCategory::None && !first);
    ObservePickupHold(locked, first, false, true, true); assert(first);
    assert(!LatchPickup(false, first, false));
    for (auto category : {PickupCategory::Item, PickupCategory::Crop, PickupCategory::Weed, PickupCategory::Moss}) {
        assert(LatchPickup(true, true, false));
        assert(RepeatPickup(category, false, false));
        assert(RepeatMatches(category, category));
        assert(!RepeatMatches(category, PickupCategory::None));
        assert(!RepeatPickup(category, false, true));
    }
    assert(!RepeatMatches(PickupCategory::Weed, PickupCategory::Moss));
    assert(IsNewCatchAnnouncement("SYS_Get_Fish", "001"));
    assert(!IsNewCatchAnnouncement("SYS_Get_Fish", "001x"));
    assert(!HurryRequested("choice-active", true));
    assert(!HurryRequested(nullptr, false)); assert(HurryRequested(nullptr, true));
    // Real action preference and native-hop priority, with no runtime headers.
    Traversal t;
    assert(TraverseWhileHeld(t, {true,true,true,true}, false) == Crossing::Ready);
    assert((t.events == std::vector<std::string>{"consume","hop","check placed","find ladder","check ladder","find pole","check pole","borrow"}));
    t = {}; t.placed = true;
    assert(TraverseWhileHeld(t, {true,true,true,true}, false) == Crossing::Ready);
    assert((t.events == std::vector<std::string>{"consume","hop","check placed","request"}));
    t = {}; t.hop = Crossing::Waiting;
    assert(TraverseWhileHeld(t, {true,true,true,true}, false) == Crossing::Waiting);
    assert((t.events == std::vector<std::string>{"consume","hop"}));
    t = {}; t.held = TraversalKind::Pole; t.complete = false;
    assert(TraverseWhileHeld(t, {true,false,false,false}, false) == Crossing::Waiting);
    assert(t.events.back() == "remember");
    t = {}; t.intent = false;
    assert(TraverseWhileHeld(t, {true,true,true,true}, false) == Crossing::None);
    assert((t.events == std::vector<std::string>{"consume","hop","wall hop"}));
    Equipment e; Borrow b{BorrowStage::Equipping};
    assert(ContinueBorrowedTraversal(b, e) == Crossing::None && e.events.empty());
    FinishTraversalAction(b, e, true, false, false); assert(!b.equipFinished);
    FinishTraversalAction(b, e, false, true, false);
    FinishTraversalAction(b, e, true, false, false); assert(b.equipFinished);
    assert(ContinueBorrowedTraversal(b, e) == Crossing::Ready && b.stage == BorrowStage::Traversing);
    assert((e.events == std::vector<std::string>{"check","traverse"}));
    assert(ContinueBorrowedTraversal(b, e) == Crossing::None && e.events.size() == 2);
    FinishTraversalAction(b, e, true, false, false); assert(e.events.size() == 2);
    FinishTraversalAction(b, e, false, false, true);
    FinishTraversalAction(b, e, true, false, false); assert(e.events.back() == "restore");
    e = {}; e.owns = false; b = {BorrowStage::Equipping,false,true,false};
    assert(ContinueBorrowedTraversal(b, e) == Crossing::None && e.events.back() == "release");
    for (bool valid : {false,true}) {
        e = {}; e.valid = valid; e.intent = false; b = {BorrowStage::Equipping,false,true,false};
        assert(ContinueBorrowedTraversal(b, e) == Crossing::Ready);
        assert((e.events == std::vector<std::string>{"check","restore"}));
    }
    e = {}; e.accepts = false;
    assert(!BorrowTraversalTool(e, Selection{12,2}, true));
    assert((e.events == std::vector<std::string>{"find equipped","record","change 2","clear"}));
    e = {}; e.sameItem = false;
    RestoreTraversalTool(e, Selection{42,1});
    assert((e.events == std::vector<std::string>{"find exact","clear","change -1","restore"}));
    StrokeState swim;
    auto request = SwimWithStick(swim, 100*TicksPerSecond, false, false, false, 1.f, false, false);
    assert(request.active && request.pulse);
    request = SwimWithStick(swim, 100*TicksPerSecond, false, true, false, 1.f, false, false);
    assert(request.armEntry && !request.pulse && request.boost == 0.f);
    assert(BoostEnvelope(swim, swim.lastStroke + BoostDelay) == 1.f);
    assert(BoostEnvelope(swim, swim.lastStroke + BoostDelay + BoostDuration) == 0.f);
    assert(StickThrottle(std::numeric_limits<float>::quiet_NaN(), 0.f) == 0.f);
    assert(StickThrottle(.15f, 0.f) == 0.f);
    swim.armPending = swim.pendingManual = true;
    request = SwimWithStick(swim, swim.lastStroke, false, false, false, 1.f, true, true);
    assert(!request.active && !request.pulse && !swim.armPending);
    assert(SwimmingPropulsion(.8f, 1.f, 1.f, false) == .8f);
    std::puts("PASS: Controls action preference, lazy hurry guards, borrow/restore ownership and cancellation, stroke timing and transition priority");
}
