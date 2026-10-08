#pragma once
#include "program/core/config.hpp"
#include <array>

namespace qol::dialogue {
inline constexpr char CustomConfigPath[] =
    "content:/qol-enhancements/config/faster-dialogue.custom.ini";
inline constexpr unsigned MaxNpcs = 64, NpcIdBytes = 16;
struct Settings {
    unsigned baseSpeed = 2, boostSpeed = 4;
    bool fasterUi = false, fasterPlayer = false;
    unsigned npcCount = 0;
    std::array<std::array<char, NpcIdBytes>, MaxNpcs> npcs{};
};
enum class MessageKind { Unknown, Npc, Ui, Player };
constexpr char Lower(char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
inline bool Equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (unsigned i = 0; i < a.size(); ++i) if (Lower(a[i]) != Lower(b[i])) return false;
    return true;
}
// Reviewed self-message banks from the supplied 3.0.3 resources. Matching
// exact basenames avoids treating NPC banks or all SYS_/FTR_ messages as self.
inline bool IsPlayerMessageBank(std::string_view bank) {
    const auto slash = bank.find_last_of('/');
    if (slash != std::string_view::npos) bank.remove_prefix(slash + 1);
    if (bank.ends_with(".msbt")) bank.remove_suffix(5);
    constexpr std::string_view banks[]{"FTR_WorkingTable", "SYS_Player", "SYS_Get_Fish",
                                       "SYS_Get_Insect", "SYS_Get_DiveFish"};
    for (auto known : banks) if (Equal(bank, known)) return true;
    return false;
}
inline std::string_view NpcAlias(std::string_view name) {
    constexpr std::string_view names[]{"Timmy", "Tommy", "Orville", "Wilbur", "Kapp'n"};
    constexpr std::string_view ids[]{"rcm", "rct", "dod", "doc", "kpp"};
    for (unsigned i = 0; i < 5; ++i) if (Equal(name, names[i])) return ids[i];
    return name;
}
inline bool AddNpc(Settings &settings, std::string_view name) {
    name = core::Trim(name);
    // SpNpcParam has both kpp and kppB with Kapp'n's identity. A friendly
    // character name covers both known profiles; explicit IDs stay exact.
    if (Equal(name, "Kapp'n")) {
        const bool regular = AddNpc(settings, "kpp");
        const bool alternate = AddNpc(settings, "kppB");
        return regular && alternate;
    }
    name = NpcAlias(name);
    // Internal resource IDs are ASCII identifiers. Unknown friendly names do
    // not silently become IDs; arbitrary real IDs (including villagers) work.
    if (name.size() < 3 || name.size() >= NpcIdBytes) return false;
    for (char c : name) if (!((Lower(c) >= 'a' && Lower(c) <= 'z') ||
                             (c >= '0' && c <= '9') || c == '_')) return false;
    for (unsigned i = 0; i < settings.npcCount; ++i)
        if (Equal(name, settings.npcs[i].data())) return true;
    if (settings.npcCount == MaxNpcs) return false;
    auto &id = settings.npcs[settings.npcCount++];
    for (unsigned i = 0; i < name.size(); ++i) id[i] = Lower(name[i]);
    return true;
}
inline unsigned SelectSpeed(const Settings &settings, MessageKind kind,
                            std::string_view npc, bool held) {
    bool automatic = (kind == MessageKind::Ui && settings.fasterUi) ||
                     (kind == MessageKind::Player && settings.fasterPlayer);
    if (kind == MessageKind::Npc)
        for (unsigned i = 0; i < settings.npcCount; ++i)
            automatic |= Equal(npc, settings.npcs[i].data());
    return held || automatic ? settings.boostSpeed : settings.baseSpeed;
}
// These are the two existing, nominal tiers, relative to the game's live fast
// rate. Keeping flags 1/3 also preserves the native fast-mode voice parameter.
inline bool ParseSpeed(std::string_view value, unsigned &speed) {
    if (value != "2" && value != "4") return false;
    speed = value == "2" ? 2 : 4;
    return true;
}
// Personal preferences are tolerant per key: a bad key/value warns and leaves
// that setting at its default; a bad NPC entry leaves other entries usable.
// Duplicate keys invalidate that setting, avoiding order-dependent surprises.
inline unsigned ParseSettings(std::string_view input, Settings &output) {
    output = {};
    if (input.size() > core::MaxConfigBytes) return 1;
    if (input.starts_with("\xef\xbb\xbf")) input.remove_prefix(3);
    unsigned warnings = 0, seen = 0, duplicate = 0;
    while (!input.empty()) {
        const auto end = input.find('\n');
        const auto line = core::Trim(input.substr(0, end));
        input = end == std::string_view::npos ? std::string_view{} : input.substr(end + 1);
        if (line.empty() || line.front() == '#' || line.front() == ';') continue;
        const auto equals = line.find('=');
        if (equals == std::string_view::npos) { ++warnings; continue; }
        const auto key = core::Trim(line.substr(0, equals));
        const auto value = core::Trim(line.substr(equals + 1));
        const unsigned bit = key == "base_speed" ? 1 : key == "boost_speed" ? 2 :
                             key == "faster_ui" ? 4 : key == "faster_npc_list" ? 8 : key == "faster_player" ? 16 : 0;
        if (!bit) { ++warnings; continue; }
        if (seen & bit) { duplicate |= bit; ++warnings; continue; }
        seen |= bit;
        if (bit == 1 || bit == 2) {
            if (!ParseSpeed(value, bit == 1 ? output.baseSpeed : output.boostSpeed)) ++warnings;
        } else if (bit == 4 || bit == 16) {
            if (value == "true" || value == "false")
                (bit == 4 ? output.fasterUi : output.fasterPlayer) = value == "true";
            else ++warnings;
        } else {
            auto list = value;
            while (!list.empty()) {
                const auto comma = list.find(',');
                if (!AddNpc(output, list.substr(0, comma))) ++warnings;
                if (comma == std::string_view::npos) break;
                list.remove_prefix(comma + 1);
                if (list.empty()) ++warnings;
            }
        }
    }
    if (duplicate & 1) output.baseSpeed = 2;
    if (duplicate & 2) output.boostSpeed = 4;
    if (duplicate & 4) output.fasterUi = false;
    if (duplicate & 16) output.fasterPlayer = false;
    if (duplicate & 8) { output.npcCount = 0; output.npcs = {}; }
    if (output.boostSpeed < output.baseSpeed) { output.boostSpeed = output.baseSpeed; ++warnings; }
    return warnings;
}
bool Install();
} // namespace qol::dialogue
