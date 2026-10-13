#pragma once
#include <string_view>

namespace qol::dialogue {
enum class MessageKind { Unknown, Npc, Ui, Unnamed };
namespace game {
enum class Presentation { Unknown, Talk, Dialog, Balloon };
enum class NameTag { Unknown, Named, Unnamed };
enum class SpeakerKind { Unknown, Player, Npc };
struct SpeakerIdentity {
    SpeakerKind kind = SpeakerKind::Unknown;
    std::string_view resourceId{}, localizedName{};
};
struct Message {
    Presentation presentation = Presentation::Unknown;
    NameTag nameTag = NameTag::Unknown;
    std::string_view bank{};
    void *identityContext{};
    SpeakerIdentity (*resolveIdentity)(void *){};
    // Synchronous, lazy borrow: resolve only if this presentation needs a
    // speaker. Native identity/generation/type validation remains authoritative.
    // Strings and callback context expire when the reveal callback returns.
    SpeakerIdentity Speaker() const {
        return resolveIdentity ? resolveIdentity(identityContext) : SpeakerIdentity{};
    }
};
struct DialogueContext {
    Message message{};
    // Pre-policy native request at the eligible reveal phase. This is distinct
    // from permitted reveal, the selected effective tier, and an achieved rate.
    // The retained input adapter normalizes B/R release and excludes L first.
    bool nativeBoostRequested{};
};
}
}
