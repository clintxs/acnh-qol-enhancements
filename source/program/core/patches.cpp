#include "lib.hpp"
#include "patches.hpp"
#include <cstring>

namespace qol::patches {
namespace {
template <std::size_t Count>
bool Install(const Patch (&patches)[Count], const char *name) {
    const auto &main = exl::util::GetMainModuleInfo();
    // Validate EVERY record before the first write, including helper padding.
    for (const auto &patch : patches) {
        const auto size = patch.words * sizeof(std::uint32_t);
        if (patch.offset + size > main.m_Text.m_Size ||
            std::memcmp(reinterpret_cast<const void *>(main.m_Text.m_Start + patch.offset),
                        patch.original.data(), size) != 0) {
            Logging.Log("[QoL Enhancements/%s] DISABLED: instruction mismatch at main+0x%lx\n", name, patch.offset);
            return false;
        }
    }
    for (const auto &patch : patches) {
        exl::patch::StreamPatcher writer(patch.offset);
        for (unsigned index = 0; index < patch.words; ++index) writer.Write(patch.replacement[index]);
    }
    Logging.Log("[QoL Enhancements/%s] READY: %u runtime patch records\n", name, static_cast<unsigned>(Count));
    return true;
}
} // namespace
bool InstallDialogue() { return Install(DialoguePatches, "Faster Dialogue"); }
bool InstallAmiibo() { return Install(AmiiboPatches, "Amiibo"); }
} // namespace qol::patches
