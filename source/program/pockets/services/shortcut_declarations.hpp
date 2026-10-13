#pragma once
#include "pocket_commands.hpp"
#include <array>
namespace qol_pockets::commands {
enum class DeclarationMatch { Empty, Identical, Conflict };
// Owned startup declarations. No runtime rebinding; comparison preserves order.
class ShortcutDeclarations {
    std::array<ShortcutRule, 3> values_{};
    unsigned count_{};
public:
    std::span<const ShortcutRule> Rules() const { return {values_.data(), count_}; }
    DeclarationMatch Compare(std::span<const ShortcutRule> input) const {
        if (!count_) return DeclarationMatch::Empty;
        if (input.size() != count_) return DeclarationMatch::Conflict;
        for (unsigned i = 0; i < count_; ++i)
            if (input[i].button != values_[i].button || input[i].action != values_[i].action)
                return DeclarationMatch::Conflict;
        return DeclarationMatch::Identical;
    }
    bool Own(std::span<const ShortcutRule> input) {
        if (!ValidShortcuts(input) || Compare(input) == DeclarationMatch::Conflict) return false;
        for (unsigned i = 0; i < input.size(); ++i) values_[i] = input[i];
        count_ = input.size();
        return true;
    }
};
}
