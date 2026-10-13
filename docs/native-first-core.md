# Core: module assembly and amiibo scan timeout

This is the final module pass on `refactor/semantic-feature-api`, starting from
accepted Shopping Card commit `bd002fdfe420118fc4031cc2e2458b123898094e`.

## Read in this order

1. [Core feature assembly](../source/program/core/features/core_features.cpp):
   `ActivateCoreFeatures` shows when the amiibo initialization override is enabled.
   `Modules` lists the seven optional modules, their minimum package versions,
   installers, configuration/reporting functions and resource dependencies.
   Catalog's individual features and their profile dependency are visible above
   the table. This is assembly policy; each module's actual behavior remains in
   its own feature implementation.
2. [Core settings](../source/program/core/settings.hpp): default timeout 9,000,
   accepted unsigned halfword range 0..65,535, zero retaining native behavior,
   and logging preferences. Existing shared schema parsing remains appropriate
   here; no new parser or settings framework was introduced.
3. [Native operations](../source/program/core/native/core_operations.hpp) and
   [native implementation](../source/program/core/native/core_native.cpp):
   Catalog profile validation and the verified amiibo initialization hook.
4. [Module loading](../source/program/core/modules.cpp) and
   [bootstrap](../source/program/bootstrap.cpp): post-content-mount timing,
   once-only activation, optional Core settings overlays, required feature
   markers, resource preparation, compatibility checks and status reporting.
   Shared activation, hook ownership, input, captions and resource budgets keep
   their existing service boundaries.

## Improvements made

Core was already small and reused the native amiibo countdown. The useful change
is a clear starting file for assembly and the actual enablement decision:

```cpp
RequestedScanTimeout = settings.amiiboTimeoutFrames;
return ActivateFeature({"core.amiibo_timeout", RequestedScanTimeout != 0,
                        InstallConfiguredScanTimeout});
```

The startup installer receives a copied setting after both Core configuration
layers are applied. Zero records a disabled feature and installs no scan hook;
nonzero requests the existing verified initialization override.

Resource dependency identity was previously inferred from a callback comparison:

```cpp
module.prepareResources == PrepareFittingResources
    ? "resources.fitting" : "resources.shopping";
```

The Fitting Room and Shopping Card declarations now explicitly supply
`resources.fitting` and `resources.shopping`. `ActivateModules` uses that declared
identity. Callback addresses no longer decide which dependency name is recorded.
The module list, activation order and identifiers are preserved.

## Description-to-code map

| Core behavior | Readable policy | Supported execution and limits |
| --- | --- | --- |
| Assemble seven optional modules | `Modules` and the small per-module feature lists in `core_features.cpp` | Existing module installers retain their own feature policies and native guards. Catalog consumers depend on `catalog.profile`. Package minimums and order are preserved. |
| Configurable amiibo scan duration | `ActivateCoreFeatures`, `Settings::amiiboTimeoutFrames`, `SettingsSchema` | `game::InstallAmiiboTimeout` validates the original instruction sequence and publishes through the existing HookPlan. The callback changes w8 after the native 450-update default load and before the original unsigned halfword store. |
| Native scan countdown and expiry | Existing native adapter evidence; no replacement policy | Native decrement/expiry at BC44B0..BC44BC remain authoritative. This does not extend other amiibo phases or add wall-clock scheduling. |
| Core configuration and logging | Existing typed settings/schema and logging preference policy | Regular Core settings followed by personal overlay; neither requires enabled/version metadata. Invalid or duplicate values retain prior settings. Warnings/errors remain mandatory. Loading/configuring/reporting mechanics remain in `modules.cpp`. |
| Installed replacement assets still load when behavior is disabled | Explicit resource IDs/callbacks in `Modules` | The loader prepares resources for a present package marker before rejecting invalid configuration or disabled gameplay. Existing monotonic allocation floors, shared ownership and partial-publication reporting remain. |
| Compatibility, independent failures and startup timing | Existing loader/activation services | Configs load after native content mounting. Atomic once-only startup, minimum/future version handling, hook/profile conflicts, dependency failure and activation results retain their existing semantics. No new lifecycle or rollback guarantee is introduced. |

## Ownership and failure paths

- Core settings are loaded once at post-mount startup. The feature copies the
  timeout value for the synchronous installer callback; the native adapter keeps
  its installed register value for later scan initialization. There is no runtime
  reconfiguration API or retained reference to the passed `Settings` object.
- A default/nonzero configuration installs the initialization hook only after
  compatibility/ownership checks. Zero installs none and leaves original
  initialization, countdown and expiry intact. The native adapter also retains
  its defensive range and zero checks for direct callers.
- A changed amiibo fingerprint disables that feature through the existing
  activation outcome; independent modules may still activate. Publication is
  not transactional: existing `published`/partial status remains meaningful.
- Module descriptions and activation identifiers have process lifetime. A module
  with resource preparation supplies its resource feature ID explicitly; other
  modules leave both absent. No dynamic registry or universal Game API was added.

## Verification and preservation

Before editing/building, the accepted ELF, NSO, NPDM and Core package were copied
to `dist/native-first-core/baseline`. All 3,298 pre-existing distribution files
were hashed in `preservation.json`. Candidate outputs, identities and logs are
kept under `dist/native-first-core`, outside Git and release archives.

Implementation/tests commit: `fdf775da2a0e32197c0313f5092257749ed32167`.

- Pinned normal Docker build passed; all diagnostics remain disabled.
- `QOL_BASELINE_ELF=dist/native-first-core/baseline/project.elf python3 tools/check.py core shopping fitting`
  completed successfully: 25 host suites and 22 focused ARM/resource entries.
  Core's existing host coverage includes all 65,536 accepted timeout settings,
  layered defaults, zero, invalid values, duplicates and atomic file failures.
- The new baseline replay retains all original Core assertions and compares 1,090
  production activation outcomes between independently loaded ELFs. Hook sites,
  feature/dependency IDs, states/publication/reasons, file open/close order and
  native/module vtable routing agree. This includes all 128 optional-module
  combinations at two load addresses, compatibility/configuration failures and
  repeated once-only activation.
- Installed amiibo callbacks and native store/decrement/expiry instructions pass
  for zero/native behavior, 1, 450, 9,000 and 65,535, with overlays, malformed
  files, old Amiibo markers and instruction conflicts. Registration is exercised
  through actual production bootstrap/activation, not assigned private state.
- Shared activation, hook ownership, logging, input and recipe-preview checks
  pass. Shopping Card's four inherited replays match 144,764 tracked native calls;
  Fitting Room's six match 21,925 native calls and 8,114 outcomes. Their resource,
  layout, texture, checkout, input, feedback and caption checks pass as well.
- Packaging/manifest checks passed without writing a release. A temporary source
  archive includes the Core feature/native files and excludes the superseded
  timeout path. The Core package, all 3,298 earlier distribution files, main and
  infrastructure refs remain unchanged. Final candidate copies match root outputs.
- No additional module-specific suites were rerun; the changes are Core assembly
  and resource identity, with all module combinations and both resource consumers
  covered above. No native feature behavior was corrected or expanded.

Live Eden amiibo scanning and normal startup remain playtest requirements.
Synthetic FS/hook/cache services and native ARM execution establish the documented
offline behavior, not actual NFC hardware or live rendering integration.
