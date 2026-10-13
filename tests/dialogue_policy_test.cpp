#include "program/dialogue/features/faster_dialogue.hpp"
#include <cassert>
#include <iostream>
using namespace qol::dialogue;
struct IdentityFixture {
    unsigned calls{};
    game::SpeakerIdentity value{};
    static game::SpeakerIdentity Resolve(void *context) {
        auto &self = *static_cast<IdentityFixture *>(context);
        ++self.calls;
        return self.value;
    }
};
int main() {
    Settings settings{};
    assert(!ParseSettings("base_speed=3\nboost_speed=9\nboost_npc_list=Timmy,Thibou", settings));
    IdentityFixture identity{0, {game::SpeakerKind::Npc, "rcm", "Timmy"}};
    game::Message message{game::Presentation::Talk, game::NameTag::Unknown, "SP_rcm_00_common",
                          &identity, IdentityFixture::Resolve};
    auto decision = ChooseSpeed({message, false}, settings);
    assert(decision.nominalSpeed == 9 && decision.message.source == ClassificationSource::Npc && identity.calls == 1);
    identity.value = {game::SpeakerKind::Npc, "owl", "Thibou"};
    assert(ChooseSpeed({message, false}, settings).nominalSpeed == 9);
    identity.value = {game::SpeakerKind::Npc, "owl", "THIBOU"};
    assert(ChooseSpeed({message, false}, settings).nominalSpeed == 9);
    identity.value = {game::SpeakerKind::Npc, "owl", "other"};
    assert(ChooseSpeed({message, false}, settings).nominalSpeed == 3);
    assert(ChooseSpeed({message, true}, settings).nominalSpeed == 9);
    identity.value = {};
    decision = ChooseSpeed({message, false}, settings);
    assert(decision.nominalSpeed == 3 && decision.message.kind == MessageKind::Unknown);
    message.resolveIdentity = nullptr;
    assert(ChooseSpeed({message, false}, settings).nominalSpeed == 3);
    assert(ChooseSpeed({message, true}, settings).nominalSpeed == 9);
    message.resolveIdentity = IdentityFixture::Resolve;
    identity.calls = 0;
    message.presentation = game::Presentation::Dialog;
    decision = ChooseSpeed({message, false}, settings);
    assert(decision.nominalSpeed == 9 && decision.message.kind == MessageKind::Ui && identity.calls == 0);
    message.presentation = game::Presentation::Balloon;
    message.nameTag = game::NameTag::Unknown;
    assert(ChooseSpeed({message, false}, settings).message.kind == MessageKind::Unknown && identity.calls == 0);
    message.nameTag = game::NameTag::Unnamed;
    assert(ChooseSpeed({message, false}, settings).message.kind == MessageKind::Unnamed && identity.calls == 0);
    // Reviewed banks preserve the historical unknown-name priority over UI.
    message = {game::Presentation::Dialog, game::NameTag::Unknown, "directory/SYS_Get_Fish.msbt",
               &identity, IdentityFixture::Resolve};
    decision = ChooseSpeed({message, false}, settings);
    assert(decision.message.source == ClassificationSource::ReviewedBank && identity.calls == 0);
    settings.boostUnnamedMessages = false;
    assert(ChooseSpeed({message, false}, settings).nominalSpeed == 3); // UI's switch cannot override this classification.
    message.nameTag = game::NameTag::Named;
    decision = ChooseSpeed({message, false}, settings);
    assert(decision.message.source == ClassificationSource::Dialog && decision.nominalSpeed == 9);
    message.presentation = game::Presentation::Talk;
    identity.value = {game::SpeakerKind::Npc, "rcm", {}};
    decision = ChooseSpeed({message, false}, settings);
    assert(decision.message.kind == MessageKind::Npc && decision.nominalSpeed == 9 && identity.calls == 1);
    identity.value = {game::SpeakerKind::Player, {}, {}};
    assert(ChooseSpeed({message, false}, settings).message.kind == MessageKind::Unknown); // Confirmed name wins over a player guess.
    message.nameTag = game::NameTag::Unknown;
    message.bank = "FTR_NotReviewed";
    assert(ChooseSpeed({message, false}, settings).message.source == ClassificationSource::Player);
    message.presentation = game::Presentation::Unknown;
    const auto calls = identity.calls;
    assert(ChooseSpeed({message, false}, settings).message.kind == MessageKind::Unknown && identity.calls == calls);
    settings.baseSpeed = settings.boostSpeed = 7;
    assert(ChooseSpeed({message, true}, settings).nominalSpeed == 7);
    assert(ChooseRevealSpeed(9, 30) == 30 && ChooseRevealSpeed(9, std::nullopt) == 9);
    assert(ChooseRevealSpeed(2, 30) == 30); // Hurry is a winning tier, never an independent multiplier.
    for (auto policy : {RevealPolicy{}, RevealPolicy{+[](const game::DialogueContext &) -> RevealDecision { return {}; }, 0, 4},
         RevealPolicy{+[](const game::DialogueContext &) -> RevealDecision { return {}; }, 4, 3},
         RevealPolicy{+[](const game::DialogueContext &) -> RevealDecision { return {}; }, 2, 31}})
        assert(!policy.Valid());
    std::cout << "PASS: native-request speed choice, conservative message classification, lazy identity, configured matches and single-tier hurry precedence\n";
}
