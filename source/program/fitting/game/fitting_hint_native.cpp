#include "program/core/hook_plan.hpp"
#include "lib.hpp"
#include "program/fitting/features/fitting_hint.hpp"
#include "program/fitting/features/fitting_features.hpp"
#include "program/core/hint_text.hpp"
#include "program/pockets/text/catalog.hpp"
#include <nn/fs/fs_files.hpp>
#include <array>
#include <cstring>

namespace able_bag {
namespace {
namespace address {
constexpr std::uintptr_t AppendFittingFooter = 0x03114f28;
constexpr std::uintptr_t AppendHint = 0x02de2b00;
constexpr std::uintptr_t GetActiveLocale = 0x037cefc4;
constexpr std::uintptr_t BufferedStringVtableGot = 0x04ab2bc0;
constexpr std::uintptr_t FittingVtable = 0x04a10620;
}
constexpr auto &CaptionKey = BagHintKey;
constexpr char MessagesPath[] = "content:/qol-enhancements/messages/fitting-room.ini";
constexpr int NativeX = 6, NativeY = 7;
struct SafeString { std::uintptr_t vtable{}; const char *text{}; };
struct HintDescription {
    int button = -1, secondary = -1, otherButton = -1, otherSecondary = -1;
    SafeString caption{};
    int capacity = 64;
    char key[64]{};
    std::uint32_t padding{};
    std::uint8_t player = 0xff, playerPadding[3]{};
    int appearance = 0;
    std::uint8_t alternate{}, alternatePadding[3]{};
    int alternateButton = -1, alternateSecondary = -1;
};
static_assert(sizeof(HintDescription) == 0x80);
static_assert(offsetof(HintDescription, key) == 0x24);
static_assert(offsetof(HintDescription, player) == 0x68);
std::uintptr_t Main(std::uintptr_t offset) { return exl::util::modules::GetTargetStart() + offset; }
template<class T> T &Field(std::uintptr_t pointer, unsigned offset = 0) {
    return *reinterpret_cast<T *>(pointer + offset);
}
template<class T> T Function(std::uintptr_t offset) { return reinterpret_cast<T>(Main(offset)); }
qol_pockets::text::Catalog Messages;
bool MessagesAttempted{};
char MessageFile[qol_pockets::text::MaxFileBytes];

const char16_t *BagCaption() {
    // The footer is built after content mounting. Read once, including failures.
    if (!MessagesAttempted) {
        MessagesAttempted = true;
        nn::fs::FileHandle file{};
        bool valid = false;
        if (nn::fs::OpenFile(&file, MessagesPath, nn::fs::OpenMode_Read) == 0) {
            long size = 0;
            valid = nn::fs::GetFileSize(&size, file) == 0 && size > 0 &&
                    static_cast<unsigned long>(size) <= sizeof(MessageFile);
            if (valid) valid = nn::fs::ReadFile(file, 0, MessageFile, size) == 0;
            nn::fs::CloseFile(file);
            if (valid) valid = Messages.Load({MessageFile, static_cast<std::size_t>(size)});
        }
        if (valid) Logging.Debug("[QoL Enhancements/Fitting Room] HINT_TEXT loaded\n");
        else Logging.Warning("[QoL Enhancements/Fitting Room] HINT_TEXT English fallback; caption file missing or invalid\n");
    }
    const auto locale = Function<unsigned (*)()>(address::GetActiveLocale)();
    const auto *caption = Messages.Text(CaptionKey, locale);
    return caption ? caption : u"Bag";
}

class NativeFooter : public game::Footer {
    std::uintptr_t footer, owner;
public:
    NativeFooter(std::uintptr_t footer, std::uintptr_t owner) : footer(footer), owner(owner) {}
    int Count() const override { return Field<int>(footer, 0x218); }
    int Capacity() const override { return Field<int>(footer, 0x21c); }
    bool Ready() const override {
        return Field<std::uintptr_t>(footer, 0x220) && Field<std::uintptr_t>(footer, 0x228);
    }
    bool Contains(const char *key) const override {
        const auto entries = Field<HintDescription **>(footer, 0x220);
        for (int i = 0; i < Count(); ++i) {
            if (entries[i] && entries[i]->caption.text &&
                !std::strcmp(entries[i]->caption.text, key)) return true;
        }
        return false;
    }
    void AppendYHint(const char *key) override {
        HintDescription hint;
        hint.button = NativeY;
        const auto player = Field<std::uint8_t>(owner, 0x1f8);
        hint.player = player < 8 ? player : 0xff;
        hint.caption = {Field<std::uintptr_t>(Main(address::BufferedStringVtableGot)) + 16, hint.key};
        // Copy only a bounded, terminated key into the native descriptor.
        if (!key) return;
        unsigned length = 0;
        while (length < sizeof(hint.key) && key[length]) ++length;
        if (length == sizeof(hint.key)) return;
        std::memcpy(hint.key, key, length + 1);
        Function<void (*)(std::uintptr_t, const HintDescription *, unsigned)>(address::AppendHint)(footer, &hint, 1);
    }
};

HOOK_DEFINE_INLINE(AppendBagFooterHint) {
    static void Callback(exl::hook::InlineCtx *registers) {
        // 03114f28 runs only when the native fitting footer is rebuilt, after
        // A/B/Purchase and before the optional ticket hint. x19 is the already
        // type-checked footer; x20 is still the fitting-room controller here.
        const auto footer = registers->X[19], owner = registers->X[20];
        if (!footer || !owner || Field<std::uintptr_t>(owner) != Main(address::FittingVtable)) return;
        RemapTailorTicketInput(owner);
        NativeFooter target(footer, owner);
        AppendBagHint(target);
    }
};


} // namespace

void RemapTailorTicketInput(std::uintptr_t owner) {
    if (!owner || Field<std::uintptr_t>(owner) != Main(address::FittingVtable)) return;
    const auto page = Field<std::uintptr_t>(owner, 0x210);
    const auto ticketButton = page ? Field<std::uintptr_t>(page, 0x580) : 0;
    const auto input = ticketButton ? Field<std::uintptr_t>(ticketButton, 0xa0) : 0;
    // Native dispatch 03c4b244..03c4b270 reads this bit index for both held
    // and consumed masks. Footer 03114f4c reads the same field for its glyph.
    // Preserve unrelated bindings and all native eligibility/repeat state.
    if (input && Field<int>(input, 0x14) == NativeY)
        Field<int>(input, 0x14) = NativeX;
}

bool ValidateFittingHint() {
    struct Check { std::uintptr_t address; std::array<std::uint32_t, 5> words; };
    constexpr Check checks[]{
        {address::AppendFittingFooter, {0xb9401fa8, 0x7100051f, 0x54000520, 0xf9410a89, 0xf942c129}},
        {address::AppendHint, {0xb941f008, 0x3100051f, 0xb941f409, 0xb941b00a, 0x3a410920}},
        {0x03c4b244, {0xb9401408, 0xb9400149, 0xb940140b, 0xb9403d4a, 0x1acb254a}},
        {address::GetActiveLocale, {0xa9be7bfd, 0xf9000bf3, 0x910003fd, 0xb00110c8, 0xf9436108}},
    };
    const auto &text = exl::util::GetMainModuleInfo().m_Text;
    for (const auto &check : checks) {
        if (check.address + sizeof(check.words) > text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(Main(check.address)), check.words.data(), sizeof(check.words))) {
            Logging.Error("[QoL Enhancements/Fitting Room] HINT_DISABLED fingerprint at %lx\n", check.address);
            return false;
        }
    }
    return true;
}
bool InstallFittingHint() {

    qol::core::HookPlan hookPlan{
        qol::core::Site<AppendBagFooterHint>("AppendBagFooterHint", address::AppendFittingFooter, {0xb9401fa8, 0x7100051f, 0x54000520, 0xf9410a89, 0xf942c129}, true),
    };
    if (!hookPlan) return false;
    if (!qol::core::RegisterCaption(qol::core::CaptionKey::Bag, BagCaption)) {
        Logging.Error("[QoL Enhancements/Fitting Room] HINT_DISABLED shared text unavailable\n");
        return false;
    }
    if (!hookPlan.Publish<AppendBagFooterHint>(address::AppendFittingFooter)) return false;
    Logging.Debug("[QoL Enhancements/Fitting Room] HINT_READY Y / Bag\n");

    return true;
}
} // namespace able_bag
