#include "pocket_input_probe.hpp"
#include "program/pockets/game/stack_bindings.hpp"
#include <array>
#include <cstring>

namespace qol_pockets::features {
namespace {
constexpr std::uintptr_t PageInputUpdate = 0x03c58d24;
constexpr std::uintptr_t PocketPageVtable = 0x04a06b00;

template <typename T> const T &Read(const void *object, std::size_t offset) {
    return *reinterpret_cast<const T *>(static_cast<const std::byte *>(object) + offset);
}

// This is the UI snapshot, NOT nn::hid's button bitset. In particular, the UI
// reserves bits 0..3 for navigation. Measure Y rather than guessing its bit.
// 0x03c4ad70 clears and fills these 0x48-byte records; 0x03c4b20c consumes
// held (+0) and already-consumed (+0x3c) bits for the existing button bindings.
struct InputRow {
    std::uint32_t held{};
    std::byte unknown[0x38]{};
    std::uint32_t consumed{};
    std::byte tail[8]{};
};
static_assert(sizeof(InputRow) == 0x48 && offsetof(InputRow, consumed) == 0x3c);

HOOK_DEFINE_TRAMPOLINE(ProbePocketInput) {
    static void Callback(void *page, void *snapshot, unsigned blocked) {
        if (Read<std::uintptr_t>(page, 0) == game::MainAddress(PocketPageVtable) && snapshot) {
            const auto count = Read<int>(snapshot, 8);
            const auto *rows = Read<const InputRow *>(snapshot, 0x10);
            if (count > 0 && count <= 8 && rows) {
                static const void *lastPage{};
                static std::array<std::uint32_t, 8> lastHeld{};
                static unsigned seen{};
                static unsigned reports{};
                if (lastPage != page) {
                    lastPage = page;
                    seen = 0;
                }
                for (int index = 0; index < count; ++index) {
                    const unsigned bit = 1u << index;
                    if ((seen & bit) && lastHeld[index] == rows[index].held) {
                        continue;
                    }
                    seen |= bit;
                    lastHeld[index] = rows[index].held;
                    // Bound the log, even if a controller reports noisy input.
                    if (reports < 128) {
                        ++reports;
                        const auto *input = Read<const void *>(page, 0x210);
                        const unsigned localBlock = input ? Read<std::uint8_t>(input, 0x54) : 1;
                        Logging.Log(
                            "[QoL Enhancements/Pockets] POCKET_INPUT row=%d held=%08x consumed=%08x "
                            "blocked=%u local=%u\n",
                            index, rows[index].held, rows[index].consumed, blocked & 1, localBlock);
                    }
                }
            }
        }
        // Always call the original, with the original arguments. No controller,
        // input, menu, inventory, or storage fields are written by this probe.
        Orig(page, snapshot, blocked);
    }
};
} // namespace

bool InstallPocketInputProbe() {
    constexpr std::array<std::uint32_t, 5> expected{0xf9410800, 0xb4000060, 0x12000042, 0x17ffcad4,
                                                    0xd65f03c0};
    if (std::memcmp(reinterpret_cast<const void *>(game::MainAddress(PageInputUpdate)),
                    expected.data(), sizeof(expected)) != 0) {
        Logging.Log("[QoL Enhancements/Pockets] POCKET_INPUT_DISABLED code mismatch\n");
        return false;
    }
    ProbePocketInput::InstallAtOffset(PageInputUpdate);
    Logging.Log(
        "[QoL Enhancements/Pockets] POCKET_INPUT_READY read-only probe; Y-to-store not enabled\n");
    return true;
}
} // namespace qol_pockets::features
