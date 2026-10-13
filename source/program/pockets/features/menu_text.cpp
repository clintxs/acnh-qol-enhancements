#include "program/core/hook_plan.hpp"
#include "menu_text.hpp"
#include "stack_menu.hpp"
#include "program/pockets/game/stack_bindings.hpp"
#include "program/pockets/text/catalog.hpp"
#include <nn/fs/fs_files.hpp>
#include <cstring>

namespace qol_pockets::features {
namespace {
constexpr std::uintptr_t CaptionLoaded = 0x01a9bfa0;
constexpr std::uintptr_t GetActiveLocale = 0x037cefc4;
// ACNH mounts its RomFS as "content" (MountRom call at 0x037b5460).
constexpr char CatalogPath[] = "content:/qol-enhancements/messages/pockets.ini";
// Saved native name-buffer layout at CaptionLoaded. Sizes are UTF-16 units.
namespace layout {
constexpr std::size_t RowAction = 0x168, BufferBelowFrame = 0xf0;
constexpr std::size_t BufferText = 8, BufferCapacity = 0x10;
constexpr std::size_t NativeCapacityUnits = 0x800;
} // namespace layout
text::Catalog Messages;
bool CatalogAttempted = false;
char FileBuffer[text::MaxFileBytes];

void LoadMessages() {
    // Called only by the inventory UI, after RomFS and game language are ready.
    // Read once per process. No per-frame I/O, heap allocation, or retained handle.
    if (CatalogAttempted) {
        return;
    }
    CatalogAttempted = true;
    nn::fs::FileHandle file{};
    if (nn::fs::OpenFile(&file, CatalogPath, nn::fs::OpenMode_Read) != 0) {
        Logging.Warning("[QoL Enhancements/Pockets] TEXT_FALLBACK catalog unavailable; using English\n");
        return;
    }
    long size = 0;
    bool valid = nn::fs::GetFileSize(&size, file) == 0 && size > 0 &&
                 static_cast<unsigned long>(size) <= sizeof(FileBuffer);
    if (valid) {
        valid = nn::fs::ReadFile(file, 0, FileBuffer, static_cast<unsigned long>(size)) == 0;
    }
    nn::fs::CloseFile(file);
    if (valid) {
        valid = Messages.Load({FileBuffer, static_cast<std::size_t>(size)});
    }
    if (valid) Logging.Debug("[QoL Enhancements/Pockets] TEXT_READY mod-owned messages.ini loaded\n");
    else Logging.Warning("[QoL Enhancements/Pockets] TEXT_FALLBACK invalid catalog; using English\n");
}

HOOK_DEFINE_INLINE(ReplaceMenuCaption) {
    static void Callback(exl::hook::InlineCtx *registers) {
        // Verified in 0x01a9be60: x19 is the destination row, with action at
        // +0x168. FP-0xf0 is the allocated UTF-16 message-buffer object:
        // vtable +0, data +8, capacity (UTF-16 units) +0x10.
        const auto action = *reinterpret_cast<const int *>(registers->X[19] + layout::RowAction);
        const auto quantity = QuantityForAction(action);
        if (!quantity) {
            return;
        }
        LoadMessages();
        const auto locale = game::FunctionAt<unsigned (*)()>(GetActiveLocale)();
        const auto buffer = registers->X[29] - layout::BufferBelowFrame;
        auto *data = *reinterpret_cast<char16_t **>(buffer + layout::BufferText);
        const auto capacity =
            *reinterpret_cast<const std::uint32_t *>(buffer + layout::BufferCapacity);
        // Game allocates 0x800 units here; reject unexpected capacity/layout.
        if (capacity != layout::NativeCapacityUnits) {
            return;
        }
        text::CopyCaption(
            data, capacity,
            Messages.Text(quantity == 5 ? text::GrabFiveKey : text::GrabTenKey, locale));
        // Original layout parsing and rendering continue with our plain text.
    }
};
} // namespace

const char16_t *PocketMessage(std::string_view key) {
    LoadMessages();
    return Messages.Text(key, game::FunctionAt<unsigned (*)()>(GetActiveLocale)());
}

bool ValidateMenuText() {
    constexpr std::array<std::uint32_t, 5> expected{0x91002280, 0x2a1f03e1, 0x52803c02, 0x9491a129,
                                                    0xf00180bc};
    constexpr std::array<std::uint32_t, 5> localeExpected{0xa9be7bfd, 0xf9000bf3, 0x910003fd,
                                                          0xb00110c8, 0xf9436108};
    return std::memcmp(reinterpret_cast<const void *>(game::MainAddress(CaptionLoaded)),
                       expected.data(), sizeof(expected)) == 0 &&
           std::memcmp(reinterpret_cast<const void *>(game::MainAddress(GetActiveLocale)),
                       localeExpected.data(), sizeof(localeExpected)) == 0;
}
bool InstallMenuText() {

    qol::core::HookPlan hookPlan{
        qol::core::Site<ReplaceMenuCaption>("ReplaceMenuCaption", CaptionLoaded, {0x91002280, 0x2a1f03e1, 0x52803c02, 0x9491a129, 0xf00180bc}, true),
    };
    if (!hookPlan) return false;
    if (!hookPlan.Publish<ReplaceMenuCaption>(CaptionLoaded)) return false;

    return true;
}
} // namespace qol_pockets::features
