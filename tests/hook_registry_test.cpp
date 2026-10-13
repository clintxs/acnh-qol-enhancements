#include "program/core/hook_registry.hpp"
#include "program/core/feature_activation.hpp"
#include <cassert>
#include <iostream>
using namespace qol::core;
int main() {
    std::array<std::uint32_t, 20> code{};
    const HookSite site{"shared.input", 123, 0, {1,2,3,4,5}};
    std::copy(site.original.begin(), site.original.end(), code.begin());
    const qol::native::ExecutableView text(reinterpret_cast<std::uintptr_t>(code.data()), sizeof(code));
    HookRegistry<2> registry;
    auto first = registry.Prepare(site, text);
    assert(first && !registry.Installed(first.token));
    auto second = registry.Prepare(site, text);
    assert(second);
    auto foreign = site;
    foreign.callback = 456;
    assert(registry.Prepare(foreign, text).error == ClaimError::Conflict);
    foreign = site; foreign.owner = "foreign";
    assert(registry.Prepare(foreign, text).error == ClaimError::Conflict);
    foreign = site; foreign.offset = 4;
    assert(registry.Prepare(foreign, text).error == ClaimError::Conflict);
    auto published = site.original;
    published[0] = 0x14000001;
    code[0] = published[0];
    assert(registry.Verify(first.token, text) == ClaimError::Changed);
    assert(registry.Published(first.token, published) == ClaimError::None);
    assert(registry.Verify(first.token, text) == ClaimError::None);
    registry.Release(second.token);
    registry.Release(first.token);
    assert(registry.Owned(site, text));
    code[0] = 0xdeadbeef;
    assert(registry.Prepare(site, text).error == ClaimError::Changed);
    code[0] = site.original[0]; // Restoring pristine code cannot hide foreign removal.
    assert(registry.Prepare(site, text).error == ClaimError::Changed);
    code[0] = published[0];
    HookSite adjacent{"next", 789, 20, {0,0,0,0,0}};
    auto reserved = registry.Prepare(adjacent, text);
    assert(reserved);
    auto third = adjacent; third.offset = 40; third.callback = 987;
    assert(registry.Prepare(third, text).error == ClaimError::Capacity);
    registry.Release(reserved.token);
    assert(registry.Verify(reserved.token, text) == ClaimError::Invalid);
    auto reused = registry.Prepare(third, text);
    assert(reused && registry.Verify(reserved.token, text) == ClaimError::Invalid);
    registry.Release(reused.token);
    assert(registry.Prepare(adjacent, qol::native::ExecutableView(reinterpret_cast<std::uintptr_t>(code.data()), 39)).error == ClaimError::Unsupported);
    auto invalid = site; invalid.callback = 0;
    assert(registry.Prepare(invalid, text).error == ClaimError::Invalid);

    FeatureResults<2> results;
    FeatureDescription feature{"consumer", true, nullptr, {"service"}};
    assert(!results.DependenciesReady(feature));
    assert(results.Store("service", {FeatureState::Disabled, false, "off"}));
    assert(!results.DependenciesReady(feature));
    assert(results.Store("service", {FeatureState::Ready, true, "installed"}));
    assert(results.DependenciesReady(feature));
    assert(results.Store("consumer", {FeatureState::DependencyUnavailable, true, "partial"}));
    assert(!results.Find("consumer")->Complete() && results.Find("consumer")->published);
    assert(!results.Store("overflow", {}));
    std::cout << "PASS: hook ownership, shared reuse, foreign changes/removal, overlap, capacity, cancellation, stale claims and feature dependencies\n";
}
