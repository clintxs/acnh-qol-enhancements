#include "program/crafting/game/preview_publication.hpp"
#include <array>
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
using qol::crafting::preview_internal::PublicationClock;
int main() {
    PublicationClock clock;
    std::atomic<unsigned> first{}, second{};
    auto load = [&] { return std::array{first.load(), second.load()}; };
    {
        PublicationClock::Write write{clock};
        first.store(1);
        assert(!clock.Read(load)); // Never expose an in-progress payload.
        second.store(1);
    }
    assert(clock.Read(load)->at(0) == 1);
    const auto interrupted = clock.Read([&] {
        const auto previous = first.load();
        { PublicationClock::Write write{clock}; first.store(2); second.store(2); }
        return std::array{previous, second.load()};
    });
    assert(!interrupted); // A completed write during the read is rejected too.
    std::atomic<bool> start{}, done{};
    std::atomic<unsigned> mismatches{};
    std::thread writer([&] {
        while (!start.load()) {}
        for (unsigned value = 3; value <= 100000; ++value) {
            PublicationClock::Write write{clock};
            first.store(value); second.store(value);
        }
        done.store(true);
    });
    std::thread reader([&] {
        start.store(true);
        while (!done.load()) {
            const auto value = clock.Read(load);
            if (value && (*value)[0] != (*value)[1]) ++mismatches;
        }
    });
    writer.join(); reader.join();
    assert(!mismatches.load());
    assert(clock.Read(load) == std::optional(std::array{100000u,100000u}));
    std::cout << "PASS: bounded coherent publication rejects active/interleaved writes and never accepts mixed atomic payloads under concurrent UI/reader stress\n";
}
