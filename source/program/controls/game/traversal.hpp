#pragma once
namespace qol::controls::game {
// Semantic kinds; native item-kind values stay in the version adapter.
enum class TraversalKind { None, Other, Pole, Ladder, PlacedLadder };
// Native HopEntry's existing result contract. Ready in the movement phase is
// permission to progress its timer/dispatcher, not proof a jump was committed.
enum class Crossing { None, Ready, Waiting };
struct TraversalOptions { bool pole{}, ladder{}, autoEquip{}, placedLadder{}; };
}
