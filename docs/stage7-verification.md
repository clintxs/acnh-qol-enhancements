# Stage 7 consolidation verification — 2026-10-13

Executable source: `ed70923623427ac575578624f898e1fa941fe287` on
`refactor/internal-feature-interfaces`. Documentation is committed separately.
No intended gameplay change, release-version change or configuration change.

## Focused implementation

Phone Session now privately owns navigation samples, pending retries, repeat
history, rotation samples and accumulated tilt. `ClearFrameInput`,
`CancelPendingInput`, `ResetNavigation`, `CancelRotationInput`,
`TakeRotationInput`, `TiltBy` and `ResetItemRotation` express the existing,
different reset/consumption policies. The adapter still interprets engine input,
checks native eligibility and performs pose application/resource restoration.

For example, RotatePreview previously read the public x/y fields and assigned
zero to each. It now takes `const auto [x, y] = s.lifecycle.TakeRotationInput();`.
This drains exactly once even if the subsequent native renderer check fails.
Item changes reset tilt/axes but retain navigation cadence; suppression cancels
pending navigation but preserves held-repeat history. Duplicate consumed input
does not cancel the sample already queued earlier in that frame.

The host test now observes navigation through Session/Runtime operations instead
of private fields, retaining timeout, retry, repeat, close/drain and repeated
lifecycle assertions. Successful navigation is also checked for no further
moves over 26 sample-free frames. New cases distinguish cancellation from full
reset, exercise one-shot rotation/tilt limits/item reset, and check close drains
without an item tilt reset. Existing compiled duplicate-dispatch assertions
remain; additional production-hook cases cancel queued rotation with a missing
snapshot or blocked redispatch at both ASLR bases. No dead wrapper was added to
satisfy fixtures, and no native behavioral assertion was removed.

## Executed checks

All logs are retained under ignored `dist/refactor-stage7/`.

| Check | Result / evidence |
| --- | --- |
| Fresh pinned normal Docker build (`sh tools/build.sh`) | Passed; no compiler warnings/errors; `build.log`. All diagnostic compiler flags 0. |
| Focused run: `QOL_BASELINE_ELF=dist/refactor-baseline-35ec0c8/fresh-build/project.elf python3 tools/check.py core crafting` | Passed: 19 host suites and 8 compiled ARM scripts; `focused-check.log`. |
| Final host session assertions and direct-field audit | Passed with C++20, `-Wall -Wextra -Werror`; `session-operations-check.log`. |
| Full run: `QOL_BASELINE_ELF=dist/refactor-baseline-35ec0c8/fresh-build/project.elf python3 tools/check.py` | Passed: all 19 host suites and 46 ARM/resource scripts, exit 0; `full-check.log`. No test failures. |
| Preservation | Passed: all 2,991 entries across stage 0–6/baseline/old-release manifests match; main remains `35ec0c865065dda9f4b09660434370d90ac57373`; `preservation.log`. |
| Candidate preparation | Passed: 29 manifest-covered files, including matching executable/loadable copies and comparison module/resources; candidate and inner package hashes verified. |
| Live Eden, actual audio/GPU/focus/cleanup | Not run; no live qualification claimed. |

The full applicable groups are Core (6), Shopping (6), Dialogue (2), Crafting
(2), Controls (10), Pockets (6), Fitting (4), Catalog (10): 46 ARM/resource
scripts plus 19 host suites. The final host assertion strengthening changed
tests only after the focused run and full run's host phase; its final host rerun
and audit are recorded separately. Executable source/output did not change
between build, focused checks and full native regression execution.

Retained coverage includes navigation repeat/loading retry/cancellation,
rotation consumption and item reset, model failure/timeout/repeated cycles,
controller/parameter/mode replacement, phone/workbench coexistence/restoration,
coherent publication, native matrix ABI and render-scope behavior. Combined
module activation executes production installation and shared input registries;
pocket tests include production shortcut registration/command dispatch plus the
inherited callback suite. The baseline comparison confirms migrated hooks and
readiness plus identical 978 pocket scenarios on baseline/current ELFs. Engine
services not executed by a fixture remain modeled; this is not GPU/live evidence.

## Candidate identity and pending handoff

Saved candidate: `dist/refactor-stage7/candidate/`. Its `identity.json` identifies
the full source commit, normal mode, pinned compiler image and executable hashes;
`sha256.json` covers the isolated candidate contents. Matching standard suite
resources/configuration were prepared using existing `tools/package.py`
`suite_files()` builders, without invoking release packaging or creating a
release ZIP. README/attribution/licenses remain included. Generated outputs are
ignored and are not committed.

| Artifact | SHA-256 |
| --- | --- |
| `build/project.elf` | `2d7533904fcab4f196ba81a56af0bfcbcc8230d39d2027382397a4e295ce15f8` |
| `build/project.nso` / `exefs/subsdk6` | `e5d57675c3ae50b02133dafbc360bdda9533705fe9c2ad08a9696a1655c419d1` |
| `build/project.npdm` / `exefs/main.npdm` | `dc0bce7c73c6ef9822ddfffbe2af1a7115ae86f42e6f22796401a40f1feb0c37` |

Use the [live Eden handoff](stage7-live-validation.md) for exact module/settings
combinations, per-test setup/actions/expected behavior/failure evidence and
rollback comparisons. All live rows remain unrun. Main, comparison archives,
earlier releases and the working installation remain preserved; nothing was
merged, published or installed.

The [internal architecture guide](internal-architecture.md) records contributor
rules and selective future migration candidates. Remaining mixed-layer storage
withdrawal, traversal/tool restoration and quantity code were not rewritten.
Native component lifetime/scheduling still depend on supported callbacks;
workbench binding/tilt remains adapter-owned. These are explicit future-work
limits, not an external API commitment or a claim the whole mod is pure policy.
