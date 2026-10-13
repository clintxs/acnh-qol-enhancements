#pragma once
#include "program/dialogue/settings.hpp"
#include "program/dialogue/services/reveal_policy.hpp"

namespace qol::dialogue {
// Faster Dialogue starts here: message classification and automatic boost are
// mod policy. Native adapters provide presentation/name/identity evidence.
// Reviewed self-message banks from the supplied 3.0.3 resources. Matching
// exact basenames avoids treating NPC banks or all SYS_/FTR_ messages as self.
inline bool IsUnnamedMessageBank(std::string_view bank) {
    const auto slash = bank.find_last_of('/');
    if (slash != std::string_view::npos) bank.remove_prefix(slash + 1);
    if (bank.ends_with(".msbt")) bank.remove_suffix(5);
    constexpr std::string_view banks[]{"FTR_WorkingTable", "SYS_Player", "SYS_Get_Fish",
                                       "SYS_Get_Insect", "SYS_Get_DiveFish"};
    for (auto known : banks) if (Equal(bank, known)) return true;
    return false;
}

inline ClassifiedMessage ClassifyMessage(const game::Message &message) {
    using game::Presentation;
    using game::NameTag;
    if (message.presentation == Presentation::Unknown) return {};
    if (message.presentation != Presentation::Dialog && message.nameTag == NameTag::Unnamed)
        return {MessageKind::Unnamed, ClassificationSource::ConfirmedUnnamed};
    // Preserve the reviewed legacy fallback, including its priority over UI
    // when name preparation is unknown. Confirmed names override the guess.
    if (message.nameTag == NameTag::Unknown && IsUnnamedMessageBank(message.bank))
        return {MessageKind::Unnamed, ClassificationSource::ReviewedBank};
    if (message.presentation == Presentation::Dialog)
        return {MessageKind::Ui, ClassificationSource::Dialog};
    if (message.presentation != Presentation::Talk) return {};
    const auto speaker = message.Speaker();
    if (speaker.kind == game::SpeakerKind::Player && message.nameTag != NameTag::Named)
        return {MessageKind::Unnamed, ClassificationSource::Player};
    if (speaker.kind == game::SpeakerKind::Npc)
        return {MessageKind::Npc, ClassificationSource::Npc, speaker.resourceId, speaker.localizedName};
    return {}; // Missing/unknown identity is not evidence of an unnamed message.
}
inline bool ShouldAutomaticallyBoost(const Settings &settings, MessageKind kind,
                                     std::string_view npc = {}, std::string_view localizedName = {}) {
    if (kind == MessageKind::Ui) return settings.boostUiMessages;
    if (kind == MessageKind::Unnamed) return settings.boostUnnamedMessages;
    if (kind == MessageKind::Npc) {
        for (unsigned i = 0; i < settings.npcCount; ++i)
            if (Equal(npc, settings.npcs[i].data()) ||
                (!localizedName.empty() && Equal(localizedName, settings.npcs[i].data()))) return true;
    }
    return false;
}
inline unsigned SelectSpeed(const Settings &settings, MessageKind kind,
                            std::string_view npc, bool nativeBoostRequested,
                            std::string_view localizedName = {}) {
    const bool boosted = nativeBoostRequested || ShouldAutomaticallyBoost(settings, kind, npc, localizedName);
    return boosted ? settings.boostSpeed : settings.baseSpeed;
}
inline RevealDecision ChooseSpeed(const game::DialogueContext &dialogue, const Settings &settings) {
    const auto message = ClassifyMessage(dialogue.message);
    return {SelectSpeed(settings, message.kind, message.resourceId, dialogue.nativeBoostRequested,
                        message.localizedName), message};
}
// The shared reveal service applies ChooseRevealSpeed after Controls' native
// hurry check. It chooses a single tier, preserving native gates/overrides;
// Faster Dialogue supplies no page-advance request.
}
