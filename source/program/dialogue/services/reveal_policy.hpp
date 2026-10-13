#pragma once
#include "program/dialogue/game/dialogue_context.hpp"
#include <optional>

namespace qol::dialogue {
// Mod classification evidence, used for diagnostics as well as speed policy.
// These names do not claim the engine has an automatic-boost classifier.
enum class ClassificationSource { Unknown, ConfirmedUnnamed, ReviewedBank, Dialog, Player, Npc };
struct ClassifiedMessage {
    MessageKind kind = MessageKind::Unknown;
    ClassificationSource source = ClassificationSource::Unknown;
    std::string_view resourceId{}, localizedName{};
};
struct RevealDecision {
    unsigned nominalSpeed{};
    ClassifiedMessage message{};
};
using RevealSelector = RevealDecision (*)(const game::DialogueContext &);
struct RevealPolicy {
    RevealSelector choose{};
    unsigned baseSpeed{}, boostSpeed{};
    bool operator==(const RevealPolicy &) const = default;
    bool Valid() const { return choose && baseSpeed >= 1 && boostSpeed >= baseSpeed && boostSpeed <= 30; }
};
// One speed tier wins. Hurry never multiplies Faster Dialogue's chosen speed.
// Controls supplies its verified request/speed and remains the only page advancer.
inline unsigned ChooseRevealSpeed(unsigned dialogueSpeed, std::optional<unsigned> hurrySpeed) {
    return hurrySpeed ? *hurrySpeed : dialogueSpeed;
}
// Selectors return one of their registered nominal speeds; unsupported
// results fall back to base. All message/identity borrows remain synchronous.
// Startup-only copied declaration. Null/invalid/conflicting policies are
// rejected before publishing hooks. No caller-owned declaration is retained.
bool RegisterFasterDialoguePolicy(RevealPolicy policy);
}
