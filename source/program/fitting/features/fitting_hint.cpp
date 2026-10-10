#include "lib.hpp"
#include "fitting_hint.hpp"
#include "program/pockets/text/catalog.hpp"
#include <nn/fs/fs_files.hpp>
#include <array>
#include <cstring>

namespace able_bag {
namespace {
namespace address {
constexpr std::uintptr_t AppendFittingFooter = 0x03114f28;
constexpr std::uintptr_t AppendHint = 0x02de2b00;
constexpr std::uintptr_t InitializeMessage = 0x01aea210;
constexpr std::uintptr_t GetActiveLocale = 0x037cefc4;
constexpr std::uintptr_t BufferedStringVtableGot = 0x04ab2bc0;
constexpr std::uintptr_t FittingVtable = 0x04a10620;
constexpr std::uintptr_t SetPaneText = 0x03c1b588, SetGroupText = 0x03c47b90;
}
constexpr char CaptionKey[] = "fitting_room.add_to_bag";
constexpr char MessagesPath[] = "content:/qol-enhancements/messages/fitting-room.ini";
constexpr int NativeY = 7;
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
        Logging.Log("[QoL Enhancements/Fitting Room] HINT_TEXT %s\n", valid ? "loaded" : "English fallback");
    }
    const auto locale = Function<unsigned (*)()>(address::GetActiveLocale)();
    const auto *caption = Messages.Text(CaptionKey, locale);
    return caption ? caption : u"Add to bag";
}

HOOK_DEFINE_INLINE(AppendBagFooterHint) {
    static void Callback(exl::hook::InlineCtx *registers) {
        // 03114f28 runs only when the native fitting footer is rebuilt, after
        // A/B/Purchase and before the optional ticket hint. x19 is the already
        // type-checked footer; x20 is still the fitting-room controller here.
        const auto footer = registers->X[19], owner = registers->X[20];
        if (!footer || !owner || Field<std::uintptr_t>(owner) != Main(address::FittingVtable)) return;
        const int count = Field<int>(footer, 0x218), capacity = Field<int>(footer, 0x21c);
        const auto entries = Field<HintDescription **>(footer, 0x220);
        // Leave one of the five visible slots for the native ticket hint.
        if (count < 0 || count >= 4 || count + 1 >= capacity || !entries ||
            !Field<std::uintptr_t>(footer, 0x228)) return;
        for (int i = 0; i < count; ++i) {
            if (entries[i] && entries[i]->caption.text &&
                !std::strcmp(entries[i]->caption.text, CaptionKey)) return;
        }
        HintDescription hint;
        hint.button = NativeY;
        const auto player = Field<std::uint8_t>(owner, 0x1f8);
        hint.player = player < 8 ? player : 0xff;
        hint.caption = {Field<std::uintptr_t>(Main(address::BufferedStringVtableGot)) + 16, hint.key};
        std::memcpy(hint.key, CaptionKey, sizeof(CaptionKey));
        Function<void (*)(std::uintptr_t, const HintDescription *, unsigned)>(address::AppendHint)(footer, &hint, 1);
    }
};

HOOK_DEFINE_TRAMPOLINE(InitializeBagCaption) {
    static void Callback(std::uintptr_t message, std::uintptr_t binding,
                         const SafeString *bank, const SafeString *key) {
        Orig(message, binding, bank, key);
        if (!message || !key || !key->text || std::strcmp(key->text, CaptionKey)) return;
        // Native initialization only copies bank/key and retains the binding.
        // State 12 bypasses subsequent message-bank resolution in 01aea9c8.
        // Its normal destructor still owns/cleans the initialized message.
        Field<unsigned>(message, 0xc) = 12;
        const auto adapter = binding ? Field<std::uintptr_t>(binding, 0x48) : 0;
        const auto methods = adapter ? Field<std::uintptr_t>(adapter, 0x18) : 0;
        const auto payload = adapter ? Field<std::uintptr_t>(adapter, 8) : 0;
        const auto setter = methods ? Field<std::uintptr_t>(methods, 0x5e0) : 0;
        if (!payload || (setter != Main(address::SetPaneText) && setter != Main(address::SetGroupText))) return;
        const auto *caption = BagCaption();
        unsigned length = 0;
        while (caption[length]) ++length;
        reinterpret_cast<void (*)(std::uintptr_t, const char16_t *, unsigned)>(setter)(payload, caption, length);
        // The footer's native layout pass measures the new text and positions
        // its controller glyph. No glyph/font edits or global MSBT replacement.
    }
};
} // namespace

bool ValidateFittingHint() {
    struct Check { std::uintptr_t address; std::array<std::uint32_t, 5> words; };
    constexpr Check checks[]{
        {address::AppendFittingFooter, {0xb9401fa8, 0x7100051f, 0x54000520, 0xf9410a89, 0xf942c129}},
        {address::AppendHint, {0xb941f008, 0x3100051f, 0xb941f409, 0xb941b00a, 0x3a410920}},
        {address::InitializeMessage, {0xa9bc7bfd, 0xa9015ff8, 0xa90257f6, 0xa9034ff4, 0x910003fd}},
        {address::GetActiveLocale, {0xa9be7bfd, 0xf9000bf3, 0x910003fd, 0xb00110c8, 0xf9436108}},
    };
    const auto &text = exl::util::GetMainModuleInfo().m_Text;
    for (const auto &check : checks) {
        if (check.address + sizeof(check.words) > text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(Main(check.address)), check.words.data(), sizeof(check.words))) {
            Logging.Log("[QoL Enhancements/Fitting Room] HINT_DISABLED fingerprint at %lx\n", check.address);
            return false;
        }
    }
    return true;
}
void InstallFittingHint() {
    AppendBagFooterHint::InstallAtOffset(address::AppendFittingFooter);
    InitializeBagCaption::InstallAtOffset(address::InitializeMessage);
    Logging.Log("[QoL Enhancements/Fitting Room] HINT_READY Y / Add to bag\n");
}
} // namespace able_bag
