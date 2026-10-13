#pragma once
#include "program/core/settings_schema.hpp"
#include "localized_name.hpp"
#include "game/dialogue_context.hpp"
#include <array>

namespace qol::dialogue {
inline constexpr unsigned MaxNpcs = 64, NpcIdBytes = 16;
inline constexpr unsigned MinSpeed = 1, MaxSpeed = 30;
struct Settings {
    unsigned baseSpeed = 2, boostSpeed = 4;
    bool boostUiMessages = true, boostUnnamedMessages = true;
    unsigned npcCount = 0;
    std::array<std::array<char, NpcNameBytes>, MaxNpcs> npcs{};
};
constexpr char Lower(char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
inline bool Equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (unsigned i = 0; i < a.size(); ++i) if (Lower(a[i]) != Lower(b[i])) return false;
    return true;
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
// Nominal whole-number multipliers. The game's live fast rate represents 2x;
// rate hooks scale it by the selected multiplier / 2, preserving locale/FPS.
inline bool ParseSpeed(std::string_view value, unsigned &speed) {
    return core::settings::ParseUnsigned(value, speed,
        {MinSpeed, MaxSpeed, core::settings::Overflow::Reject,
         core::settings::LeadingZeros::Reject});
}
// Both files use this parser. Missing/invalid values inherit the earlier layer;
// duplicates restore that key's earlier value. A supplied NPC list replaces
// the earlier list, while invalid entries leave its other entries usable.
inline unsigned ApplyNpcList(std::string_view list, Settings &output) {
    output.npcCount = 0;
    output.npcs = {};
    unsigned warnings = 0;
    while (!list.empty()) {
        const auto comma = list.find(',');
        if (!AddNpc(output, list.substr(0, comma))) ++warnings;
        if (comma == std::string_view::npos) break;
        list.remove_prefix(comma + 1);
        if (list.empty()) ++warnings;
    }
    return warnings;
}
inline void RestoreNpcList(Settings &output, const Settings &previous) {
    output.npcCount = previous.npcCount;
    output.npcs = previous.npcs;
}
inline constexpr std::array SettingsSchema{
    qol::core::settings::Boolean<Settings, &Settings::boostUiMessages>("boost_ui_messages"),
    qol::core::settings::Boolean<Settings, &Settings::boostUnnamedMessages>("boost_unnamed_messages"),
    qol::core::settings::Number<Settings, &Settings::baseSpeed, ParseSpeed>("base_speed"),
    qol::core::settings::Number<Settings, &Settings::boostSpeed, ParseSpeed>("boost_speed"),
    qol::core::settings::Field<Settings>{"boost_npc_list", ApplyNpcList, RestoreNpcList}
};
static_assert(qol::core::settings::ValidSchema(SettingsSchema));
inline unsigned ApplySettings(std::string_view input, Settings &output) {
    return qol::core::settings::Apply(input, output, SettingsSchema);
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
void ReportSettings();
bool Install();
} // namespace qol::dialogue
