#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace qol::native {
inline bool ContainsRange(std::uintptr_t start, std::size_t size,
                          std::uintptr_t offset, std::size_t bytes) {
    constexpr auto max = std::numeric_limits<std::uintptr_t>::max();
    return start != 0 && offset <= size && bytes <= size - offset &&
           offset <= max - start &&
           (bytes == 0 || bytes - 1 <= max - (start + offset));
}
// Offsets here are relative to an executable's text start, never object/save fields.
enum class CodeStatus { Match, OutOfRange, Mismatch };
struct CodeResult {
    const char *binding{};
    std::uintptr_t offset{};
    CodeStatus status{CodeStatus::OutOfRange};
    std::array<std::uint32_t, 5> expected{}, actual{};
    std::size_t wordCount{};
    bool actualReadable{};
    explicit operator bool() const { return status == CodeStatus::Match; }
};
template<std::size_t N = 5> struct Fingerprint {
    std::uintptr_t offset;
    std::array<std::uint32_t, N> words;
    const char *name;
};

// The caller supplies a mapped, readable segment from module discovery. Bounds
// checks cannot prove mapping/liveness for arbitrary pointers. No allocations.
class ExecutableView {
    std::uintptr_t start_;
    std::size_t size_;
public:
    ExecutableView(std::uintptr_t start, std::size_t size) : start_(start), size_(size) {}
    bool Contains(std::uintptr_t offset, std::size_t bytes) const {
        return ContainsRange(start_, size_, offset, bytes);
    }
    template<std::size_t N>
    CodeResult Check(const Fingerprint<N> &fingerprint) const {
        static_assert(N > 0 && N <= 5);
        CodeResult result{.binding = fingerprint.name, .offset = fingerprint.offset,
                          .wordCount = N};
        for (std::size_t i = 0; i < N; ++i) result.expected[i] = fingerprint.words[i];
        if (!Contains(fingerprint.offset, N * sizeof(std::uint32_t))) return result;
        // memcpy supports unaligned text views without aliasing/alignment UB.
        std::memcpy(result.actual.data(), reinterpret_cast<const void *>(start_ + fingerprint.offset),
                    N * sizeof(std::uint32_t));
        result.actualReadable = true;
        result.status = CodeStatus::Match;
        for (std::size_t i = 0; i < N; ++i)
            if (result.actual[i] != result.expected[i]) result.status = CodeStatus::Mismatch;
        return result;
    }
};

// Internal ABI descriptor. Only adapters resolve this after their installation
// guards pass. It does not validate code, objects or calling context by itself.
template<class Function> struct FunctionBinding {
    static_assert(std::is_pointer_v<Function> &&
                  std::is_function_v<std::remove_pointer_t<Function>>);
    const char *name;
    std::uintptr_t offset;
    Function Resolve(std::uintptr_t textStart) const {
        return reinterpret_cast<Function>(textStart + offset);
    }
};
} // namespace qol::native
