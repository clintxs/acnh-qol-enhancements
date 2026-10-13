#pragma once
#include <atomic>
#include <array>
#include <cstdint>
#include <optional>
namespace qol::crafting::preview_internal {
// One UI writer per clock. Scope only synchronous atomic payload stores, never
// native calls or deferred operations. Payload accesses must be sequentially
// consistent too: the common total order makes an unchanged even version a
// coherent snapshot, not just individually race-free fields.
class PublicationClock {
    std::atomic<std::uint64_t> version_{};
public:
    class Write {
        PublicationClock &clock_;
    public:
        explicit Write(PublicationClock &clock) : clock_(clock) { clock_.version_.fetch_add(1); }
        Write(const Write &) = delete;
        ~Write() { clock_.version_.fetch_add(1); }
    };
    template<class Load> auto Read(Load load) const -> std::optional<decltype(load())> {
        const auto before = version_.load();
        if (before & 1) return std::nullopt;
        auto value = load();
        if (version_.load() != before) return std::nullopt;
        return value;
    }
};
struct PublishedPose {
    unsigned handle, angleZ, originZ, epoch;
    std::uint64_t angleXY, originXY, item;
    bool requestedFixed;
    std::array<unsigned,3> Delta() const {
        return {unsigned(angleXY) - unsigned(originXY), unsigned(angleXY >> 32) - unsigned(originXY >> 32), angleZ - originZ};
    }
};
struct WorkbenchPoseSnapshot { void *controller; unsigned handle, mode, tilt; };
}
