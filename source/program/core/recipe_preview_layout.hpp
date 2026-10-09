#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace qol::core::recipe_layout {
inline constexpr char SurfaceName[] = "N_QolPreview_00";
inline constexpr char CaptureName[] = "N_QolCapture_00";
inline constexpr char PictureName[] = "P_QolCapture_00";
inline constexpr std::size_t MaximumBytes = 65536;

template<class T> T Read(const std::byte *p, std::size_t o) {
    T value; std::memcpy(&value, p + o, sizeof(value)); return value;
}
template<class T> void Write(std::byte *p, std::size_t o, T value) {
    std::memcpy(p + o, &value, sizeof(value));
}
inline bool Tag(const std::byte *p, const char *tag) { return !std::memcmp(p, tag, 4); }
inline std::size_t Align(std::size_t n) { return (n + 3) & ~std::size_t(3); }
inline void Section(std::byte *p, const char *tag, unsigned size) {
    std::memset(p, 0, size); std::memcpy(p, tag, 4); Write(p, 4, size);
}
inline void Pane(std::byte *p, const char *tag, unsigned size, const char *name,
                 unsigned char flags, float width, float height) {
    Section(p, tag, size); p[8] = std::byte(flags); p[10] = std::byte(255);
    std::memcpy(p + 12, name, std::strlen(name) + 1);
    Write(p, 68, 1.0f); Write(p, 72, 1.0f); Write(p, 76, width); Write(p, 80, height);
}
// Entries in usd1 use offsets relative to each 12-byte entry, rather than
// to the section. Strings include their terminator; integer values are s32.
inline unsigned UserData(std::byte *p, const char *stringKey, const char *value,
                         const char *integerKey, int integer) {
    const unsigned valueLength = std::strlen(value) + 1;
    const unsigned keyLength = std::strlen(stringKey) + 1;
    const unsigned integerKeyLength = std::strlen(integerKey) + 1;
    const unsigned size = Align(40 + valueLength + keyLength + integerKeyLength);
    Section(p, "usd1", size); Write<std::uint16_t>(p, 8, 2);
    Write(p, 12, 28u + valueLength); Write(p, 16, 28u);
    Write<std::uint16_t>(p, 20, valueLength);
    Write(p, 24, 16u + valueLength + keyLength); Write(p, 28, 12u);
    Write<std::uint16_t>(p, 32, 1); p[34] = std::byte(1);
    Write(p, 36, integer);
    std::memcpy(p + 40, value, valueLength);
    std::memcpy(p + 40 + valueLength, stringKey, keyLength);
    std::memcpy(p + 40 + valueLength + keyLength, integerKey, integerKeyLength);
    return size;
}
inline unsigned Capture(std::byte *p) {
    Pane(p, "pan1", 84, CaptureName, 2, 850, 750);
    Write(p, 44, -430.0f); Write(p, 48, -64.0f);
    return 84 + UserData(p + 84, "DynamicCaptureOn", "R10G10B10A2", "CaptureBGAlpha", 255);
}
inline unsigned Surface(std::byte *p, unsigned material) {
    unsigned used = 0;
    Pane(p, "pan1", 84, SurfaceName, 2, 850, 750); used += 84;
    Write(p, 44, -430.0f); Write(p, 48, -64.0f);
    Section(p + used, "pas1", 8); used += 8;
    Pane(p + used, "pic1", 136, PictureName, 1, 850, 750);
    std::memset(p + used + 84, 255, 16);
    Write<std::uint16_t>(p + used, 100, material); p[used + 102] = std::byte(1);
    Write(p + used, 112, 1.0f); Write(p + used, 124, 1.0f);
    Write(p + used, 128, 1.0f); Write(p + used, 132, 1.0f); used += 136;
    used += UserData(p + used, "DynamicCaptureUseName", CaptureName, "DynamicCaptureUseIndex", 0);
    Section(p + used, "pae1", 8); return used + 8;
}

// Add a capture pair to the loaded MenuRecipe resource. Native 03A2E5D0 copies
// the framebuffer at capture draw time. Match WorkBenchInfo: capture as the
// first root child, before UI backgrounds cover the model; display its picture
// after the original panes. The two siblings have independent visibility.
// All existing panes,
// animations, materials and texture indices retain their original values,
// including changes supplied by aspect-ratio mods. No workbench asset is loaded.
// The caller must keep output alive for the entire native layout lifetime.
inline std::size_t AddSurface(std::span<const std::byte> input, std::span<std::byte> output) {
    const auto *data = input.data();
    if (input.size() < 20 || input.size() > MaximumBytes - 1024 ||
        std::memcmp(data, "FLYT\xff\xfe\x14\0", 8) || Read<unsigned>(data, 12) != input.size()) return 0;
    unsigned sections = 0, depth = 0, rootStart = 0, rootEnd = 0, textures = 0, materials = 0;
    bool menu = false, root = false, textureTable = false, materialTable = false;
    for (std::size_t at = 20; at < input.size();) {
        if (input.size() - at < 8) return 0;
        const auto *s = data + at; const auto size = Read<unsigned>(s, 4);
        if (size < 8 || size % 4 || size > input.size() - at) return 0;
        if (Tag(s, "lyt1")) menu = size >= 39 && !std::memcmp(s + 28, "MenuRecipe\0", 11);
        if (Tag(s, "pan1") || Tag(s, "pic1") || Tag(s, "txt1") || Tag(s, "prt1") || Tag(s, "wnd1")) {
            if (size < 84 || !std::memchr(s + 12, 0, 24)) return 0;
            if (!std::strcmp(reinterpret_cast<const char *>(s + 12), SurfaceName)) return 0;
            if (!std::strcmp(reinterpret_cast<const char *>(s + 12), "RootPane")) root = true;
        }
        if (Tag(s, "pas1")) {
            if (!depth && root && !rootStart) rootStart = at + size;
            ++depth;
        }
        if (Tag(s, "pae1")) {
            if (!depth) return 0;
            if (!--depth) rootEnd = at;
        }
        if (Tag(s, "txl1") || Tag(s, "mat1")) {
            if (size < 12) return 0;
            const unsigned count = Read<std::uint16_t>(s, 8);
            if (!count || count > 1024 || size < 12 + 4 * count) return 0;
            const bool texture = Tag(s, "txl1");
            if (texture ? textureTable : materialTable) return 0;
            if (texture) { textureTable = true; textures = count; }
            else { materialTable = true; materials = count; }
            for (unsigned i = 0; i < count; ++i) {
                const auto offset = Read<unsigned>(s, 12 + i * 4) + (texture ? 12 : 0);
                if (offset < 12 + count * 4 || offset >= size) return 0;
                if (texture && !std::memchr(s + offset, 0, size - offset)) return 0;
            }
        }
        at += size; ++sections;
    }
    if (!menu || !root || !rootStart || !rootEnd || depth || !textureTable || !materialTable ||
        sections != Read<std::uint16_t>(data, 16) || output.size() < input.size() + 1024) return 0;
    auto *dest = output.data(); std::memcpy(dest, data, 20); unsigned used = 20;
    for (std::size_t at = 20; at < input.size();) {
        if (at == rootStart) used += Capture(dest + used);
        if (at == rootEnd) used += Surface(dest + used, materials);
        const auto *s = data + at; const unsigned size = Read<unsigned>(s, 4);
        auto *target = dest + used;
        if (Tag(s, "txl1") || Tag(s, "mat1")) {
            const bool texture = Tag(s, "txl1"); const unsigned count = texture ? textures : materials;
            const unsigned tableEnd = 12 + count * 4;
            const unsigned newSize = texture ? Align(size + 4 + 10) : size + 4 + 84;
            Section(target, texture ? "txl1" : "mat1", newSize);
            Write<std::uint16_t>(target, 8, count + 1);
            for (unsigned i = 0; i < count; ++i) Write(target, 12 + i * 4, Read<unsigned>(s, 12 + i * 4) + 4);
            Write(target, tableEnd, size + 4 - (texture ? 12 : 0));
            std::memcpy(target + tableEnd + 4, s + tableEnd, size - tableEnd);
            auto *record = target + size + 4;
            if (texture) std::memcpy(record, "Capture^t", 10);
            else {
                std::memcpy(record, PictureName, sizeof(PictureName));
                // One texture, one transform, texture-only TEV stage; native
                // capture material flags verified in ACNH's v9 layout format.
                Write(record, 28, 0x15u); Write(record, 32, 0x08040200u); Write(record, 36, 0u); Write(record, 40, 0xffffffffu);
                Write<std::uint16_t>(record, 44, textures); record[46] = std::byte(4); record[47] = std::byte(4);
                Write(record, 60, 1.0f); Write(record, 64, 1.0f);
            }
            used += newSize;
        } else { std::memcpy(target, s, size); used += size; }
        at += size;
    }
    Write(dest, 12, used); Write<std::uint16_t>(dest, 16, sections + 7);
    return used;
}
} // namespace qol::core::recipe_layout
