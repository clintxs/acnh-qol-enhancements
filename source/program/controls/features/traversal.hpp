#pragma once
#include "../game/traversal.hpp"

namespace qol::controls::features {
using game::TraversalKind;
using game::Crossing;
enum class BorrowStage { None, Equipping, Traversing };

// The operations here borrow one player synchronously. A movement proof is an
// adapter-owned, one-use observation of the same identity, exact bank and held
// kind; it grants no landing reservation. Every commit revalidates natively.
template<class Traversal>
Crossing TraverseWhileHeld(Traversal &traversal, game::TraversalOptions options, bool movementPhase) {
    const auto proof = traversal.ConsumeMovementProof();
    if (options.autoEquip && traversal.ToolBorrowActive()) return traversal.OrdinaryHop();

    auto hop = traversal.SearchNativeHop(false);
    if (hop == Crossing::None && !traversal.IntentObserved() && options.ladder &&
        (options.placedLadder || options.autoEquip || traversal.HeldTool() == TraversalKind::Ladder))
        hop = traversal.SearchNativeHop(true); // Ladder-only wall-contact extension.

    // Native stick/contact/direction guards and ALL adjusted landing searches
    // (including a pending ordinary hop) finish before a tool is considered.
    if (hop != Crossing::None || !traversal.IntentObserved()) {
        traversal.TraceHop(hop);
        return hop;
    }
    if (options.placedLadder) {
        auto placed = movementPhase ? traversal.TransferProof(proof, TraversalKind::PlacedLadder)
                                    : traversal.CheckLanding(TraversalKind::PlacedLadder);
        if (placed.Valid()) {
            if (!traversal.HoldComplete()) {
                if (!movementPhase) traversal.RememberBank(TraversalKind::PlacedLadder);
                return Crossing::Waiting;
            }
            if (movementPhase) return Crossing::Ready;
            return traversal.RequestTraversal(TraversalKind::PlacedLadder);
        }
    }
    const auto held = traversal.HeldTool();
    auto tool = held;
    bool ladder = options.ladder && tool == TraversalKind::Ladder;
    auto selection = traversal.EmptySelection();
    auto check = traversal.EmptyCheck();
    if (movementPhase) {
        tool = traversal.ProvenTool(proof);
        ladder = tool == TraversalKind::Ladder;
        check = traversal.TransferProof(proof, tool);
        if (!(options.autoEquip || tool == held)) check.transferred = false;
    } else {
        if (ladder) check = traversal.CheckLanding(TraversalKind::Ladder);
        else if (!traversal.IncludesWalls() && options.pole && tool == TraversalKind::Pole)
            check = traversal.CheckLanding(TraversalKind::Pole);
        if (options.autoEquip && !check.Valid()) {
            ladder = false;
            tool = TraversalKind::None;
            // Placed ladder, already held tool, pocket ladder, then pocket pole.
            if (options.ladder && held != TraversalKind::Ladder) {
                selection = traversal.FindTool(TraversalKind::Ladder);
                if (selection.slot >= 0) {
                    check = traversal.CheckLanding(TraversalKind::Ladder);
                    if (check.Valid()) { tool = TraversalKind::Ladder; ladder = true; }
                }
            }
            if (!ladder && !traversal.IncludesWalls() && options.pole && held != TraversalKind::Pole) {
                selection = traversal.FindTool(TraversalKind::Pole);
                if (selection.slot >= 0) { tool = TraversalKind::Pole; check = traversal.CheckLanding(tool); }
            }
        }
    }
    // Solid-wall contact is never permission to vault through a wall.
    if (traversal.IncludesWalls() && !ladder) return hop;
    if (!(ladder || (options.pole && tool == TraversalKind::Pole))) return hop;
    const bool valid = check.Valid();
    const bool complete = valid && traversal.HoldComplete();
    if (!valid || !complete || movementPhase) {
        const auto result = !valid ? Crossing::None : complete ? Crossing::Ready : Crossing::Waiting;
        if (valid && !complete && !movementPhase) traversal.RememberBank(tool);
        traversal.TraceCheck(hop, valid, result, check);
        return result;
    }
    if (tool != held)
        return options.autoEquip && traversal.BorrowTool(tool, selection, held) ? Crossing::Ready : Crossing::None;
    const auto result = traversal.RequestTraversal(tool);
    traversal.TraceCheck(hop, valid, result, check);
    traversal.TraceRequestedTraversal(tool, result);
    return result;
}

// Native action-entry completion is authoritative, including after the stick
// is released. A queued Move is not evidence that a traversal has finished.
template<class Borrow, class Equipment>
void FinishTraversalAction(Borrow &borrow, Equipment &equipment, bool locomotion,
                           bool toolChange, bool traversalAction) {
    if (borrow.stage == BorrowStage::Equipping) {
        if (toolChange) borrow.equipEntered = true;
        if (locomotion && borrow.equipEntered) borrow.equipFinished = true;
    } else if (borrow.stage == BorrowStage::Traversing) {
        if (traversalAction) borrow.traversalEntered = true;
        if (locomotion && borrow.traversalEntered) {
            if (equipment.StillOwnsBorrowedTool()) equipment.RestorePreviousTool();
            else equipment.ReleaseOwnership();
        }
    }
}
template<class Borrow, class Equipment>
Crossing ContinueBorrowedTraversal(Borrow &borrow, Equipment &equipment) {
    if (borrow.stage == BorrowStage::Equipping && !borrow.equipFinished) return Crossing::None;
    if (!equipment.StillOwnsBorrowedTool()) { equipment.ReleaseOwnership(); return Crossing::None; }
    if (borrow.stage == BorrowStage::Traversing) return Crossing::None;
    // Recheck both stick intent and native geometry after asynchronous equip.
    const auto stick = equipment.Stick();
    const bool intent = stick.x*stick.x + stick.y*stick.y >= .81f &&
        equipment.FacingMatchesBorrow() && equipment.AtBorrowedBank();
    const auto check = equipment.CheckLanding();
    if (!intent || !check.Valid()) {
        equipment.TraceCancellation(intent, check);
        equipment.RestorePreviousTool();
        return Crossing::Ready;
    }
    equipment.TraceTraversal();
    borrow.stage = BorrowStage::Traversing;
    if (!equipment.RequestTraversal()) equipment.RestorePreviousTool();
    return Crossing::Ready;
}
// Borrow/restore use native silent ToolChange requests, not pocket writes.
template<class Equipment, class Selection>
bool BorrowTraversalTool(Equipment &equipment, Selection selection, bool hasHeldTool) {
    if (!equipment.CanBeginBorrow() || selection.slot < 0) return false;
    auto previous = equipment.EmptySelection();
    if (hasHeldTool) {
        previous = equipment.FindEquippedItem();
        if (previous.slot < 0) return false; // Event/temporary equipment is not ours.
    }
    equipment.RecordBorrow(selection, previous);
    if (!equipment.RequestToolChange(selection)) { equipment.ClearBorrow(); return false; }
    equipment.TraceBorrow();
    return true;
}
template<class Equipment, class Selection>
void RestoreTraversalTool(Equipment &equipment, Selection previous) {
    // Match the full item value, including customization/durability. Search
    // again after reordering; if removed, restore empty hands, never recreate.
    if (previous.slot >= 0 && !equipment.SameItemAtSlot(previous))
        previous = equipment.FindExactItem(previous.item);
    equipment.ClearBorrow(); // Clear before scheduling, including rejection.
    equipment.RequestToolChange(previous);
    equipment.TraceRestore(previous);
}
}
