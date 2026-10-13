# Stage 7 Eden validation handoff

Status: **offline candidate; no live Eden checks performed**. This document
prepares testing only. The working installation has not been replaced; no merge,
publish or release action is authorized by this handoff.

## Candidate and recovery identity

- Executable source commit: `ed70923623427ac575578624f898e1fa941fe287` on `refactor/internal-feature-interfaces`.
  The subsequent documentation commit changes no executable source.
- Fresh normal build: all three diagnostic compiler switches are 0, using the
  pinned network-disabled devkitA64 image from `tools/build.sh`.
- Candidate copies: `dist/refactor-stage7/candidate/build/project.elf`,
  `project.nso`, `project.npdm`; loadable names under `candidate/exefs/subsdk6`
  and `main.npdm`. `candidate/identity.json` records full commit, hashes, build
  mode and image. `candidate/sha256.json` covers candidate files; verify before use.
- Separate comparison folders: `candidate/comparison-mods/` contains the eight
  suite folders, built with the existing package builders and their standard
  resources/configs. This is local preparation, not a new release archive.
  `SHA256.json` there records package content. No personal custom overlays are
  included. Preserve actual user overlays separately before a future test.
- Rollback comparison: stage 6 `dist/refactor-stage6/final-build/` and its
  `sha256.json` preserve the preceding `f1db681` candidate. Main remains
  `35ec0c865065dda9f4b09660434370d90ac57373`; stage 0 outputs are in
  `dist/refactor-baseline-35ec0c8/fresh-build/`, checked against its sibling
  `fresh-build-sha256.json`. Pre-refactor files and earlier release hashes are
  retained under that baseline directory and `existing-dist-sha256.json`.
  These comparison binaries are not a backup of the user's working installation.
- Before any separately authorized installation, save the working mod folders,
  personal settings and enabled-folder list in a new recovery location. Restore
  those exact folders/settings and restart Eden to roll back; do not mix two
  Core executables or restore only one of a matching executable/resource pair.

All paths above are repository-relative. Do not overwrite candidate/earlier
archives when collecting another build; create a new comparison location.
Supported game target is ACNH 3.0.3 (title `01006F8002326000`, main build prefix
`FF1D1C05670DB6021C85B624A710B963`), not an arbitrary version.

## Test setup and evidence

Record candidate manifest/hash, game build, Eden version, OS/GPU/backend,
resolution/frame limit, save/resident, enabled suite folders, other enabled mods
and complete regular/custom settings. Use a backed-up test save and comparable
items/recipes. Restart the game between module/setting combinations: activation
and resource registration are startup-only. Custom overlays live at
`romfs/qol-enhancements/config/<module>.custom.ini`; clear unintended overlays
for controlled runs. For diagnosis, set Core `log_debug=true` in a separate
custom overlay; record that setting and capture startup/activation output.

For every failed row below, capture: row ID, exact actions and timing, startup
log, enabled folders/settings, candidate hashes and a short video from before
trigger through failure. Add the row-specific evidence listed below. Also run
the same setup against the stage 6 comparison when practical. If it also fails,
record it as pre-existing relative to stage 7; do not silently classify it as
acceptable. Differences from stage 0 can be earlier refactor/product changes,
so keep that comparison distinct. Record passes and unavailable setups as well.

## Module/settings matrix

Core is required in every row. Folder absence differs from `enabled=false`.

| ID | Installed/enabled setup (all other modules absent unless stated) | Purpose |
| --- | --- | --- |
| M0 | Core alone | Native pockets/phone/workbench retain ordinary behavior; no optional input consumers. |
| M1 | Core + Pockets defaults | Pocket actions, multiselect and captions. Repeat with `enable_multiselect=false`. |
| M2 | Core + Crafting defaults | Phone preview, workbench tilt, quantity input share the dispatcher. |
| M3 | Core + Crafting; phone true/workbench false, then phone false/workbench true, then both false | Independent toggles; disabled surfaces retain native behavior. Keep `crafting_quantity_wrap=true`, `crafting_speed=6` unless recorded otherwise. Quantity shortcut still installs with Crafting. |
| M4 | Core + Pockets + Crafting + Fitting Room | Shared hooks/captions, suppression and phone/workbench transitions. |
| M5 | All seven optional modules enabled with defaults | Representative full-suite interaction; includes Controls traversal/dialogue and Catalog labels. |
| M6 | Core + Fitting Room and Shopping Card folders installed with their regular markers, both `enabled=false` | Replacement layouts still load with allocation floors while gameplay is disabled. Compare with those folders absent. Do not delete assets to simulate disabling. |
| M7 | M4, then remove Pockets folder and restart; repeat with Crafting `enabled=false` | Independent features survive absent/disabled consumers; no stale registration across a fresh process. |

Keep matching candidate resource folders when testing M4–M6. Do not combine an
older standalone pockets, preview or fitting hook mod with this suite. Record
unavoidable third-party conflicts as a separate comparison.

## Observable checks

Checkboxes remain open until live evidence is recorded. Each row specifies
setup, action, expected result and additional failure evidence.

| ID | Setup | Action | Expected result | Additional failure evidence |
| --- | --- | --- | --- | --- |
| [ ] P1 | M1/M4; home storage available, ordinary storable item selected | Tap Y, then hold Y; repeat across several items | One storage action per fresh eligible press, native completion/capacity rules, one feedback event | Before/after item and storage counts; audio recording and held duration |
| [ ] P2 | M1; droppable item, releasable creature and prohibited/context-restricted item | Tap ZL for each; repeat during an in-progress action | Native drop/release only where eligible; no duplicate transfer or bypass of rejection | Item/menu/context and resulting world/inventory counts |
| [ ] P3 | M1; equipment, food and drink; test full energy and unavailable action | Tap ZR on each, including equipment with other possible actions | Equipment precedence, eligible native consume behavior; invalid/disabled actions do not execute | Item identity, energy, native rows and animation/feedback |
| [ ] P4 | M1/M4; eligible item | Hold each shortcut through repeated frames; press Y+ZL, Y+ZR and ZL+ZR; release one button before all | Held presses do not repeat; chords do not trigger a delayed action when one remains held; fresh press rearms | Controller overlay/video showing exact press/release order |
| [ ] P5 | M1/M4; eligible action, native blocked state or transition | Press while blocked, then unblock with button held; close/reopen pockets; navigate during pending work | No delayed shortcut after blocking, no stale owner/item action after reopen or selection change | Focus/selected slot before and after transition, activation/input logs |
| [ ] P6 | M1; multi-frame storage/drop/equip/consume; hold-select on then off | Trigger once; keep input held through animation, reopen and retry | One operation and feedback per accepted action; cursor/focus restored; multiselect respects setting | Video/audio through completion, counts, selection state |
| [ ] U1 | M1/M4; all relevant footers visible | Inspect Use Item, Drop/Release, Bag and phone A/Select; repeat with a supplied locale/custom caption | Correct text, no clipping, correct footer width; normal unowned captions intact | Screenshot, locale/custom UTF-8 text and footer dimensions |
| [ ] U2 | M4; phone/workbench preview and quantity controls available | Exercise transitions and conflicting input ownership; press while another consumer/native blocking owns input | Shipped input order, no double action or later replay after suppression | Frame-by-frame video with held buttons/axes, transition and action counts |
| [ ] F1 | M2/M4; ordinary DIY recipe with model | A open, left/right navigate, right-stick yaw/tilt, B close; hold B then release | Asynchronous open, one step then 12/4-update repeat, one rotation application per update, B drains until release, original grid/focus returns | Recipe index, stick overlay, timing and focus before/after |
| [ ] F2 | M2; slow-loading model and list endpoints | Navigate during loading, hold/release/reverse direction, try endpoints; block or close while pending | One retryable step, no backlog; cancellation prevents delayed movement; bounds respected | Loading interval, selected recipe sequence and input timing |
| [ ] F3 | M2; fixed-pose categories (e.g. tools) and ordinary furniture | Rotate, change between ordinary/fixed items, tilt to both limits and change item | Fixed native pose retained; ordinary tilt bounded ±60°, item change resets tilt/axes without a one-frame pose snap | Video of old/new item, angles/preset and transition frames |
| [ ] F4 | M2/M4; renderer legitimately busy/unavailable | Try A; release/retry after renderer is available | Failed acquisition keeps grid/focus usable; later fresh open works; no stale preview ownership | Busy context, startup/preview logs and restored UI |
| [ ] F5 | M2; at least 20 open/rotate/navigate/close cycles, app exit/re-entry | Repeat cycles, including close while starting/loading and transition away | No stuck capture surface, missing grid, residual input, wrong focus or increasing model/resource residue | Cycle number, memory trend if available, videos/logs before/after app re-entry |
| [ ] F6 | M2; reproducible model failure, prolonged loading or renderer replacement if accessible | Exercise native failure/timeout/context loss without arbitrary memory edits | Close/restoration on failed creation or >120 starting updates; foreign controller/parameters/mode are not changed | Native failure/transition logs, UI outcome and timing; mark unavailable if no reproducible setup |
| [ ] W1 | M2/M3; workbench and cooking previews, ordinary/fixed items | Move right stick vertically/horizontally, change recipe while loading; use native craft/quantity controls | Vertical tilt bounded; native yaw/crafting/input preserved; new item resets tilt, fixed poses retained | Recipe, input overlay, resulting pose and quantity/material values |
| [ ] W2 | M4; phone and workbench paths in same session | Alternate phone open/close and workbench entry/exit repeatedly | Correct current renderer ownership, no stale phone pose in workbench or reverse; native visibility/focus/mode restored | Continuous transition video and binding/preview logs |
| [ ] R1 | M2/M5; large, small, animated, particle and fixed-pose items | Rotate/navigate; view at supported frame limits/backends used for release | Native pose/framing retained, clipping/background/layers correct; animations/effects do not jump or accumulate transforms | Video, item IDs, backend/frame rate, screenshots of clipping/layer mismatch |
| [ ] R2 | M4/M5; preview over/under ordinary UI layers | Open/close and transition while animated; inspect non-preview UI afterward | Preview background stays scoped, no global clear-color leak, stale meshes or cleanup artifacts | Before/during/after layer screenshots and video |
| [ ] C1 | M0/M3/M7 | Exercise native pockets, DIY, workbench and quantity after each restart | Disabled/absent feature does not intercept its input; enabled independent features continue | Startup result/settings log and exact folder/toggle list |
| [ ] C2 | M6 then absent resource modules | Open fitting/shop footers and exercise normal disabled gameplay | Installed replacement layouts size/load correctly; gameplay remains disabled; absent folders use native assets | Layout screenshot, allocation/error log and module markers |
| [ ] C3 | M5 | Pocket shortcuts, phone/workbench, shopping/fitting, dialogue and traversal in one session | Shared hooks coexist without unrelated regression; features report expected availability | Full startup log, first failing interaction and clean M1/M2 comparison |

## Limits and qualification record

Controller/parameter/mode replacement, failed model creation, timeout, duplicate
snapshot dispatch, coherent publication and render-scope capacity have explicit
offline coverage. Some cannot be induced reliably through normal live controls;
mark those live rows unavailable and retain the offline evidence. Do not invent
live passes or use unsafe memory mutation to force a result.

Pre-existing limits: raw native controller pointers are callback correlations,
not stable lifetime handles; renderer scheduling/component lifetime, actual GPU
layers, sound, focus and cleanup require live observation. Busy publication,
unsupported profile, full bounded registries/cache or invalid caption adapters
follow their documented fallback, including suppressed private captions. A new
failure under the same supported setup compared with stage 6 is a candidate
regression, even if offline tests passed. See [architecture status](internal-architecture.md)
and [preview threading/lease contract](preview-sessions.md).

Record tester/date, each row's pass/fail/unavailable status and evidence path in
a separate results file beside the candidate. Leave this checklist's unrun
status intact until evidence exists. Offline build/suite results are recorded in
[stage 7 verification](stage7-verification.md). Live acceptance and any subsequent
installation, merge or publication are separate decisions.
