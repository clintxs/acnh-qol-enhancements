# Hook ownership and feature activation (stage 2)

This stage builds on stage 1's bounded native validation. Work stays on
`refactor/internal-feature-interfaces`, starting from `0f061b1`; `main` and the
stage 0/1 comparison artifacts remain preserved. These interfaces serve this
mod only, with startup registration and no external SDK or hot loading.

## Explicit activation results

`core/feature_activation.hpp` defines the bounded feature result table and
descriptors. Catalog, Pockets, Crafting and Controls declare their actual
features and settings; single-feature modules use the same activation path.
Common build-profile checks are recorded as dependencies. Pockets retains its
real dependency chain: hold-select and pocket hints need quick-store; the
withdrawal hint needs both storage withdrawal and pocket hints. Disabled
settings do not invoke their native installers. Independent features continue
after another feature fails.

| State | Meaning |
| --- | --- |
| `Disabled` | The declared feature was not requested. |
| `Ready` | Its installation/preparation completed. |
| `Unsupported` | Native compatibility or owned code validation rejected it. |
| `DependencyUnavailable` | A required feature, registration slot or claim capacity was unavailable. |
| `Failed` | Publication/readback failed or installation ended incompletely. |

Every result also records `published` and a reason. State alone must never be
interpreted as proof that nothing changed: `Unsupported` or `Failed` can have
`published=true`. Hook publication, callback registration, resource allowance
activation and private pointer writes contribute to mutation tracking.
Publication attempts are tracked conservatively because Exlaunch can abort
instead of returning an error. Native installer booleans remain an internal
implementation detail beneath the feature activation results.

`ModuleDescription::install` returns an `InstallResult` aggregate instead of
one boolean. Module error summaries include publication and ready/unavailable
counts; individual feature results identify what remains available. Traversal
also records pole, handheld ladder, placed ladder and tool-swap capabilities
when attempted. A rejected placed-ladder extension now makes Controls' summary
partial while working traversal paths and other controls remain active.

Resource support is separate from gameplay. A present Fitting Room or Shopping
Card marker prepares the existing exact-path allocation support even when the
module is disabled, invalid or too old. A disabled module can legitimately have
`published=true` because its resources remain supported. Missing packages do
not prepare allowances. Module summaries include those resource mutations even
when gameplay activation fails. Configuration parsing, overlay order, defaults,
minimum versions and normal supported gameplay are unchanged.

Results describe startup attempts, not continuous health monitoring. Absent
packages have disabled module records; their leaf installers are not invoked.
Repeated activation does not install a feature twice. The table has 64 slots,
reserved before invoking installers, and cannot allocate dynamically.

## Preparation and publication

All executable hook installation in `source/program` goes through
`core/hook_plan.hpp`; Exlaunch itself is unchanged. Each native installer names
its callback, site and original instructions in a preparation group. Stage 1's
profiles remain the source of quick-store, traversal and preview fingerprints.
Other installers keep their instruction evidence beside their native guards.

1. Validate native prerequisites and reserve every requested site in the group.
   Reservations do not install hooks or expose callbacks. Rejected preparation
   releases unpublished reservations when the plan leaves scope.
2. Shared services check bounded callback/provider capacity before installing
   their hook and publishing registration. Startup is synchronous: there is no
   worker-thread registration or callback invocation during this preparation.
3. Publish hooks in the existing native order. Pockets still publishes its menu
   after handlers. Shared input, captions and reveal policies retain their
   existing dispatch/observer order and independent behavior switches.
4. Private fitting/preview vtable slots retain their original-pointer and range
   checks, with mutation tracking and explicit failed readback results.

The executable registry reserves a conservative 20-byte span per site, the
maximum patch width of the pinned Exlaunch implementation. Even near branches
that modify four bytes reserve all 20. First claims validate original words;
shared reuse requires the same named owner, callback identity, original
fingerprint and exact installed main-site bytes. Conflicting owners, overlapping
ranges, foreign overwrites and restoration of pristine bytes after ownership
are rejected. An installed boolean is no longer evidence of hook ownership.

`HookRegistry` has 160 claims, below the pinned 32-KiB trampoline pool's 163
200-byte slots. Every program hook uses it; reused sites do not allocate another
trampoline. A plan contains at most 20 reservations. The existing input handler and
input observer capacities remain 5 and 5. Stage 5 replaced the historical
four-provider caption registry with three exclusively owned caption keys; see
[the current caption contract](shared-input-ui.md#captions). Duplicate
registrations validate the owned hook before succeeding; incompatible observer
pairs are rejected. The registry checks main-site bytes, not the integrity of
every Exlaunch pool byte or native object lifetime.

Preparation is not a transaction. Exlaunch allocation/relocation may terminate
the process; published callbacks and pointer writes are not rolled back if a
later step fails. The activation result must retain that partial-publication
evidence. No additional per-frame executable scanning or dynamic allocation was
introduced.

The fitting caption helper now returns failure. If caption capacity is
unavailable, the bag installer declines before publishing private input vtables
or gameplay hooks. Previously it ignored this failure and continued without
caption support. Existing resource support remains active and is reported.

## Verification

The final normal build uses the pinned toolchain and prerequisites recorded in
`internal-feature-interfaces-baseline.md`. Logs and comparison outputs live
under the ignored local directory `dist/refactor-stage2/`.

```sh
sh tools/build.sh
QOL_BASELINE_ELF=dist/refactor-baseline-35ec0c8/fresh-build/project.elf python3 tools/check.py core
python3 tools/check.py shopping dialogue crafting controls pockets fitting catalog
```

- Normal, verbose-dialogue and auto-vault-diagnostic builds passed. Focused
  stage 2 ARM checks also passed for both conditional builds. The normal build
  was restored before final regressions.
- The host ownership/dependency test passed with AddressSanitizer and
  UndefinedBehaviorSanitizer. It covers conflicting identities, overlapping and
  adjacent ranges, capacity, cancellation, stale reservations, changed/removed
  patches and missing/disabled dependencies.
- Production ARM tests cover both consumer orders for shared input, reveal,
  resource, caption and preview hooks; caption/provider and observer capacity; duplicate
  registration; foreign changes; preparation without private publication; and
  failure at a real renderer-slot write/readback boundary. Fitting caption
  failure leaves its private vtables untouched while retaining resource support.
  All five feature states and partial traversal/resource outcomes are checked
  at two load addresses.
- Core tests passed all 128 module subsets at two load addresses. Installed
  hook sites/readiness and the same 978 pocket callback scenarios match the
  preserved stage 0 ELF. The hold-select fixture now models actual hook
  publication and registration instead of setting a deleted ownership boolean;
  its behavioral assertions remain intact. Its first run exposed missing text
  segment metadata inherited from the callback-only fixture; supplying the
  actual text start/size lets real bounded preparation run. All 96 hold-select
  scenarios pass with the original behavioral assertions.
- Final regression coverage: all 14 host suites and all 41 regression scripts
  passed across the final runs against the restored normal build. Pockets,
  Fitting and Catalog were rerun after correcting fixture metadata; earlier
  successful groups used the same binary. The finalized focused activation
  test, including both caption-consumer orders, also passed separately.

No live Eden test was performed. Modeled services, Exlaunch installation stubs
and rendering fixtures cannot establish live engine effects, pool relocation
correctness or visual behavior. Existing live-only limits in `VALIDATION.md`
still apply. No release packaging, installation update or integration into
`main` is part of this stage.

Stage 2 completed on 2026-10-13. `main` remains at
`35ec0c865065dda9f4b09660434370d90ac57373`. Final SHA-256 checks verified the
151 saved pre-build files, all 2,816 pre-existing dist files, three saved fresh
stage 0 outputs and all three saved stage 1 outputs remain unchanged. The final
stage 2 ELF, NSO and NPDM are saved separately in
`dist/refactor-stage2/final-build/` with `sha256.json`.
