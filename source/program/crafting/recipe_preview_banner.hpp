#pragma once
#include <array>
#include <cstddef>
#include <cstring>

namespace qol::crafting::recipe_banner {
// ui2d::Pane: confirmed by 0027BFB4/0027BFE4 (resource geometry),
// 0027D550 (matrix dirty bit) and 0027E208 (ordered child traversal).
namespace field {
inline constexpr unsigned Sibling = 8, Parent = 0x18, Children = 0x20;
inline constexpr unsigned Translation = 0x30, Rotation = 0x3c, Scale = 0x48;
inline constexpr unsigned Size = 0x50, Flags = 0x58, Origin = 0x5b;
}
inline constexpr float Inset = 16.0f;
template<class T> T &At(void *object, unsigned offset = 0) {
    return *reinterpret_cast<T *>(static_cast<std::byte *>(object) + offset);
}
inline void *Offset(void *object, unsigned offset) {
    return static_cast<std::byte *>(object) + offset;
}
inline bool Linked(void *node) {
    auto previous = At<void *>(node), next = At<void *>(node, 8);
    return previous && next && At<void *>(previous, 8) == node && At<void *>(next) == node;
}
inline bool MoveBefore(void *pane, void *parent, void *before) {
    auto node = Offset(pane, field::Sibling);
    if (!parent || !before || before == node || !Linked(node) || !Linked(before)) return false;
    if (before != Offset(parent, field::Children) &&
        At<void *>(static_cast<std::byte *>(before) - field::Sibling, field::Parent) != parent) return false;
    auto previous = At<void *>(node), next = At<void *>(node, 8);
    At<void *>(previous, 8) = next; At<void *>(next) = previous;
    previous = At<void *>(before);
    At<void *>(node) = previous; At<void *>(node, 8) = before;
    At<void *>(previous, 8) = node; At<void *>(before) = node;
    At<void *>(pane, field::Parent) = parent;
    At<unsigned char>(pane, field::Flags) |= 0x10; // Recalculate this subtree's matrices.
    return true;
}
struct Placement {
    void *pane{}, *background{}, *savedParent{}, *savedNext{};
    std::array<float, 3> savedTranslation{};
};
inline bool Restore(Placement &binding) {
    if (!binding.savedParent) return true;
    if (!MoveBefore(binding.pane, binding.savedParent, binding.savedNext)) return false;
    std::memcpy(Offset(binding.pane, field::Translation), binding.savedTranslation.data(), sizeof(binding.savedTranslation));
    binding.savedParent = binding.savedNext = nullptr;
    return true;
}
inline bool Place(Placement &binding, void *surface) {
    if (!binding.pane || !binding.background || !surface || binding.savedParent) return false;
    // Work from the native, dynamically sized/localized banner background.
    // Its origin can be left/center/right and top/center/bottom. Account for
    // the alignment containers and scales rather than guessing a text width.
    auto base = binding.background;
    const unsigned origin = At<unsigned char>(base, field::Origin);
    const unsigned horizontal = origin & 3, vertical = (origin >> 2) & 3;
    if (horizontal > 2 || vertical > 2) return false;
    const float width = At<float>(base, field::Size), height = At<float>(base, field::Size + 4);
    if (!(width > 0 && width < 16384 && height > 0 && height < 16384)) return false;
    float right = width * (horizontal == 0 ? 0.5f : horizontal == 1 ? 1.0f : 0.0f);
    float bottom = height * (vertical == 0 ? -0.5f : vertical == 1 ? -1.0f : 0.0f);
    bool reached = false;
    for (unsigned depth = 0; base && depth < 12; ++depth, base = At<void *>(base, field::Parent)) {
        const float sx = At<float>(base, field::Scale), sy = At<float>(base, field::Scale + 4);
        if (!(sx > 0 && sx <= 16 && sy > 0 && sy <= 16)) return false;
        for (unsigned axis = 0; axis < 3; ++axis)
            if (At<float>(base, field::Rotation + axis * 4) != 0) return false;
        right *= sx; bottom *= sy;
        if (base == binding.pane) { reached = true; break; }
        const float x = At<float>(base, field::Translation), y = At<float>(base, field::Translation + 4);
        if (!(x >= -16384 && x <= 16384 && y >= -16384 && y <= 16384)) return false;
        right += x; bottom += y;
    }
    const float surfaceWidth = At<float>(surface, field::Size), surfaceHeight = At<float>(surface, field::Size + 4);
    if (!reached || !(surfaceWidth > Inset * 2 && surfaceWidth < 16384 &&
                      surfaceHeight > Inset * 2 && surfaceHeight < 16384)) return false;
    const auto parent = At<void *>(binding.pane, field::Parent);
    const auto next = At<void *>(binding.pane, field::Sibling + 8);
    std::memcpy(binding.savedTranslation.data(), Offset(binding.pane, field::Translation), sizeof(binding.savedTranslation));
    // The surface's existing picture stays first; the native banner is last.
    // No copied text, visibility override or second Hotel count is introduced.
    if (!parent || !MoveBefore(binding.pane, surface, Offset(surface, field::Children))) return false;
    binding.savedParent = parent; binding.savedNext = next;
    At<float>(binding.pane, field::Translation) = surfaceWidth * 0.5f - Inset - right;
    At<float>(binding.pane, field::Translation + 4) = -surfaceHeight * 0.5f + Inset - bottom;
    return true;
}
} // namespace qol::crafting::recipe_banner
