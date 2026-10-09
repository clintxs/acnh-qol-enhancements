#pragma once
#include "program/core/config.hpp"
#include "localized_name.hpp"
#include <array>

namespace qol::dialogue {
inline constexpr unsigned MaxNpcs = 64, NpcIdBytes = 16;
struct Settings {
    unsigned baseSpeed = 2, boostSpeed = 4;
    bool fasterUi = false, fasterUnnamed = false;
    unsigned npcCount = 0;
    std::array<std::array<char, NpcNameBytes>, MaxNpcs> npcs{};
};
enum class MessageKind { Unknown, Npc, Ui, Unnamed };
constexpr char Lower(char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
inline bool Equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (unsigned i = 0; i < a.size(); ++i) if (Lower(a[i]) != Lower(b[i])) return false;
    return true;
}
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
    // A list entry can be a resource ID or the localized visible name. The
    // current speaker, not spelling heuristics, determines whether it matches.
    if (name.empty() || name.size() >= NpcNameBytes || !core::ValidUtf8(name)) return false;
    for (unsigned i = 0; i < settings.npcCount; ++i)
        if (Equal(name, settings.npcs[i].data())) return true;
    if (settings.npcCount == MaxNpcs) return false;
    auto &id = settings.npcs[settings.npcCount++];
    for (unsigned i = 0; i < name.size(); ++i) id[i] = Lower(name[i]);
    return true;
}
inline unsigned SelectSpeed(const Settings &settings, MessageKind kind,
                            std::string_view npc, bool held, std::string_view localizedName = {}) {
    bool automatic = (kind == MessageKind::Ui && settings.fasterUi) ||
                     (kind == MessageKind::Unnamed && settings.fasterUnnamed);
    if (kind == MessageKind::Npc)
        for (unsigned i = 0; i < settings.npcCount; ++i)
            automatic |= Equal(npc, settings.npcs[i].data()) ||
                         (!localizedName.empty() && Equal(localizedName, settings.npcs[i].data()));
    return held || automatic ? settings.boostSpeed : settings.baseSpeed;
}
// These are the two existing, nominal tiers, relative to the game's live fast
// rate. Keeping flags 1/3 also preserves the native fast-mode voice parameter.
inline bool ParseSpeed(std::string_view value, unsigned &speed) {
    if (value != "2" && value != "4") return false;
    speed = value == "2" ? 2 : 4;
    return true;
}
// Both files use this parser. Missing/invalid values inherit the earlier layer;
// duplicates restore that key's earlier value. A supplied NPC list replaces
// the earlier list, while invalid entries leave its other entries usable.
inline unsigned ApplySettings(std::string_view input, Settings &output) {
    if (input.size() > core::MaxConfigBytes || !core::ValidUtf8(input, true)) return 1;
    const Settings previous = output;
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
        if (key == "enabled" || key == "version") continue; // Validated by Core.
        // Old/new spellings address one preference, including duplicate-key
        // handling. Either spelling may override the earlier file's value.
        const unsigned bit = key == "base_speed" ? 1 : key == "boost_speed" ? 2 :
                             key == "faster_ui" ? 4 : key == "faster_npc_list" ? 8 : (key == "faster_unnamed" || key == "faster_player") ? 16 : 0;
        if (!bit) { ++warnings; continue; }
        if (seen & bit) { duplicate |= bit; ++warnings; continue; }
        seen |= bit;
        if (bit == 1 || bit == 2) {
            if (!ParseSpeed(value, bit == 1 ? output.baseSpeed : output.boostSpeed)) ++warnings;
        } else if (bit == 4 || bit == 16) {
            if (value == "true" || value == "false")
                (bit == 4 ? output.fasterUi : output.fasterUnnamed) = value == "true";
            else ++warnings;
        } else {
            output.npcCount = 0;
            output.npcs = {};
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
    if (duplicate & 1) output.baseSpeed = previous.baseSpeed;
    if (duplicate & 2) output.boostSpeed = previous.boostSpeed;
    if (duplicate & 4) output.fasterUi = previous.fasterUi;
    if (duplicate & 16) output.fasterUnnamed = previous.fasterUnnamed;
    if (duplicate & 8) { output.npcCount = previous.npcCount; output.npcs = previous.npcs; }
    return warnings;
}
// Normalize only after both layers: a custom lower base can make a regular
// boost valid without having that inherited boost changed prematurely.
inline unsigned NormalizeSettings(Settings &output) {
    unsigned warnings = 0;
    if (output.boostSpeed < output.baseSpeed) { output.boostSpeed = output.baseSpeed; ++warnings; }
    return warnings;
}
inline unsigned ParseSettings(std::string_view input, Settings &output) {
    output = {};
    const auto warnings = ApplySettings(input, output);
    return warnings + NormalizeSettings(output);
}
unsigned Configure(std::string_view input);
bool Install();
} // namespace qol::dialogue
