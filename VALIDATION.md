# Unified QoL validation

## Equipped-pole auto vault diagnostic 1 (2026-10-11)

The first live prototype test did not vault. Eden confirms `auto_vault=true`
and reports no instruction mismatch, but its normal log cannot locate the
failed native stage. This build adds observation; it is not a confirmed fix.

`--auto-vault-diagnostic` enables bounded movement and entry traces even with
Core debug logging disabled. A fingerprinted inline observer runs before the
movement scheduler's hop guards. Mode-specific trace state keeps recursive
manual short-hop validation from exhausting the automatic probe/commit logs.
The normal build excludes the observation hook and diagnostic log strings.

The pinned offline diagnostic build and 158 focused ARM cases at two bases
pass. Added checks cover debug-off traces, observer context/player preservation,
deduplication, nested native validation logs, timer stages and the new fingerprint
conflict. Terrain, equipment resources and actual vault commits are still modeled.
The separate 0.4.18 diagnostic ZIP keeps subsequent Catalog changes, enables the
prototype, and includes matching source. Previous archives are preserved.

## Equipped-pole auto vault prototype 1 (2026-10-11)

Opt-in `auto_vault=true` extends the native held-stick hop path only while
a vaulting pole is equipped. Two C++ trampolines observe native intent and
wait for the complete ordinary-hop landing search to fail before checking a
native vault. The native hop counter and loaded delay threshold remain in
control; native reservation and traversal perform the jump. Explicit A,
pending/ready ordinary hops and other tools retain native behavior. Tool
switching and ladder traversal are outside this prototype.

The pinned offline build and eleven host suites pass. The Controls group
passes, including 150 parameterized auto-vault cases at two ASLR bases,
compiled hook policy, actual native stick/contact-direction guard flow and
timer update, strict/dynamic threshold, pending/adjusted hop priority,
recursive manual checks, commit refusal, configuration overlays and atomic
fingerprint rejection. Equipment, resources, terrain and traversal commits
are modeled; live Eden terrain/animation and multiplayer behavior remain
unverified. The actual native final probe-only early return also executes.
The Core group also passes, including logging, all 128 module combinations
at two ASLR bases, configuration overlays and recipe-preview regressions.

Core logging's stale 19-line expectation is corrected to 20: the existing
Pockets detailed-settings line brought the total to four feature detail
lines. Auto vault adds a field to the existing Controls settings line.

The regular Controls marker keeps `auto_vault=false`. A separate 0.4.17-based
prototype ZIP supplies updated Core executables and Controls with it enabled,
plus a matching source ZIP. Normal numbered releases are preserved. See
[the native investigation and live checklist](research/controls/auto-vault.txt).

## 0.4.17 Pockets menu-bypass settings (2026-10-11)

Added three independent true-by-default preferences for sole Open, recycling
Claim, and home-storage withdrawal. They gate existing eligibility checks,
including pending-action revalidation, without changing hook ownership or
shortcut handling. The storage Select hint is restored when withdrawal is off.

Validation: offline release build; all host policy tests including new settings
defaults, eight combinations, custom inheritance and invalid/duplicate inputs;
Pockets compiled/native ARM suite (846 cases), storage hints (226), recycling
Claim (104), and hold-A selection (88), at two ASLR bases. Added compiled Core
regular/custom configuration checks for all eight combinations. Engine/rendering
boundaries are mocked; false-setting behavior still needs live Eden testing.


## 0.4.13 Cooking preview Y / Set quantity (2026-10-11)

CookingTableInfoSeq inherits WorkCraftInfoSeq’s Play entry/exit and
MultiCreation state (03494B2C / 03494E7C / 03496160 / 0349634C). It uses the
same quantity child, recipe, material, action and result fields. Its concrete
preview page is CookingTableInfoPage (049C88A0), obtained by 02CBB710, and its
footer builder overrides the workbench builder at 02CBB99C.

Accept this cooking page alongside WorkCraftInfoPage in the existing shortcut
binding. Hook cooking’s native footer and use the same temporary X / Favorite
descriptor copy, Y / Set quantity descriptor and localized label routing.
Native eligibility and child readiness still decide whether quantity opens.
The existing shared state opens cooking’s own selector and returns to its
preview on cancel. Other page classes remain outside the shortcut binding.
No changes to quantity_wrap.cpp, its input policy, timing or animations.

The expanded ARM64 fixture runs both concrete preview types at two ASLR bases,
including cooking’s real page getter, native footer construction, shared Play
entry/exit, Favorite dispatch and quantity open/confirm/cancel. Recipe/material
eligibility, fresh/held/consumed/blocked input, capacity limits, unrelated-page
rejection, localized caption routing and atomic fingerprint conflicts are
covered. Core, Crafting and Pockets groups plus the nine host test suites pass.
The offline Docker build succeeds. Live cooking preview hints and controller
flow still need an Eden check.

Core and all six module markers are 0.4.13. Suite/source/custom-example ZIPs
are preserved alongside older releases and checked for archive integrity,
manifest hashes, extracted binary/config parity and source parity.

## 0.4.9 Preserve native Claim grid pause/resume

User reports the cursor still disappears with 0.4.8. That fix removed the
general name-window close side effect, but our bypass also skipped native
CommandSelect's grid pause (02df1c74..02df1cbc). Native Claim disables the
claimed widget, then calls ResumeGrid 03764fe0. ResumeGrid immediately returns
unless grid state is 3; our grid had remained in browsing state 2. Therefore
its native state-2 entry/focus restoration never ran after item invalidation.

Preserve the original grid pause sequence (mode 0 / state 3), but still skip
the child popup. Native Claim now resumes the grid itself. Limit bypass to
ready grid states 2/3; missing/not-ready grids use the ordinary menu path.
Keep the name-only disappearance fix from 0.4.8. No new hooks.

The expanded fixture no longer stubs ResumeGrid: original Claim, widget-mode
update, ResumeGrid and state transitions run. Rendered widget enable/focus
entry effects remain modeled. A negative case skips grid pause and reproduces
the missing resume; the fixed flow executes resume and returns focus to the
same pane after marking the item unavailable. This does not prove Eden rendering.

Passed: clean build, 842 compiled Pockets cases, 102 recycling cases, and all
64 Core module combinations at two relocated bases.

Pending Eden: claim without moving the stick, repeated claims, navigation to
another item, and full-pockets error/recovery. Previous local tests did not
cover the grid lifecycle sufficiently; the 0.4.8 in-game cursor fix was not
confirmed.

## 0.4.8 Name-only disappearance preserves grid focus

User confirmed 0.4.6 dismisses the balloon, but the hand disappears until moving
slots. General CloseController 02b0cb68 explicitly sets the shared cursor's
hide flag (02b0cbf0) and releases/clears its focused pane (02b0cc58..02b0cc5c).
Replace that call with the owned NameWindowCtrlSeq's native disappearance
request 030127b8 (virtual +1c0), keeping the close-request flag and state-4
transition without invoking parent focus cleanup or UI-manager removal.
Scope remains successful quick Claims and the verified name-window type.

The fixture now has a parent focus chain and shared cursor. It reproduces the
old native close's cursor-hide and pane-clear side effects, then checks that
the new path queues balloon disappearance with both values untouched. Passed:
clean build, 842 compiled Pockets cases and 90 recycling cases at two bases,
all 64 Core module combinations, and 580 retained Y-to-Hurry scenarios.
The existing 0.4.7 Y-to-Hurry feature is retained. No new hooks or assets.

Pending Eden: hand remains on the empty slot, old balloon disappears, the next
item's name appears normally, and a failed/full-pockets Claim remains unchanged.

## 0.4.6 Recycling item-name cleanup

User confirmed Claim bypass works, but the name balloon remains after removal.
Recycling owns NameWindowCtrlSeq at +220 (creation 02df0808, item-name update
02def8b0 -> 03011440); its vtable is 04a01130. After a successful native Claim
clears the slot's availability bit, call that controller's native close method
02b0cb68 (virtual +b0). This reaches 030127b8 and queues its disappearance
state 4. No blanket name-window hook or text/layout edits. Null/unrecognized
windows and failed transfers are untouched.

The recycling fixture executes the original close and disappearance-queue
code, checking success versus full pockets/insertion failure. UI manager and
rendering remain mocked; visible dismissal and the next item's balloon need
Eden verification.

Passed: clean build, 842 compiled Pockets cases, 88 recycling cases (including
missing/unrecognized windows and both close-queue phases), and all 64 Core
module subsets at two relocated bases. No additional hooks are installed.

## 0.4.5 Single-action Open / Claim bypasses

Pockets hooks CommandSelect entry 0308c4c4 and update 0308c590, restricted to
normal mode and first-level state 5. A live builder probe must produce exactly
one unflagged row with an empty callback: action 32 / CmnSubWin 0113, 33 / 0144,
or 54 / 0148. These are the three native English Open captions; matching keys
does not depend on the active locale. Second-level state 6 is never bypassed.
Entry preserves the page input lock but defers transitions until update. The
existing command hook supplies the row to native dispatch, including multi-frame
operations; A's existing feedback is not emitted again. Identity and eligibility
are checked again before dispatch. Normal menus remain the fallback.

Recycling uses its own controller (049df828), builder (02defbfc), command entry
(02df1c58) and selection update (02df1d68), state 4. Require one native action 0
with CmnSubWin/1602, an available nonempty slot, and the exact builder binding.
Revalidate the 12-byte item/record descriptor before executing the original
selection processor in UPDATE. Native capacity/insertion, saved-box record
removal, visible-slot invalidation, focus restoration and error routing remain
native. No changes to Hotel donations or unrelated one-option menus.

Compiled fixtures cover first-open child states 0/1, full pockets and failed
insertion without removal, actual native Claim record removal, menu fallbacks,
changed identities, and one-/multi-update Open dispatch at two relocated bases.
Open's engine action is stubbed; Claim's capacity/insertion and rendered UI are
stubbed. These are not full-game or save tests.

Passed: clean offline build; nine host suites; 842 compiled Pockets cases,
82 recycling Claim cases and 194 footer cases; Fitting Room checkout/feedback/
caption regressions; and all 64 Core module combinations at two bases. Pockets
now installs 19 feature hooks (plus its shared Core input/caption subscriptions).

Pending Eden: first and repeated Open/Claim, exit/reopen, full pockets, a normal
multi-option item, Open inside/outside, and existing Y/ZL/ZR/storage behavior.

## 0.4.4 Short hint captions

User confirmed 0.4.3 fixed sizing and unequip. Use native CmnSubWin/0110 (Drop
Item) and /0108 (Hold), with their existing translations, leaving all action
selection and input behavior unchanged. Remove shipped custom overrides but
retain the private INI keys for optional user wording. The footer resolver
leaves an explicitly overridden token for the shared custom-text provider;
otherwise it substitutes the native bank/key. No LayoutMsg replacement.

## 0.4.3 Native text-change publication and Put Away

User confirmed 0.4.2 shortcuts work, but the custom captions overflow their
backgrounds. L2/R2 also appears in the camera app with their controller font;
native ZL/ZR IDs and glyph selection remain unchanged.

The shared custom-caption path previously called only the text setter. Native
publication at 01aeb27c..01aeb2c0 also prepares adapter operation 9 and calls
03c7dd80 on the binding afterward. Follow this sequence to notify dependent
layout bindings, rather than guessing background widths. Extend the compiled
caption fixture to execute the real notification routine and assert that its
dependent event is queued, in prepare/set/notify order, for both providers.
Rendering/measurement itself remains an Eden check.

ZR now accepts the native Put Away row 35 (Exclude, CmnSubWin/0127), callback
index 32 / 030773ac. The hint defaults to Equip / Unequip, retaining the
existing configurable qol_pockets.hold_wear key. Extend native command tests
to dispatch Put Away and run its original callback with equipment/UI services
mocked: it requests empty-hand equipment, clears the equipped marker, and
does not remove the item from Pockets.

Passed: 794 compiled ARM Pockets cases at two bases, 190 footer cases,
Fitting Room checkout/feedback/caption regressions, host policy/catalog tests,
and all 64 Core module combinations for the new notification fingerprint.
Eden verification is still needed for visible background width and ZR unequip.


## 0.4.2 Pockets trigger shortcuts

ZL selects native Release (menu row 41) or Drop Item (30); ZR selects Hold (6)
or Wear (5). These are original ItemUIContextMenu.bcsv row indices, not its
operation IDs. The native builder must offer an unflagged row with its empty
callback. Y retains the additional own-home restriction. No save edits, item
spawns, direct world mutations, or LayoutMsg replacement are introduced.

The native Command UPDATE drives the operation. Drop Item callback 0307513c
requires multiple UPDATEs: the synthetic selected row is supplied on each
update until native code queues its next state. Item identity/menu eligibility
are rechecked before starting, not halfway through a native transaction.
Shortcuts require a fresh, unconsumed single button and normal browsing/focus;
holding, chords, dragging, special modes, disabled/absent actions and queued
transitions are rejected. New hints use native ZL/ZR glyphs and configurable
plain-text captions; Pockets and Fitting Room share Core's caption hook.

Local checks: 760 compiled ARM Pockets cases at two relocated bases, including
the actual six-update Drop Item callback with world-placement services mocked;
190 native footer/caption cases; all 64 module subsets at two bases; independent
and combined Pockets/Fitting Room caption providers and once-only text loading.
These checks do not emulate the full world, render the footer, or test a save.

Pending Eden: ZL on ordinary items and critters, ZR on tools/clothes, unavailable
actions/empty slots, no-room placement, already-held items, rapid/held/chorded
input, outside/inside transitions, and the existing Y storage/withdrawal flows.
Check five-hint width in the house (including controller-font mods and longer
localized captions), and Fitting Room Bag with Pockets enabled/disabled.


Validated locally on October 11, 2026 against the supplied original ACNH 3.0.3
main (build FF1D1C05670DB6021C85B624A710B963). The user confirmed the player
dialogue fix and subsequently the corrected 0.2.3 NPC diagnostic build work in Eden. The user also confirmed Nook’s Cranny transactions in Shopping Card 0.3.1.
The user confirmed the corrected savings HUD in 0.3.4 and balance animation
in 0.3.5, the restored counter in 0.3.10 and the 0.3.14 allocation fix.
Localized name capture, configuration layering, aspect-ratio placement and Able
Sisters still need a live check. No running game or save files were accessed
by the local tests below.

## 0.3.71 Fixed Bag caption and locale coverage

The user reports that changing from Remove to Add retains excessive footer
width and reverted to 0.3.69 with a fixed Bag caption. Restore that version's
single-key hint, native focus behavior and transaction source. Remove the
0.3.70 membership-based caption selection, cache invalidation and immediate
focus calls. This avoids triggering the reported width transition; it does not
claim to fix native dynamic-label measurement. Keep the trapezoid icon.

Supply action translations in all 16 supported locales, including USpt/EUpt.
Retain fitting_room.add_to_bag for compatibility and change the built-in English
fallback to Bag. The removal key is no longer used. Locale tests load the actual
shipped INI through the compiled parser and check the UTF-16 setter text.
Native footer/caption tests and package checks cover the fixed behavior;
translations and final visual fit are not native-speaker/live-game verified.

## 0.3.70 Contextual Add/Remove caption

Choose the native footer's private token from the hovered exact item/variant's
bag membership. Both tokens use the same once-loaded locale catalog and separate
English fallbacks. The session remembers the last action rather than re-reading
the previous filtered index, because category changes can replace that index.
Invalidate the native footer-mode cache (+1c4f4) when the action changes. On a
successful Y toggle, invalidate and invoke the existing native focus/footer
routine immediately; rejected toggles do not change the action. Begin/Destroy
clear the session cache. No extra hooks or layout edits are introduced.

Validation: native footer and caption tests at two bases, including descriptor
selection from the real session bag; Add/Remove locale fallback, old add-only
INIs; compiled input/focus tests for cursor changes, same-index filter changes,
successful toggles, unchanged action, and capacity rejection; badge feedback
through 33 entries. Live footer transitions still need an Eden check.
The packaged icon was verified byte-identical to the 0.3.69 trapezoid resource;
the user's earlier icon mismatch was selecting the wrong mod folder.

## 0.3.69 Basket appearance experiment

Replace the semicircular body with a trapezoid: 34-unit top, 22-unit bottom,
17-unit height, and radius-three corner fillets before the existing 1.25 scale.
Keep the handle and centerline positions. Bake into the same RGBA8 texture;
no runtime logic or pane-count changes. Inspected the generated texture at
three scales on a simplified circle. Layout preservation, texture decode/native
lookup and package checks cover the replacement; final appearance needs Eden.

## 0.3.68 Fitting-room Y hint and editable caption

Append a native KeyGuide descriptor at 03114f28 in the existing fitting-room
footer rebuild, after A/B/Purchase and before its optional ticket hint. The
native routine has already checked the footer type; x19 is the footer and x20
is the owner at this site. The hint uses native Y ID 7 and the owner's player.
Bounded pool/entry checks reserve a fifth slot for the ticket and reject duplicate
tokens. Normal native hint-mode caching and UI teardown remain responsible for
rebuilds/removal; no per-frame append or new input action is registered.

InitializeBagCaption at 01aea210 forwards native initialization for every key.
For the private fitting_room.add_to_bag token only, set the initialized parser
state to 12 (no message resolution) and pass the UTF-16 caption through the
verified pane/group setter at adapter +5e0. Native footer sizing follows. Native
update and destruction were executed with this state in the regression test;
no custom ownership pointers or allocations are introduced. The Pockets inline
caption resolver at 02de4954 remains separate and untouched.

The Pockets UTF-8 catalog parser reads fitting-room.ini once on the first actual
caption render, with active-locale, USen and built-in English fallbacks. Ordinary
footer captions do not trigger I/O. FS errors, malformed encoding, duplicates
and oversized files fall back. The shipped INI provides the requested English
label and allows users to add localized sections; no speculative translations.

Checks: pinned build; all 64 module subsets at two bases; native fitting footer
construction and descriptor copying with/without ticket, cached rebuild,
limited capacity and wrong footer type; native message initialization/update/
destruction plus modeled final text setter; exact-key routing, pane/group
bindings, locale fallback, invalid/oversized/missing files and once-only reads.
The native controller glyph and final caption spacing need an Eden live test.
No save files were accessed or transaction behavior modified.

## 0.3.67 Corrected Y quantity shortcut and X Favorite

The user confirmed 0.3.66 opens and cancels correctly in Eden, then corrected
the requested Switch input to Y (their Steam Deck X button sends Y). The
workbench item preview now uses Y / Set quantity and X / Favorite, with both
controller hints updated. Other pages retain their native bindings.

Y samples are consumed even when quantity is unavailable, preventing the
native Favorite delegate from also running. A fresh X queues the existing
Favorite action (3); native Play update still performs the toggle. Held or
blocked input cannot retrigger either action. The footer copies Favorite's
X icon while temporarily overriding its descriptor source, then restores
the native key. The new quantity hint uses Y and the existing translated text.

The updated ARM fixture covers 84 cases at two bases, including native
Favorite dispatch, held X, ineligible Y consumption, restored footer key
binding and the existing opening/cancellation/gating cases. Core, Crafting
and Pockets checks pass. Live button/hint confirmation is pending.

## 0.3.66 Direct workbench preview quantity shortcut

X / Set quantity now opens the native quantity selector from the large 3D
workbench item preview. Both input and the localized hint use the native
recipe/material/multiple-crafting checks. The selector retains native recipe
data, limits, ownership, crafting result and cancellation behavior.

The new ARM fixture passes 76 cases at two ASLR bases, executing original
Play entry/exit, MultiCreation open/cancel, footer building and native hint
record copying. It covers eligibility combinations, fresh/held/blocked/
consumed X, competing A/B, pending state, child readiness, footer capacity,
caption routing alongside Pockets and isolated fingerprint conflicts.
Core, Crafting and Pockets regressions pass, including all 64 module subsets
at two bases, preview/tilt/navigation/particles, quantity repeat/animation and
crafting speed. Native rendering, localized text display and the live
controller flow still need confirmation in Eden.

## 0.3.65 Native quantity wrap animation and directional sound

Wraps now use the native single-row cyclic movement and cScroll feedback
instead of resetting/reopening the list. Quantity-only cAppear calls prepare
cyclic neighbor binding before display; endpoint gesture gating retains the
300 ms pause and stopping when held input reaches the next boundary. A scoped
row-binding adapter preserves valid adjacent labels for material-limited
ranges shorter than the padded runtime button pool.

The ARM fixture executes original reset/activation, native quantity label and
selection callbacks, button repeat, grid movement/interpolation and scroll
feedback. It checks both directions and native page classes, quantities 2..10,
pool sizes 5/15, two ASLR bases, correct visible neighbor labels, a single-row
animation, direction-specific sound selectors, settling, reset scope and the
existing exact 300 ms behavior at 30/60/120 Hz. Compatibility conflicts in
movement, either row binder or either cAppear call disable only quantity wrap.
Final text/layout/audio playback and controller/clock samples remain modeled;
the user confirmed the 0.3.65 visual/audio behavior in Eden.

Validation: pinned offline build, all nine host suites, Core startup/settings
checks across 64 module subsets and two ASLR bases, full preview/quantity
fixtures, 30,700 crafting/cooking cases and shared Pockets regressions pass.
Release archive integrity, manifest hashes and extracted/deployed/source parity
are verified separately. Previous releases are preserved.

## 0.3.63 Fresh-wrap pause adjusted to 300 ms

The user requests 300 ms after trying 200 ms. Set WrapPauseTicks to 5,760,000
(19.2 MHz * 0.3 seconds). Only this deadline changes: immediate endpoint wrap,
held native repeat at expiry, early release cancellation and stopping at the
next endpoint keep their existing behavior.

The quantity ARM fixture checks tick 5,759,999 remains paused and 5,760,000
resumes, at modeled 30/60/120 Hz, both ASLR bases, crafting/cooking pages,
directions and d-pad/stick controls. It also covers release/restart, pressure
jitter, native limits and input ownership. Clock and controller samples remain
modeled; live feel needs a Steam Deck retest.

Validation: pinned offline build, nine host suites, focused quantity ARM
fixtures at two ASLR bases and 30,700 crafting/cooking cases all pass.
Release ZIP integrity, manifest hashes and extracted/deployed/source parity
are verified separately. The previous 0.3.62 dialogue update is retained.

## 0.3.61 Shorter pause after a fresh endpoint wrap

The user confirms the 0.3.59 pause/resume behavior and requests a shorter pause,
approximately 200 ms. Change only the fresh-wrap deadline from 9,600,000 ticks
(500 ms) to 3,840,000 ticks (200 ms). Native repeat takes over at expiry; holds
that reach an endpoint from inside the range still require release to wrap.

The native 03C4B20C repeat handler counts input updates using snapshot+0x18 /
+0x1c timing parameters (or context overrides), rather than a fixed millisecond
delay. Its exact live quantity-page timing has not been established; 200 ms is
the requested tuning value, not a claimed measurement of the game's default.

ARM regressions cover both crafting/cooking pages, both directions, d-pad and
stick, and 30/60/120 Hz: remain paused through tick 3,839,999, resume at
3,840,000, stop at the next endpoint, cancel on release and restart a fresh
pause. In-range repeat and input ownership remain covered. System time and
controller samples are modeled; live feel still requires a Steam Deck retest.

Validation: pinned offline build; nine host suites; all 64 module subsets at
two ASLR bases; preview/quantity fixtures; 30,700 crafting/cooking cases;
630 Pockets and 190 storage-hint cases. All pass. Release ZIP integrity,
manifest hashes and extracted/deployed/source parity are verified separately.

## 0.3.60 Fitting-room badge rendering load and resource allowance

The user reports UI flashing and unrelated elements changing size/position
once the bag exceeds 16 items. The screenshot shows corrupted Bell and row
presentation. This is consistent with shared renderer/resource exhaustion,
but the exact GPU buffer and failure point have not been captured in Eden.

Concrete findings: the old basket contains 70 individual picture strips plus
its background circle. Seventeen simultaneously drawn badges would contribute
1,207 picture panes. The prior archive expands to 144,496 bytes against a native
RSTC allowance of 144,512 bytes, leaving only 16 bytes of archive headroom;
the original allowance includes 8 KiB beyond the original archive. The
16-entry motion ring wraps its index and stores no retained pane pointers.
The native 16-item purchase-report limit applies only during checkout.

Bake the same silhouette into a 128x128 RGBA8 picture with antialiased alpha,
white RGB and the existing mint pane tint. Keep the original group/background,
placement and scale; replace 70 strip panes with one picture. Seventeen visible
badges now add 34 picture panes. Preserve native panes, materials, textures,
animations and other archive entries. The expanded archive is 210,800 bytes.

Move the existing Shopping Card resource allowance into Core, retaining one
shared hook and independently enabled exact-path floors. Fitting Room requests
512 KiB; Shopping Card keeps 1 MiB. Missing/disabled/invalid module behavior
matches the existing installed-resource preparation contract. Never reduce a
larger allowance or replace the global RSTC. Both Core and Fitting Room must
be updated for this release.

Checks: pinned build; all 64 module subsets at two ASLR bases; native RSTC
lookup, independent floors, disabled/absent packages and fingerprint rejection;
native SDK BNTX relocation and texture lookup at two bases; independent GPU
byte-order decode and alpha-cutout checks; native layout preservation and
constant two-picture badge cost; existing Shopping Card layout/texture checks;
suite structure/manifest checks. The feedback regression crosses 16/17 and
32/33 distinct bag entries while retaining the native scale/dirty propagation.

Live verification remains required: reproduce with 17+ items, scroll through
bagged rows, switch categories and remove/re-add items. These tests do not
render GPU frames, establish the engine's exact draw limit, or access a save.

## 0.3.59 Half-second pause after a fresh endpoint wrap

The user confirms 0.3.58 fixes joystick wrapping. They now request a fresh
outward press at quantity 1 / maximum to wrap immediately, pause scrolling for
500 ms, then resume if the control remains held. Reaching a boundary by holding
from inside the range still stops there; release and press again to wrap.

Replace the post-wrap release-only lock with a timed pause using Horizon's
19.2 MHz system tick (svcGetSystemTick), independent of UI frame rate. During
the pause, present neutral vertical direction buttons and a neutral left stick
to native dispatch. Consuming the inputs instead leaves native repeat timers
in a release-required state and would prevent held-input resume. At expiration,
pass the fresh snapshot through to native repeat, keeping the wrap latch
disarmed. A real release cancels the pause and allows a new deliberate press.
A new wrap gets its own timer. A/B remain available throughout the pause.
Native list reset/reopen and invalid/inactive list samples cancel pending pause.

The ARM quantity fixture executes native button repeat timers, grid movement,
reset/activation/focus and selected quantity/material callbacks with a modeled
system clock. Both page classes, directions and control sources are covered at
two ASLR bases. Timed tests simulate 30/60/120 Hz input updates, remain paused
through tick 9,599,999 and resume at tick 9,600,000 (500 ms). Holds continue to
the next boundary and stop without wrapping again. Other cases cover early
release and a new pause, immediate in-range movement, consumption at expiry,
delayed logical directions, pressure jitter, native lower ranges and reopening.
Controller sample generation, scene/layout services and the clock remain test
boundaries; live Steam Deck timing and feel still require confirmation.

Validation: pinned offline build; nine host suites; Core/configuration checks;
preview/quantity ARM fixtures; crafting/cooking and shared Pockets regressions.
Package manifest hashes, extracted/deployed binary parity, source archive parity
and ZIP integrity are verified. Older releases are preserved.

## 0.3.58 Joystick quantity wrap release and repeat handling

The user confirms d-pad wrapping in 0.3.57, but reports intermittent joystick
overscroll (1 -> 10 -> 9) and held input continuing past an endpoint. The prior
consumer suppressed only the wrap frame and rearmed as soon as the raw stick
fell below its 0.5 press threshold. Native logical-button repeat dispatch is
separate from raw axis consumption (03C4B20C / 03C48C70 versus 03C4B880).

After wrapping, consume both vertical logical buttons and the left-stick axis
on every input update until release. Require both no held vertical button and
a centered stick (radius 0.15) to rearm; pressure changes below the 0.5 press
threshold or a horizontal deflection cannot rearm an ongoing hold. Retain an
armed latch during slow travel from neutral to the press threshold. Native
page reopen clears both latches. Within-range scrolling, limits, confirmation,
cancellation and the setting remain native/unchanged.

The quantity ARM fixture now executes native 03C4B20C repeat timers,
03762978 direction callbacks and 0375FD8C list movement, alongside native
reset/activation/focus and the quantity/material callback. Cases cover long
holds after both scrolling and wrapping, delayed logical stick buttons,
pressure jitter, slow deliberate presses, direction reversal, both exact
crafting/cooking page classes, material-limited ranges, blocked/consumed input
and reused-page reopening at two ASLR bases. Raw-to-logical controller samples,
layout construction and material text/GPU services remain modeled. Live Steam
Deck confirmation is still required.

Validation: pinned offline build; nine host suites; Core/configuration checks;
preview/quantity ARM fixtures; crafting/cooking and shared Pockets regressions.
Release packaging checks include manifest hashes, deployed/extracted binary
parity, source archive parity and ZIP integrity. Older releases are preserved.

## 0.3.57 Quantity wrapping, matching DIY yaw, and workbench transition poses

The user confirms that 0.3.56 workbench tilt works. This release reverses DIY
right-stick horizontal rotation to match the native workbench, retaining the
working vertical sign, speed, item-specific starting poses and fixed categories.

`crafting_quantity_wrap=true` is now active and remains the default. Input is
scoped to native WorkBenchNumSelectPage / KitchenNumSelectPage. A neutral-observed
latch permits a fresh endpoint press to select the opposite endpoint. Holding
continues native repeats within the range and stops at the boundary; changing
direction without neutral does not rearm wrapping. Native quantity ranges,
material status, confirmation and cancellation remain native. Native list reset
also disarms reused page/grid pointers during reopening. Setting false installs
neither its list-reset hook nor its shared-input consumer. Installation remains
independent of preview/tilt/speed settings and compatibility failures.

The selector uses reversed rows (quantity = native count - selected row). It
wraps using native grid reset, activation, row selection and focus; row selection
fires the native quantity/material callback. The ARM fixture executes those
bodies and 03469E1C's selected quantity store, plus native list movement/clamping
for worst-case repeated holds, at two ASLR bases. Layout row visibility, cursor
art, material text formatting, and repeat timing are modeled. Both exact page
classes, d-pad/stick, resident row 3, native lower limits, blocking/consumption,
reversal without release, disabled behavior and reused-page resets are covered.

Workbench tilt previously reset on MaterialPending; the scene trampoline then
removed the last tilt before skipping reapplication, exposing the old mesh's
default orientation during loading. Keep the input tilt until a replacement is
ready, and retain/reapply the displayed pose only on matching full child handles
while loading. Native framing and new completed items still reset the new item.
The regression holds the outgoing native matrix across pending updates and a
native default-pose write, then checks reset for the replacement. Particle
primary-angle alignment and native horizontal updates remain covered.

Validation: pinned offline Docker build; nine host suites; Core's 64 module
subsets/configuration layering at two ASLR bases; preview/quantity ARM fixtures;
30,700 native crafting/cooking cases; 630 shared Pockets cases and 190 storage
hint cases. Package hashes, source parity, extracted/deployed binary parity and
ZIP integrity are checked before delivery. Older releases remain intact.
Live Steam Deck confirmation is still required for quantity interaction and
visible workbench transitions. No running game, save, synced log or installed
mod was modified by these checks.

## 0.3.56 Native-selected workbench profile and input diagnostics

The user reports that 0.3.55 still does not tilt. The synced log confirms
BOOT_OK 0.3.55 and the enabled setting, but no vertical-input log entries.
Further disassembly corrects the previous release's interpretation: 034951BC
indexes context +40's background pages and changes page visibility. It never
writes controller +50. Background page index 3 is not a model profile.

Retain the concrete WorkCraftInfo page guard, and accept its current native
item profile using the exact 032BD990 mask (0x32DEC, modes below 18). Publish
that mode with the full model handle; the scene hook requires that both still
match. Exclude the owned DIY controller. Do not change native mode selection.
The fixture exercises every accepted profile on the workbench page and rejects
the phone profile and unrelated pages. Layout allocation and native horizontal
dispatch remain modeled, so these tests do not identify the live profile.

Log vertical input before all ownership/state/consumption guards, bounded to
16 unique page-class/profile pairs per boot. For matching workbench pages, log
binding changes with resident row/loading/blocking state and the first four
vertical movements per item; log the first four successful scene applications.
Also log the first eight native item-root handles independently of input binding.
These diagnostics distinguish rejected input bindings, blocked/consumed axes
and scene propagation without consuming or modifying native input. The new
build requires another live workbench test; it is not a confirmed live fix.
The user-confirmed DIY flicker fix and all setting defaults remain unchanged.

## 0.3.55 Workbench tilt binding fix

The user confirms 0.3.54 fixes the outgoing DIY pose flicker, but workbench tilt
has no effect. The synced log confirms the setting and installer are enabled.
Native WorkCraftInfoSeq uses controller profile 3 (034951BC), not profile 2
used by the DIY renderer. Its visibility wrapper is the nested loading icon
returned by 0346E9C0, not an entry in the page's top-level wrapper array.
Both former guards prevented input from reaching workbench tilt.

Recognize the concrete WorkCraftInfo page vtable (04A3B3E0, constructor
0346E7E8) and profile 3; preserve DIY profile 2. Fingerprint the actual native
profile selection and page constructor. Continue honoring resident row,
blocked/consumed input, fixed categories and pending items. Native horizontal
rotation/crafting dispatch remains untouched. Add bounded input diagnostics
for the first four vertical movements per item. No setting defaults change.

The workbench fixture now uses the native page vtable/input method, executes
both actual page and nested-loading-icon getters, and uses profile 3's root
mode 2. It verifies that the nested visibility wrapper is distinct from the
top-level wrapper, and checks that profile 2 and another page type cannot tilt.
Scene matrices, effects, native-yaw preservation, limits, blocking and item
reset pass at two ASLR bases. Layout allocation, SDK math and native horizontal
input dispatch remain modeled; actual workbench visuals need a live check.
The confirmed DIY transition retention is unchanged.

## 0.3.54 Preview transitions and workbench tilt

The user confirms native starting poses in 0.3.53; an outgoing-item pose flash
remains during navigation. Keep displayed scene ownership/carrier identities and
restore the last displayed pose on a surviving carrier reset during loading.
Switch fixed policy only when native framing completes for the requested item.

`workbench_preview_tilt=true` now enables right-stick vertical tilt (default on),
independently of DIY previews. Workbench native dispatch remains in control of
horizontal rotation and crafting. Scope by profile/state/full handle and the
exact visibility layout wrapper; use the footer's resident row and existing input
blocking. Share the actor trampoline and Core input dispatcher. Tilt is +/-60
at 6 degrees/update with a 0.15 dead zone, applied to scene primary angles so
particles and drawing agree. Fixed wallpapers/floors/rugs/fences stay native.
Quantity wrapping remains deferred; crafting speed is unchanged.

Compiled regression coverage at two ASLR bases checks modeled old-carrier reset
retention and fixed-policy handoff, native starting matrices, effects, navigation,
workbench input ownership/resident/blocked/consumed guards, tilt limits, no pose
accumulation, native yaw writes, item reset and fixed categories. Core checks the
independent default/custom toggles, both preview settings false, shared-hook
ownership and feature conflict isolation. Workbench wrapper/row binding and native
horizontal input dispatch are modeled; actual GPU transitions/workbench controls
require the user's next live test. No game/save files were accessed.

Local build and focused Core/Crafting/Pockets checks pass: nine host suites,
all 64 module subsets at two bases, DIY/workbench ARM regressions, 30,700 crafting
cases, 630 pocket cases and 190 storage-hint cases. Additional workbench setting
overlay/disabled/conflict checks pass at both bases.

## 0.3.53 Native initial item orientation

The user confirms 0.3.52 fixes particles, and reports that the Log pack starts
backward in the DIY app while the workbench shows its front. The shared
controller's initial angles are not the item's starting pose. Native item-root
framing selects per-item/carrier angles; our override previously discarded them.

After native framing reaches state 0 for the requested item, capture each owned
carrier's complete rotation matrix without scale. Convert secondary X-Z-Y/native
primary Y-X-Z composition into primary Y-X-Z angles using main's atan2f import.
This preserves the actual native orientation while keeping particle state and
drawing on the same primary rotation path. Cache by recipe epoch, full carrier
handle and component; recapture after native reframing. Stick input adds only
movement relative to the controller's opening angles. Loading/previous-item
states cannot seed the cache. Before scene propagation, the render fallback
also preserves the native carrier pose and adds only movement. Fixed types keep
their existing bypass. No new hooks, settings or workbench behavior changes.

Compiled checks at two ASLR bases compare matrices for all 53 rotation presets
in ItemPreviewSwapActor.byml and five additional pole cases. The Log pack's ID
and Bag kind are verified against ItemParam.bcsv. Checks cover unchanged cached
poses across frames, stick yaw/tilt and particle radians relative to a nonzero
native pose, loading/previous-item guards, navigation epoch/origin reset, and
reopening even with a reused modeled handle. Native matrix creation, primary
setter, steady-state root/drawable and item-effect update run; parameter selection,
scene allocation, SDK atan2f, GPU rendering and skeleton evaluation remain
modeled boundaries. Full Core/Crafting/Pockets and package parity checks run
before delivery. Live Log pack appearance still needs Steam Deck confirmation.

## 0.3.52 Primary rotation for item effects

The user confirms 0.3.51 navigation works, but faucet particles still separate
when rotating. Native item-effect update 006904C0 reads primary Euler angles
at component +2B0/+2B4/+2B8 directly and ignores secondary viewing angles.
0.3.51 updated only the secondary angles. Its modeled bone attachment check
missed this independent native particle-state path.

The owned carrier now receives the preview angles through its native primary
XYZ setter (virtual +150, 005F7B4C); secondary angles are cleared so drawing
applies the rotation once. Native drawable propagation carries the same primary
pose to drawable-owned effects. Combined yaw/tilt now uses the native primary
Euler convention. Fixed wallpapers/floors/rugs/fences retain native framing;
ownership guards, input, speed and tilt limits are unchanged.

At two ASLR bases, the compiled fixture first reproduces the old secondary-only
failure, then executes actual native item-effect update through its publication
boundary. It checks primary angle-to-radian output for both carrier and drawable
owners under yaw/tilt, actual drawable propagation, and existing attachment and
invalid/fixed/closed-owner checks. Full skeleton evaluation, GPU particles and
live scheduling remain outside the fixture. Core, Crafting and shared Pockets
regressions and package hash/source/binary parity are checked before delivery.
Live Steam Deck confirmation of attached particles is still required.

## 0.3.51 Horizontal controls and effect attachment pose

The user confirms left-stick navigation in 0.3.50, but reports no D-pad movement,
vertical stick movement changing recipes, and faucet effects drifting on rotation.
The prior direction constants confused a grid scroll enum with logical input bits.
Native input routes kinds 0/1 to Y and 2/3 to X; preview navigation now reads masks
4/8 and ignores up/down, including synthesized vertical stick button bits.

A new hook runs after native type-42 item-root update, copies the owned pose into
validated type-6 carrier components, and preserves native framing for fixed types.
Native drawable transforms then copy that stored pose before bone evaluation.
The final matrix override alone previously changed drawing without updating the
scene pose that effect attachments use. Workbench rotation also writes the actual
carrier component. Other actors, stale handles, dead entities, parent/type mismatches,
missing components and fixed previews are excluded.

Compiled checks at two ASLR bases cover horizontal navigation and vertical button/
stick rejection. They execute native steady-state item-root update, carrier-to-
drawable transform, and bone attachment matrix composition under yaw/tilt, with
skeleton evaluation modeled. Fixed/invalid/closed ownership cases retain their
original component values. These checks establish stored-pose and attachment
composition behavior; live kitchenette particles and game scheduling need Deck
confirmation. Core, Crafting and shared Pockets input regressions and package
hash/source/binary parity are checked before delivery.

## 0.3.50 DIY preview recipe navigation

D-pad/left-stick left/right moves linearly through the app's current sorted or
filtered recipe array, crossing rows and scrolling pages. Native grid reset,
focus and viewport clamping maintain the selected/visible card, materials and
Hotel banner. Native item-change updates the owned preview actor, preserving
camera ownership; recipe changes reset rotation/tilt and reclassify fixed types.
Holding repeats after a delay and stops at list ends. One pending step survives
model loading; B, blocking and missing input clear queued movement.

At two ASLR bases, compiled integration tests execute native grid reset and
clamping, pooled-row mapping, input-list focus, item-change and deferred readiness.
A 35-item nonmonotonic list with 32 pooled buttons covers both directions, partial
last rows, scrolling, input repeat/reversal, duplicate/consumed/blocked/missing
snapshots, B priority and cursor/reopen behavior. GPU row visibility/position,
scene rebuilding and material text rendering are modeled boundaries. Existing
Core/preview, Crafting speed and Pockets shared-input regressions run before
delivery, followed by package hash/source/binary parity checks. Live Deck
rendering and scrolling/loading timing still require confirmation.

The user confirms a noticeable speed difference with 0.3.49's higher maximum.

## 0.3.49 Crafting speed up to 30x

Maximum `crafting_speed` rises to 30, with larger positive integers clamped to
30 without overflow. Default 3, ramp timing, recipe readiness and native actor
gates are unchanged. Local parser checks cover every integer 1..30, oversized
decimals, custom-layer inheritance, duplicate rollback and invalid input.

The 30,700 compiled crafting/cooking cases cover every ramp sample at all 30 peaks,
custom values and clamping, register preservation and native entry/exit behavior
at two ASLR bases. They now execute the real animation-rate setter 03750760 and
verify both animation-track +A0 writes and component +A8 storage. No 3x/6x clamp
exists in that setter. Other animation processing and live completion timing
remain outside this fixture. Dialogue/transitions retain native timing, and
short actions can end before the ramp's peak. Core/preview regressions and
package hashes/source/binary parity are also checked before delivery.

## 0.3.48 Hotel preview banner

The existing Hotel Required: X group is temporarily placed after the capture
picture as the preview’s last child. Its native background bounds determine a
16-unit bottom/right inset, including dynamic widths, origin and container scale.
The native text/count, visibility and background style remain authoritative.
The original parent, sibling order and XYZ position are restored before each
native phone update and on B/deactivation. Missing/unsupported banner geometry
or inconsistent list links skip relocation; preview rendering remains available.
No new hooks or workbench/layout resource replacements are added.

At two ASLR bases, compiled integration tests execute native recursive Pane draw
order with modeled GPU submissions. They check picture-before-banner layering,
corner alignment across changing widths/preview dimensions, native content and
hidden-banner behavior, repeated updates, B/reopen/deactivation cleanup and failed
model creation or malformed geometry/links. Core activation, full DIY preview
lifecycle/fixed poses and crafting speed regressions are checked before delivery,
as are package hashes and source/binary parity. Live Hotel placement needs a
Steam Deck test; the other documented preview/UI follow-ups remain deferred.

## 0.3.47 expanded Crafting speed

`crafting_speed` now accepts integer peaks 1..6, with higher positive integers
clamped to 6. The default remains 3 with the exact original curve. The 60-update
ramp, native readiness/suspension/local-player gates and exit resets are unchanged.
Peak 6 doubles vanilla’s 3x peak; total crafting time also includes the ramp and
other native animation/UI timing.

Parser checks cover all six peaks, overflow-sized decimals, malformed suffixes,
regular/custom inheritance and duplicate rollback. The 24,252 compiled ARM
crafting/cooking cases cover every ramp sample through both native paths at two
bases, including maximum clamping and preserved GP/SIMD registers. Core activation and
DIY preview regressions are also checked. Package manifests and source/binary
parity are verified before delivery. Live 6x behavior still needs a Deck test.
Workbench tilt, quantity wrap and preview UI follow-ups remain deferred.

## 0.3.46 Crafting module and settings

Crafting replaces Faster Crafting, requiring the new crafting.ini marker at
minimum package version 0.3.46. The existing DIY renderer/layout code moves from
Core to Crafting, preserving its confirmed 0.3.45 rendering, color, fixed item
categories and right-stick movement. Core alone now installs discovery/amiibo
only. Crafting and Pockets register their existing shared input handlers when
needed; package order changes are covered by the two-base integration fixtures.

Defaults are diy_app_preview=true, workbench_preview_tilt=true,
crafting_quantity_wrap=true, crafting_speed=3. Preview and speed are active;
tilt/wrap are accepted reserved settings with no hooks or runtime UI behavior.
Speed config selects integer peaks 1..3 for the existing 60-update ramp. The
default returns the original table exactly; lower peaks scale its gain. Native
actor gates, suspension, recipe readiness and exit resets remain unchanged.

Checks cover parser defaults, regular/custom overlays, invalid values, duplicate
rollback and atomic file errors. Compiled activation checks cover all 64 module
combinations, absent/disabled/retired markers, the independent preview toggle,
reserved settings, incompatible preview/speed isolation, untouched disabled UI
slots and Pockets sharing. Native crafting/cooking fixtures execute all ramp
samples at each supported peak, readiness/local actor guards and register
preservation. The full preview lifecycle, color, layout and 767 fixed-item
regressions continue to execute after the source/module move. Core, Crafting and
Pockets checks and package/source/hash parity are completed before delivery.

The user accepted 0.3.45's visuals. The new module installation, settings and
lower speed peaks still need a live Steam Deck test. Workbench tilt, quantity
wrap and the other documented UI improvements remain deferred.

## 0.3.45 native fixed poses and doubled speed

The user confirms 0.3.44's right-stick turning and vertical tilt work. They
request native workbench poses locked for wallpapers, floors, rugs and fences,
and twice the movement speed elsewhere. The accepted cream color is unchanged.

Workbench 0348D8BC uses the same native item-model creation function, mode 2
and display 0 as this preview. Controller parameter decoding 032B74F0 and
item-root update 0192A590 supply native angles and framing; the latter selects
per-item/carrier parameters (including the named mRemakeFence resource from
019305D0), converts angles at 0192B394 and writes carrier component +2BC.
The fixed categories now bypass our controller-angle protection and final
matrix substitution, preserving the native transform rather than hardcoding
angles from a sample item. Core classifies validated ItemParam rows independently
of Catalog, once per renderer acquisition, and publishes an atomic fixed flag.
Right-stick input is drained without moving these items. Other items use 6° per
update at full stick, retaining the existing dead zone and 60° tilt bound.

Checks use every real 3.0.3 fixed item and sample every other ItemKind through
compiled open/deactivation, including malformed metadata and English/Japanese
enums. Fixed input, supplied native setter angles, root/carrier/grandchild matrix
preservation, B cleanup and ordinary rotation after a fixed item are exercised
at both ASLR bases. Existing checks cover native lifecycle, background scoping,
6° turning/full-turn wrap, bounded tilt and all compatibility fingerprints.
Full Core checks and package/source/hash parity are run before delivery.

Native resource loading and GPU rendering are modeled locally. Confirm all four
fixed categories, the faster controls, B/reopening and ordinary workbench behavior
on Steam Deck. No live game or save is accessed by these checks.

## 0.3.44 carrier ownership correction

The user accepts the 0.3.43 cream background but reports no rotation. The synced
log records thousands of matrix calls with matched_matrices=0 across recipes,
while right-stick input and native transform application succeed. The previous
fixture incorrectly modeled the drawable as an immediate child of the controller.

Native type-42 initializer 0192A1F0 requests type-6 carriers, storing handles at
actor +274..280 (count +394). Type-6 initializer 019362F4 prepares type-6F
requests. This extra carrier level means the drawable's parent handle differs
from the controller's handle. 0.3.44 publishes the four carrier handles after
the normal native transform pump, validating the root's registry identity,
generation, liveness and type. The matrix hook matches type-6F drawables against
those published parent handles. It does not traverse the registry on a render
worker. B clears all published handles before releasing the renderer.
The accepted background and native clear flags are unchanged.

Checks execute both native initializers, with allocation/component attachment
and factory services modeled, to establish the carrier relationship. The
rotation fixture now uses the two-level hierarchy, retains stale drawable
angles, and executes 019283F0 -> the compiled hook -> native 006021A4. It checks
changed matrices, unchanged component bytes, all four carriers, stale generation,
wrong kind/parent, dead or missing root, invalid child counts and cleanup.
The old immediate-parent check fails this fixture. Both ASLR bases, native
lifecycle, 31 fingerprint rejection cases and all 64 module combinations are
covered. GPU output and asynchronous scene scheduling remain modeled.

Visible rotation, rotation feel and ordinary workbench behavior still require
a live Steam Deck test. Local matrix results do not establish on-screen output.

## 0.3.43 displayed mesh pose and HDR clear

The 0.3.42 live log reports every queued transform applied, layer matches each
frame, and successful B/reopen/release. The user reports no visible rotation;
the supplied screenshot has a uniform background around CCC9C0, not E5DFC7.
Neither controller updates nor matching layer draws establish displayed output.

Native 019282FC copies a parent's transform into a separate item mesh component;
019283F0 then uses 006021A4 to build that mesh's matrix. The previous fixture
only built the controller actor's matrix. The revised fixture gives the mesh
stale angles and executes the actual mesh matrix entry. A hook at 006021A4
substitutes an atomic preview pose in a local component copy, scoped by owner
handle +230 or parent handle +234. Tests check the complete matrix, unchanged
source bytes, unrelated parent handles and scope removal on B. Live evidence
does not yet establish whether this matrix boundary runs in the phone preview;
new bounded counters record matrix calls and ownership matches.

The background now overrides float RGBA at 039F63F0 during its enclosing owned
layer draw. Original options and native clear flags pass unchanged. Atomic
command/renderer/view/draw-info slots scope the synchronous substitution and
are cleared after return; unrelated layers and calls pass through. HDR values
(1.5950871, 1.3879482, .8751216, 1) approximate an inverse ACES-style filmic
curve fitted per channel to the screenshot. This is an empirical experiment,
not an established game tonemap or proof of exact display RGB. Tests verify
scoping, native argument preservation and GPU clear values, not pixel output.
The live test must verify visible right-stick rotation, background match,
B/reopening and ordinary workbench behavior. The old Unmapped Read64 at 0x88
still appears after release without a PC; its origin remains unestablished.

## 0.3.42 native pose and color paths

The synced 0.3.41 trace confirms all 77 queued transforms were applied, while
none of 2,662 low-level clear calls matched the layer-color pointer. RE found
032B9510 can reapply animated default angles and 039392B0 copies the layer color
to its stack before the observed 039F6E00 clear.

The new setter hook preserves the owned preview's cached pose while retaining
native validation. The layer draw hook passes a local native color override
for #E5DFC7, scoped by the actual draw context's layer pointer. It leaves layer
storage and caller options untouched. Tests execute the native parameter update
to reproduce/prevent overwrite, build the native 3x4 matrix, and execute the
layer's copied-color conversion followed by the graphics-clear boundary.
Steam Deck visuals, rotation feel, B/reopen stability and ordinary workbench
behavior still require user validation. No live fix is claimed by local tests.

## 0.3.41 live render diagnostic

The user reports no change with 0.3.40. The synced log confirms Core 0.3.40,
right-stick snapshot input (consumed=0, blocked=0), queued Angle32 updates with
ready=1/flags=1 and a non-null layer-color pointer. At the 30-frame report the
pending bit remains set; continuing input means that line alone cannot prove
whether the native pump ran or applied previous angles. The log does not record
actual native update attempts or which clear passes occurred. It again ends with
an Unmapped Read64 at 0x88 after release, without a PC/backtrace.

The previous fixture explicitly invokes late dispatch and assigns entity phase
1 after every phone frame. That proves native behavior in that fixture, not the
live app's schedule or phase. Its requested layer-2 clear likewise does not
prove that pass occurs in the live renderer. 0.3.41 adds observational C++ hooks:

- Native 032B9FCC executes exactly once with its original arguments. During
  preview ownership, count calls/attempts/applied pending updates and report the
  first eight pending attempts, flags before/after, readiness, entity phase/stage.
- The existing clear hook counts actual passes and matching layer-color clears,
  reporting each distinct caller once (matching/nonmatching separated), bounded
  to 16 process-lifetime entries. Reports include target, mask, color address and
  original RGBA bits. No UI session data is accessed by a render worker.
- Report totals after 30 preview frames and before release; remove both atomic
  observation scopes before native cleanup. No extra native update is invoked,
  no entity phase is changed and no additional rendering behavior is changed.

Local checks execute the observer plus original native pump for rejected phase-0
and successful phase-1 calls, verifying bounded logs, accurate counters and
unchanged native results. Color diagnostics distinguish matched/unmatched clears
and preserve original GPU/depth semantics. Unowned/released calls are unobserved.
Existing tests still cover both bases, lifecycle, layout preservation and all 64
module combinations. Build, package integrity and source parity are checked.
This is a diagnostic release; rotation and background matching remain unfinished.
The next live run should open one preview, hold the right stick for a few seconds,
then press B. The synced log should distinguish missing scheduling, rejected
native updates and a nonmatching clear path before another rendering change.

## 0.3.40 DIY deferred rotation and cream background

The user confirms 0.3.39 renders models, but right-stick rotation has no visible
effect. Its log shows successful model allocation, flags=0/state=1 and release.
The previous implementation called 032B9E08 directly. That helper can reject
writes outside its entity-update phase. At this stage, 032BE2B0 and entity +398
were interpreted as transform invalidation; 0.3.42 research corrects that field
to a material publication gate and identifies native pose animation overwrite.

0.3.40 writes controller +80 and sets its transform-pending bit at +74. Native
late dispatch 02B0E20C -> 032B9FCC applies position/rotation and material publication gating,
then clears the bit on success. Material updates and other owners remain
protected. Repeated dispatch of an already-consumed snapshot no longer erases
this frame's queued movement. B and missing/blocked input still stop rotation.
Bounded right-stick/rotation log entries distinguish input from queued updates.

The following 0.3.40 pointer-matching approach was superseded in 0.3.42: live
clears copy the color to their stack instead. The item renderer uses layer/camera
2 (01513A48). Native layer draw 0386D730
passes layer +9C as the RGBA pointer to ClearRenderTarget (038025B0). A C++ hook
substitutes RGBA (229/255, 223/255, 199/255, 1), #E5DFC7, only for that pointer,
color target 0 and a color-clear mask while the preview owns the renderer. An
atomic pointer publishes the scope to render workers; no Session is read there.
Close/deactivation removes the scope before release. Stored layer colors are
never changed. Missing layer metadata leaves the normal preview usable.
CaptureBGAlpha stays 255; CaptureBGColor remains absent.

Compiled ARM tests execute the native late dispatch, registry/type/thread checks,
Angle32 writes and material publication gating at two bases. They reproduce rejection in
UI entity phase 0, then successful application in phase 1, and require entity
+398 to change. The clear hook executes the actual native clear body through
modeled graphics API calls: requested RGB, untouched other layers/targets,
depth/stencil values, source color preservation and scope removal are checked.
Existing rotation, layout/aspect-ratio preservation, camera lifecycle and all
64 Core module combinations pass. New native bindings/call sites are guarded
before any integration hook installs. Build/package integrity and source parity
are checked. GPU output and right-stick feel still need an Eden/Deck test.

## 0.3.39 DIY visibility regression

The user reports 0.3.38 no longer renders the model. Its synced log confirms
Core 0.3.38, successful model allocation, flags=0/state=1 and release. Changing
CaptureBGAlpha from 255 to zero was the only capture change from the visible
0.3.37 build. Preserving source alpha therefore appears to hide the whole model:
the source 3D pass does not provide usable item alpha for this compositing path.
The previous native parser test established the flag change, but could not
establish that the source framebuffer contained useful alpha.

Restore CaptureBGAlpha=255, exactly as in 0.3.37 and native WorkBenchInfo, while
retaining right-stick controls unchanged. The layout regression requires the
opaque capture and fails against the 0.3.38 ELF. Native alpha parsing still runs
for both branches, explicitly documented as insufficient to prove GPU visuals.
The preview background remains black; background matching is unfinished.

Core checks cover both load addresses, native rotation, B/deactivation/reopening,
parameter/pane restoration, camera counters and all 64 module combinations.
Build, ZIP manifest integrity and source parity are checked for this release.
Restored visibility and rotation need a live Deck test. The log also ends with
an Unmapped Read64 at 0x88 after release, with no PC/backtrace; its origin is not
established by that line and these tests do not claim to explain it.

## 0.3.38 DIY transparency and right-stick rotation

The user confirms 0.3.37 renders models, with a screenshot of a wooden simple bed
inside the existing app and a black rectangular background. The synced log shows
repeated model creation and release without a reported crash. The requested
rotation control was corrected to the right stick before implementation.

CaptureBGAlpha changes from 255 to 0. Native 03A2DAD4 sets the capture's force-
opaque bit for 255; zero leaves the scene alpha intact. CaptureBGColor remains
absent so the framebuffer model is copied rather than cleared. The late picture
can then composite over the original app background instead of an opaque black
rectangle. The opacity assertion fails against the 0.3.37 ELF; the new fixture
executes native alpha parsing for both values at two load addresses.

The shared snapshot's right-stick float pair is row +0C/+10, with consumed-axis
bit 1 at +40. Native 03C4BAC4 verifies the pair ABI. Input is collected only for
the showing preview's routed row and applied once in its following phone update.
A 0.15 dead zone rejects drift; invalid/non-finite axes, consumed/blocked input,
missing snapshots and loading/unowned renderers cannot rotate the item. Maximum
speed is three degrees per update, with yaw wrapping and pitch limited to ±60°
from the item's initial pose. Reopening uses the native pose again.

Native 032B9E08 resolves the model handle and writes its Angle32 XYZ vector;
controller +80 stores the cached angles. 032B74F0 establishes a full turn as 2^32
units. Controller +78/+7C are position floats and remain untouched. The compiled
fixture executes the native handle/type/thread validation and angle writes,
checking both directions, a full turn, tilt limits, roll/position/material
preservation, routing on resident 5/input row 4, stale handles, missing ownership,
B while the stick is held and reopening. New native bindings/call sites are
fingerprinted; the camera lifecycle and all 64 Core module combinations pass.

Build and package integrity/source parity are checked. GPU compositing and
rotation feel still need a live Deck test. Recipe navigation remains deferred.

## 0.3.37 DIY capture draw order

The synced 0.3.36 log shows valid item handles, loading flags cleared and state 1
retained. B releases the model and restores the renderer. The user sees the grid
hide but no model. Native capture draw helper 03A2E5D0 copies the current
framebuffer crop via 0384A8E0, then draws any capture children via 0027E140.
WorkBenchInfo places its empty N_Capture_00 first under RootPane and its display
picture later in the UI. The prototype instead appended capture after all app
panes, so the original background had already painted over the scene.

The capture is now the first root child, at (-430,-64), independent of the late
hidden display surface. Both start hidden, become visible after model allocation,
and hide on B, deactivation or failure. Original resource sections and installed
aspect-ratio edits remain preserved. No workbench layout is loaded.

The new pane-order regression fails against the 0.3.36 ELF. It checks the native
workbench ordering and empty capture, then requires the transformed MenuRecipe
capture to precede every original root child, with the picture under the late
surface. Compiled lifecycle checks cover visibility of both siblings across A,
B, reopening, deactivation and failed allocation at both load addresses. Core
module combinations, build and package integrity are checked for this release.
GPU rendering is still outside the local fixture: live model visibility and
16:10 placement require a Deck test. Rotation and navigation remain deferred.

## 0.3.36 DIY renderer reservation

The synced 0.3.35 log confirms physical A reaches the preview with the correct
recipe, state 3, no pending transition and no input blocking. Every request is
then refused by the idle-only renderer guard. Native MenuRecipe virtual +200
(slot 049E8378, function 02EA0754) returns mode 26. The base app update
02B0C720 invokes PhoneFrame and then reserves that mode in controller state 3.
Thus an active app is not an idle renderer, and merely allowing mode 26 would
let the outer update reclaim it immediately after preview activation.

Core accepts only an empty mode 26/state 3 phone reservation or an idle
mode -1/state 0 controller, with no handle/background/visibility page. It saves
the original reservation, changes to item mode 2/state 1 through native
ChangeState, and reports mode 2 through the phone's writable virtual-table slot
while that session owns the renderer. The original tiny getter stays untouched.
B and phone deactivation release the item through native ReleaseModel and
restore mode 26/state 3 when borrowed. Parameters and panes are restored; other
renderer owners are refused with their actual mode/state/handle in the log.
The native app subsequently releases its own reservation during normal closure.

The ARM fixture now executes the native outer app update. Reverting only the
mode getter reproduces immediate renderer reclamation. Separate lifecycle cases
execute native ChangeState, state 0/1/3 callbacks and ReleaseModel, with delegate
glue, scene allocation and mutex/GPU operations modeled. Camera reference counts
remain balanced across B, reopening and deactivation at both load addresses.
The mode slot and relevant instructions are guarded before installation; a
slot conflict leaves all DIY hooks uninstalled. Existing input/layout/ABI checks
and all optional-module combinations pass. Build and ZIP integrity, manifest
hashes and source parity are checked for this package.

Live model rendering, placement and B behavior still require an Eden test.
Rotation and left/right recipe navigation remain deferred until this stationary
preview milestone works in-game.

## 0.3.35 DIY direct A-input activation

The live 0.3.34 test still does nothing on A. Its synced log shows input_row=0,
press_bindings=1 and valid capture/grid pointers, but no A queued/declined log.
This proves the shortcut correction applied; it does not establish whether the
native press callback is reached or an earlier readiness guard rejects it.
Previous tests injected a row-5 dispatch event directly and therefore could not
prove that a physical A press would reach that path.

Activation now uses the shared PageInput snapshot's A mask (10 hex). The native
MenuRecipePage virtual table entry at 049E7F10 is 03C58D24, confirming this page
uses the shared hook. Native focus-enter and focus-leave delegates are observed;
their original functions still run, including native scrolling-row to recipe
index mapping. Row-5 press delegates remain untouched. Focus alone cannot queue
a model. A must be released after app entry, and activation respects consumed
buttons, page/global input blocking, phone readiness and focused recipe membership.
A is consumed only if a request is queued; renderer work stays in the next frame.
Input reports now precede readiness checks and include state/pending/focus/blocking.

The ARM fixture sends A snapshots without synthesizing press events. Native focus
mapping executes, including visible row 1 mapping to recipe 5 after scrolling,
with the materials view builder modeled separately. Checks cover
initial held A, focus leave/unrelated focus, blocked/consumed A, inactive phone and
pending transitions, shortcut rows, queued input suppression, B consumption and
reopen, missing capture/accessor, renderer failures and restoration at two bases.
The native input blocker and focus entry are fingerprinted. GPU rendering, camera
activation and entity allocation remain modeled; live visibility and B return are
still unverified.

## 0.3.34 DIY A-button input routing

The user confirmed 0.3.33 opens without crashing but A does nothing. The synced
log shows capture/grid binding succeeded, with input_row=-1 and no A queued.
Native MenuRecipePage setup 02E936EC creates shortcut kind 5 at page +570 and
kind 6 at +578. The prototype incorrectly required kind 3 (Back), declining A
because it could not resolve an input row. B remains snapshot mask 20 hex;
its mask is independent of the kind of shortcut used to obtain the input row.

The revised fixture executes native 02E936EC; shortcut allocation alone is
modeled. It reproduces the no-op against the 0.3.33 ELF. The compiled fix reads
kind-5 routing, refreshes it on A, and reports declined recipe presses and the
number of bound press callbacks. Regression cases cover missing/wrong shortcuts,
out-of-range rows, routing assigned after bind and row 4 on resident 5, including
B on an unrelated row not closing the preview. The shortcut-creation call site
is fingerprinted. Existing renderer/input/lifecycle checks and Core module
combinations run again at both bases.

The user has confirmed app loading; live model rendering and B return remain
unverified. Local ARM checks model allocation/camera/GPU boundaries.

## 0.3.33 DIY app loading crash

The newly synced log identifies Core 0.3.32 and a null execution fault during
BindPhoneRecipes: project+78F8 returns from native FindPane, main+0027D228.
Main base is 80DDD000; the alleged root 855D6770 is actually SafeString virtual
table main+047F9770. The layout capture patch completed and no app-bound/model
log was emitted. This pinpoints the wrapper cast, before preview activation.

Page +3F8 is an array of layout-instance wrappers. Wrapper +18 is not a Pane.
Native 03C6627C resolves the drawable layout through wrapper +58, accessor
virtual +28 and an output pointer. The fix follows that API, checks the returned
RootPane name and initialized child sentinel, and otherwise declines the preview.
The accessor call-site instructions are included in compatibility validation.

The revised ARM fixture models the wrapper's actual SafeString table and accessor
separately. Native recursive Pane lookup now runs during compiled app binding,
not only in an isolated lookup test. The old path faults as reported; the fixed
path binds the capture/grid panes and passes A/B/reopen checks at both bases.
Missing accessors and wrong/unbuilt roots decline without native traversal.
Core installation/configuration combinations and the preview runtime are checked
again; prior 0.3.32 suite regressions remain recorded below. Build, ZIP integrity,
manifest hashes and source parity are checked for the new package.

The subsequent 0.3.33 live test confirmed app loading. A was blocked as described
above; model visibility and B return remain pending.

## 0.3.32 renderer-only DIY prototype

The user rejected replacing the DIY app with the workbench browser because it
would remove the app's immediately visible materials. This version removes the
previous workbench detail/background child pages and their private virtual table.
A now queues a native UI3D item request for a newly authored capture surface in
the existing app; B releases the renderer and restores the grid. This milestone
has a stationary model only. Rotation and recipe navigation remain unfinished.

The in-memory MenuRecipe transformation preserves existing section/pane bytes
and existing texture/material indices. A modified canvas and grid position are
checked alongside vanilla data. BFLYT buffers persist for native layout lifetime
in a bounded static cache; missing bindings, malformed data, unavailable renderer
or failed model allocation leave/restore the grid. Binding validation uses the
native picture types and actual dynamic-capture pointer, not just pane names.
No workbench RomFS layout is loaded or packaged for this feature.

The row-5 native press callback is used; row 1 is focus-leave and remains native.
The actual 3.0.3 button translation/dispatch bodies execute against the compiled
callback at two ASLR bases. Focus movement and unrelated button presses cannot
queue the preview. Native Pane lookup and item-request/release bodies execute,
including the null-background branch and handle/item/type cleanup. Camera state
transitions, entity allocation, layout construction and GPU rendering remain
modeled. Compiled hooks cover input suppression, B consumption and held-button
release, saved parameter/pane restoration, renderer busy/missing/capture-unbound
failures, repeated preview/deactivation, and individual fingerprint rejection.

An initial combined-module check exposed a duplicate PageInput hook with Pockets.
Core and Quick Store now register handlers with one shared, fingerprinted hook.
All 64 module combinations at two bases pass installation checks. Preview runtime
also runs with Pockets and Fitting Room installed; Pockets can independently own
the input hook if a DIY renderer guard prevents preview installation. Its existing
Quick Store runtime fixture executes through the shared dispatcher. The ELF test
loaders now size mapped memory from segment sizes, including the new retained
resource buffers, and keep import stubs beyond BSS.

Validation: pinned Docker build with networking disabled; all groups from
`python3 tools/check.py` (Core, Shopping Card, dialogue, crafting, Pockets,
Fitting Room, Catalog); ZIP integrity and per-file SHA256 manifest verification.

Live Eden validation remains required: open the app without pressing A, move
focus without opening a preview, press A on the Flimsy axe and a furniture item,
verify a model replaces the grid while materials remain visible, press/release B
and navigate again, repeat and close/reopen the app. Check the installed 16:10
layout, ordinary workbench rendering/crafting afterward, and Pockets Quick Store.
Local checks do not establish model visibility or a freeze-free live exit.

## Build and loader

- Pinned devkitPro Docker build, networking disabled: passed with `-Wall -Werror`.
- Core deploys only `subsdk6` plus the existing minimally adjusted `main.npdm`.
  No package contains subsdk9; avoiding the slot does not prove future
  Jubbsticks Additions hook/NPDM compatibility.
- All 64 combinations of six feature packages at two ASLR bases passed.
  `exl_main` installs only the content-mount continuation hook and performs no
  configuration reads. Activation executes the real compiled registry/parser and
  installers; a second activation performs no additional reads or installations.
- Full suite installs 70 disjoint modeled hook ranges (Core discovery 1,
  DIY preview 4 + shared page input 1, Catalog 16, Pockets 15, Faster Dialogue 7, Fitting Room 11,
  Amiibo 1, Faster Crafting 2, Shopping Card 11 behavior + 1 allocation support), plus the private
  fitting input vtable entry. No embedded binary replacement arrays, IPS or
  dialogue code-cave helper remain in the collection.
- Per-feature absent, disabled, invalid, missing-version, oversized, below-minimum
  and newer-version configurations were checked. Newer valid packages warn and
  run. Below-minimum packages leave feature code untouched. Open/read/size errors
  close handles where appropriate and disable requested behavior. Installed Shopping Card assets retain their
  independently checked allocation support, including invalid/disabled metadata,
  because RomFS overrides still load; absent packages install no support. Occupied
  startup/hook locations fail their instruction checks; dialogue validation
  rejects altered hook sites or legacy instruction changes before any of its
  hooks are installed. Original code-cave padding remains untouched.
- Every installed module reads its regular INI and then its optional custom INI,
  including disabled regular configurations. Shared enabled/version overrides,
  empty overlays, absent/invalid markers, atomic rejection of invalid/duplicate
  metadata, missing/read/size/open failures and once-only reads are tested at
  two load addresses. A custom file can re-enable installed code but cannot
  activate an absent module. Dialogue tests cover inheritance, false overrides,
  list replacement/clearing, invalid/duplicate preference fallback, normalization
  after merging, and the same keys in either file. Module packages from 0.1.0
  through the current version remain accepted under minimum 0.1.0 for the
  original five modules; Shopping Card requires minimum 0.3.2 (currency resource required).
- Compiled Amiibo C++ callback feeds 9,000 into the unchanged native halfword
  timeout store at both load addresses. The original immediate is untouched.

The loader harness models FS services, code-hook allocation, writable aliases
and cache services. It does not execute Eden's loader or establish that the
content-mount continuation is safe in a live process. Exlaunch's existing inline
adapter is selected with full SIMD-register preservation for discovery. The
SIMD context declaration was corrected to match the assembly stack layout, with
compile-time offset assertions. Ordinary InlineCtx layout is unchanged.

## Shopping Card

- 0.3.7 covers regular Nook Shopping/catalog orders for yourself. Catalog gift
  sending uses a separate native payment path and is outside this prototype.
- At two ASLR bases, the catalog affordability caller reads each of eight
  residents' savings in every modeled room and outside shops. The original
  accepted-order BL executes the compiled debit hook and native encrypted
  withdrawal; pocket funds remain unchanged. Invalid residents and adjacent
  unlisted callers retain native routing. Empty savings returns zero even
  with cash available. Native price comparison and the 20% Bell Boom adjustment
  execute unchanged at below/equal/above thresholds through the full savings cap.
- The 0.3.7 same-context root-list fixture passed, but the user reported cash
  still displayed outside Nook’s Cranny. 0.3.8 removes that context assumption.
  At two ASLR bases, the revised HUD test executes native Catalog open/close
  routines and the original BellPage update with deliberately separate contexts.
  It checks all eight residents outside shops and in Resident Services,
  savings Down/Finish feedback after ordering, silent unchanged/reopened balances,
  cash restoration on close, direct/deleting destruction and resident isolation.
  The deleting destructor's original BL enters the compiled teardown hook.
  Native open/close engine calls and destructor internals are modeled at their
  boundaries. No live Nook Stop/phone gameplay is claimed.

- Compiled C++ hooks run at two ASLR bases for both Nook's Cranny rooms and
  Able Sisters. Eight separately modeled resident accounts retain independent
  balances. Every allowed balance/debit caller is covered; ABD, pocket and
  unrelated caller/room paths retain the original wallet service. Missing room
  state and invalid residents do not route to savings.
- Tests execute the original savings read, deposit, withdrawal and encrypted
  money routines, including native zero/cap behavior; only resident lookup,
  current-field type lookup and RNG service are modeled. No actual save is used.
  The original hash routine verifies all three room-name constants.
- The original ShopSellDone payment branch executes with the compiled capacity
  callback's result and deposits the full test proceeds into the same resident's
  savings. The cShop capacity query selects ordinary sale success text (case 0)
  for every resident. Actual positive proceeds still follow the native savings
  branch. Other capacity modes/shops retain the native query result. No housing
  prerequisite or pockets-full explanation is introduced. Native cap clamping remains.
- Purchase-space callback passes zero Bell payment and unchanged ticket count
  to the original space helper, preventing false slots from Bell bags that
  savings payment does not consume. Shared Fitting Room affordability uses
  savings only while Shopping Card is installed and in a supported shop.
- Every Shopping Card instruction fingerprint is individually corrupted; each
  conflict prevents financial/UI behavior hooks; independent allocation support
  stays active for installed assets. Its own entry conflict prevents all support
  and behavior.
- Existing native fitting checkout and feedback regressions are rerun with the
  shared account accessor. Actual shopping gameplay, native field state and
  both modules together require the live checks below.

- Original generic and top-right cash refresh branches execute with the
  compiled wallet hook, different savings/cash balances, and matching native
  cash caches. Neither queues the amount animation that caused the reported
  pocket-close ka-ching in 0.3.1. A positive control with a real cash change
  still queues the original animation; no sound routine is muted.
- The user reported that 0.3.3 still showed pocket Bells in Nook’s Cranny.
  The earlier test covered an auxiliary controller only. 0.3.4 also hooks the
  actual BellPage HUD update at 02B13BF0, resolving its /L_Bell_00 counter from
  the Page/layout parameter table and reading resident+224 rather than +230.
- New checks execute the original BellPage update from entry to return at two
  bases, with only engine lookup/text boundaries modeled. All rooms/residents,
  an intentionally wrong alternate resident field, savings purchase/deposit
  refreshes without cash animations, cash restoration, and genuine cash-change
  animation are covered. This closes the earlier auxiliary-only coverage gap.
- Compiled savings display uses each controller’s current resident,
  resolves both direct/group text adapters, shows full zero/cap balances,
  restores cash panes outside supported shops, and tolerates absent resources.
  Native text setting, typed parameter lookup and pane lookup are modeled;
  0.3.10 recovery was confirmed in Eden; the user confirmed the
  balance animation in 0.3.5.
- 0.3.5 tests execute native 03353B14 number stepping and 03025400 feedback
  through its Up/Down/Finish animation dispatch and positional sound setup,
  with engine formatting, animation playback, position query and thread index
  boundaries modeled. Purchases count down, deposits count up, and completion
  selects the correct Finish cue. Full-cap-to-zero, mid-flight reversal and
  one-Bell completion pass at two ASLR bases. Initial display, resident/room
  switches, settled updates and auxiliary refreshes produce no duplicate cue.
  Hidden cash text is restored after feedback; native cash current/target and
  cached amounts remain unchanged. Actual rendered animation and audible sound
  are not established by these engine-boundary tests.
- Resource checks preserve every existing pane/material and font/animation archive
  member byte for byte. Original texture settings/payloads are preserved inside
  an extended BNTX container. The added subtree starts hidden, carries an independent
  24-byte amount buffer, and uses native rounded windows and ordinary textured pictures.
  The eight-unit oval rim, centered card transform, dark foreground card
  edge, separate white backing behind the oval and right-hand Bells badge
  are checked. Both later icon experiments failed live despite local checks:
  0.3.8 flattened the card; 0.3.9 made both savings and normal cash counters
  disappear everywhere. Its geometry preview established shape coverage only,
  not native resource loading/rendering. That build increased the component
  from 35 panes/74 sections/7,996 bytes to 281 panes/320 sections/33,580 bytes.
  Layout expansion is a suspect; no native pane/allocation limit is established.
- 0.3.10 restores the compact layout that previously displayed. A recorded hash
  of the 0.3.7 exported BFLYT verifies the recovery byte for byte after normalizing
  its sole intentional difference: Bells-badge X120 -> 96. The existing native
  resources, materials, controls and animations remain unchanged. A conservative
  project budget enforces 35 panes/74 sections; it is not an engine-limit claim.
  The original 1920x1080 canvas/root and the catalog lifecycle hooks remain.
  Local ARM regressions verify cash presentation outside supported contexts,
  savings presentation within them, amount feedback and teardown. These model
  the layout/engine boundaries. The user subsequently confirmed the restored
  counter works. That recovery retained the earlier stepped icon corners.

- 0.3.11 uses native `spi1` shape resources and PIC flag bit 0, with three
  RoundRectShape records for white backing/dark edge/green face. The component
  shrinks to 29 panes/69 sections/7,444 bytes, below the live-confirmed recovery's
  budget. Original panes, materials, textures and animations are preserved.
- The 0.3.11 ARM test executed the original section reader at 00268550, PIC shape
  selection at 00282D9C, shape resolver/constructor and RoundRect mesh generation
  from the exported resource at two ASLR bases. Only allocation and imported
  sinf/cosf are modeled; loader relocations are supplied. Each silhouette yields
  84 vertices and 210 indices, with checked radius normalization, corner-circle
  positions, index bounds and allocation guards. It does not execute the full
  layout constructor, material/shader setup or GPU drawing; an Eden playtest
  is required to establish appearance and continued visibility. Raster preview
  supersampling is illustrative and does not prove native antialiasing.

- The user reported 0.3.11 no longer looked like a card. Its screenshot shows
  white shapes/detail stripes with no green face or dark edge. Mesh-only tests
  missed the native drawing path's color behavior: Picture's shape branch at
  00283E0C selects material mode 2 and skips the quad vertex-color constants.
  All three silhouettes shared the solid-white material, so their pane colors
  could not supply the intended dark/green colors.
- 0.3.12 adds a separate foreground material for each silhouette, with white
  pane colors. Layout count stays 29 panes/69 sections; size is 7,588 bytes.
  The 0.3.12 tests executed native material decoding (002745C0), Picture drawing setup
  (00283B50) from entry to return, material setup (00278410), and foreground constant writes (00278BDC)
  at two ASLR bases. Deliberately conflicting pane colors leave the correct
  normalized white/dark/green material constants; a shared-white material
  reproduces the 0.3.11 failure. Existing mesh, index/area/guard checks remain.
  Shader initialization/selection, transforms, constant-buffer allocation,
  matrix binding and GPU upload/draw are modeled;
  those CPU tests do not establish live appearance or antialiasing. The raster
  preview now reads material colors for shapes instead of their ignored pane
  colors. The user subsequently reported split, distorted card geometry.
  These shape-path tests did not establish GPU rendering and are superseded
  by the texture checks below.

- 0.3.13 abandons the unused native shape path. Two ordinary 136-byte PICs
  clone the native Bells icon's material/sampler and full-image UV coordinates.
  The entire foreground card is one RGBA8 image; its white backing is another,
  drawn before the oval. The layout has 24 panes/63 sections/7,100 bytes.
- Both textures are appended to the embedded BNTX. Original seven payloads and
  format/dimension/channel/tiling fields remain byte-identical. Checks cover
  required relocation pointers and their section bounds, both new descriptors,
  full independent block-linear decoding, stripe/chip/detail colors and partial
  alpha coverage at rounded corners. Native 3.0.3 dictionary lookup (00263368
  and 00263410) resolves all original and added texture names at two bases and
  rejects missing names; strcmp is modeled. The actual hash-checked 3.0.3 SDK
  relocation routine (00081A90 in SDK) now relocates both original and extended
  containers and agrees byte for byte with the relocation model (including the
  relocated flag). SDK NSO segments are decompressed and hash-verified locally.
- The preview resolves exported PIC/material/texture references and decodes
  packaged image bytes. Its oval is illustrative. GPU loading/sampling/drawing,
  native output colors and continued counter visibility require an Eden check.

- The user reported all currency display missing in 0.3.13. The native RSTC
  entry for `Layout/LBellCounterRT.Nin_NX_NVN` is 77,952 bytes; vanilla expands
  to 69,760, 0.3.10/0.3.12 to 73,856, 0.3.9 to 98,432, and 0.3.13 to 463,528.
  This is a concrete allocation mismatch missed by prior structural tests.
- 0.3.14 adds a trampoline at native resource-size lookup 03E3CAD0. It executes
  the original lookup, then raises only the exact normalized counter path to
  at least 1 MiB. Larger sizes and existing metadata are preserved; missing
  entries receive the floor with flag zero. No shared RSTC file is replaced.
  The builder checks expanded resource size plus native overhead fits the floor.
- The new resource ARM test executes actual CRC32 and RSTC binary search with
  the local original table and compiled callback at two bases. It reproduces
  the insufficient original budget, checks the new floor, exact-path isolation,
  larger overrides/metadata, missing table, disabled behavior and removal.
  Resource-entry fingerprint conflicts reject support and all behavior.
- Resource preparation is independent of financial/UI behavior: while the
  package marker remains installed, its enlarged assets can still load with
  disabled, invalid or incompatible configuration. Preparation is once-only;
  absent packages install no hook. Core subset/metadata/overlay tests cover
  this exception without enabling transactions or savings presentation.
  Actual live counter visibility and GPU rendering still need an Eden check.

- The user confirmed 0.3.14 works. 0.3.15 changes only foreground texture
  pixels: detail-line centers move from Y -12/-19 to -6/-13 and the border
  changes from linear RGBA (10,12,8,255) to (5,18,16,255), a dark blue-green.
  Original textures, backing, layout structure, allocation support and payment
  behavior stay unchanged. Existing packaged-pixel checks follow the new centers;
  the exported texture preview is inspected before packaging.

- 0.3.16 raises the line centers another 1.5 layout units to Y -4.5/-11.5
  (one screen pixel at 1280 width) and changes the border to linear RGBA
  (12,50,42,255). The previous shade still appeared black in-game. Only the
  foreground texture pixels change; the packaged preview is inspected locally.
  Final color and placement still need an Eden check.

## Conditional dialogue

- Host settings/policy checks cover defaults, friendly aliases and resource IDs,
  case-insensitive deduplication, UI/player independence, no stacked boost, 64-ID bounds,
  invalid values/keys/list entries, empty lists, duplicate keys and size limits.
- 6,454 compiled ARM cases at two ASLR bases execute the original native input,
  reveal and renderer gate code with the real compiled SIMD adapter/callbacks.
  No-input results match native held fast; B/R uses the extra tier. Protected
  modes, forced native display, disabled acceleration, scripted overrides,
  countdown handling and parser call counts remain unchanged.
- Input matrices include B/R, excluded L, unrelated buttons, prior latch,
  active/fallback/missing controllers and release. Controller bytes are unchanged.
  Main/renderer matrices use five live fast rates, scales and completed states.
- Native-produced owned speaker handles and modeled registry entries cover rcm/rct/dod/doc/kpp,
  kppB, another NPC, a villager, player narration, dialog/balloon/unknown,
  missing or invalid speaker, stale generations, retiring entities, unrelated
  entity kinds, NPC RTTI mismatch/uninitialized/missing/foreign methods, and
  overlong resource names. Sequences switch speakers and
  repeatedly hold/release; borrowed speaker/registry bytes remain unchanged.
  Kapp'n's friendly alias includes both known SpNpcParam profiles.
- Player cases execute the original 01AF89A0..01AF8A84 pending-to-active
  bank copy, then the compiled classifier through the actual SIMD adapter.
  They cover all five self banks, missing speaker entities, UI/player settings
  independently on/off, B/R non-stacking, ignored pending IDs, transitions to
  unknown/NPC banks, bounded/overlong names, and read-only metadata. Known self banks use the player
  category even when the presentation is a system dialog. All five bank names
  were found in each of the 14 localized TalkFtr/TalkSys resource sets.
- Speaker metadata lookup and parser completion are modeled; the owned handle is produced by original constructor instructions. Hook
  allocation/trampoline relocation is modeled; the actual SIMD adapter and C++
  callbacks execute. These tests do not validate physical controllers, audio,
  emotes, complete rendering or actual game context coverage in Eden.

## Existing behavior against the unified ELF

The inherited tests were adjusted for the new source/translation paths and
combined ELF. The Pockets fixture's synthetic adapters were moved beyond the
larger module's BSS; this was a test-memory placement change, not a gameplay fix.

- 192 version comparisons plus valid/invalid config boundaries passed.
- 392 stack-menu cases and 864 storage-policy cases passed.
- All fourteen caption locales, fallback handling, UTF-8, capacity and atomic
  parser rejection passed.
- 372,075 fitting-bag capacity/policy cases passed.
- 630 compiled Pockets ARM cases and 190 storage-hint cases passed at two bases:
  native Split, menu dispatch, translated captions at the new path, withdrawal,
  Y-to-storage lifecycle, native footer allocation and caption routing.
- Fitting Room native checkout tests passed at two bases: exact variants,
  outfit/bag overlap, wear/send choices, storage/pocket/mixed delivery, tickets,
  purchase records, metrics, repeat guards and preflight rejection. Engine/save
  APIs are simulated. Native scale/dirty-transform, alpha/removal and cursor-tap
  feedback checks also passed.
- 1,926 dialogue/ground catalog cases, 44 UI-result source cases, 148 variation
  label cases, 32 Miles name-binding cases and 54 counted-name cases passed.
  Real fence metadata/name types were exercised. Imported catalog fixtures also
  exercise their prerequisite native query, naming and badge checks.
- 26,077 fitting catalog-star cases passed, including all 5,207 clothing IDs,
  native selection frames for eight kinds, real USen names and two load bases.

These tests use original local game instructions/resources and modeled engine
boundaries. They do not confirm rendering, physical input, audio, real purchases,
real delivery/catalog saves, or complete emulator behavior.

## Packaging

- Exactly seven mod folders: Core, Catalog, Pockets, Faster Dialogue, Fitting Room,
  Amiibo and Shopping Card. Six versioned INIs; only Core has ExeFS files. The actual personal
  custom INI is absent from the suite; a root .example is inert. A separate
  optional personal-mod ZIP contains the requested preset and no executable or
  module marker, so replacing the suite folders cannot overwrite preferences. Optional modules are
  absent from Core's own RomFS, so removing/disabling one removes its marker.
- Six layout payloads match the current standalone binary ZIPs byte for byte,
  checked against independently captured SHA-256 values. A seventh layout
  belongs to Shopping Card and adds the initially hidden savings presentation.
- Catalog badge builders verify original resource hashes, archive changes and
  hidden initial badge panes. Fitting builders preserve other archive members.
- Binary ZIP contents and SHA-256 manifest are checked. Matching source excludes
  game resources, binaries, build outputs, Git data and caches. Packaging refuses
  to overwrite an existing release.

## Reproduce

From the workspace root, build with `sh qol-enhancements/tools/build.sh`.
Then run `python3 qol-enhancements/tools/check.py` and
`python3 qol-enhancements/tests/package_test.py`. The ARM emulator requires JIT
access outside the Codex filesystem sandbox on this Mac. Its Python packages are
currently at `Grab 5/working/python`; original local main/resources and the zstd
CLI are also required. None of those original game assets are redistributed.

## Required live checks

1. Launch Core alone. Confirm ENTRY, WAITING_FOR_CONTENT and BOOT_OK with zero
   enabled modules, and unchanged gameplay. Launch all six modules and confirm
   all package versions, ENABLED lines and BOOT_OK without installation errors.
2. Disable/re-enable each module in Eden, fully restarting each time. Verify
   code behavior and resource replacements follow the module toggle. Check a
   below-minimum INI and a newer INI produce the documented error/warning.
3. Check Catalog badges, scrolling, variants, Miles, recipe/ground stars and
   dialogue item sources. Check Pockets Grab 5/10, caption translations, native
   storage withdrawal, Y storage and footer hints with each module alone and
   together.
4. Test Faster Dialogue without a custom mod: base 2, held B/R 4, release and
   disconnect, multi-page dialogue, choices, A/B behavior, Animalese and emotes.
   Then enable the separate personal preset and restart: verify automatic 4
   for Timmy and Tommy independently, Orville, Wilbur, and Kapp'n (including
   alternate profile if encountered), plus blue UI/ABD dialogs. Independently
   enable faster_player for DIY workbench prompts/results, fish/bug/sea-creature
   catches and recipe learning; turn faster_ui off and check those self messages
   still boost. Reverse the toggles and check blue system dialogs separately.
   The corrected NPC diagnostic was confirmed working; verify
   unlisted villagers remain at base and no prior NPC boost carries into the
   next message. Check custom file edits, removal, module disabled, unknown IDs
   and invalid values across full restarts. Check regular-file preferences,
   custom inheritance and enabled overrides for other modules in 0.2.4. Kapp'n songs/travel should retain
   native scripting; UI recognition may need additional verified contexts.
   Check intended FPS-mod combinations separately; dynamic rates are retained
   but static hook fingerprints do not establish complete compatibility.
5. Use the existing save backup for Fitting Room purchases: bag plus outfit,
   overlap, 17–20 items, storage overflow, wear/send and Bell deduction/delivery.
   Confirm badge animation and catalog-star interaction in the fitting UI.
6. Check Amiibo late scans, timeout, B cancellation and retry. The standalone
   timer was confirmed at five minutes; unified runtime installation is new.

Switch hardware and other emulators/game versions are not validated targets.

7. Shopping Card: first confirm both ordinary pocket-Bells display outside shops
   and savings display in shops/catalog return after 0.3.9's disappearance.
   Check entering/leaving shops, opening pockets and opening/closing Nook Shopping.
   Use the existing save backup. Test a floor item and cabinet
   item in both Nook's Cranny versions, clothing from Mabel and the fitting room,
   then the fitting bag with both modules enabled. Compare savings at the ABD
   before/after; confirm pocket Bells and Bell bags do not change. Test cash
   rich/savings poor and cash empty/savings sufficient, cancellation, tailor
   tickets, full storage/pockets, wearing/sending, and several residents. Sell
   items/turnips at the counter and verify the deposit and ordinary thank-you message. Check
   drop-off box, ABD transfers, Harv and another vendor retain normal behavior.
   Close/reopen pockets repeatedly without a transaction; verify no false
   sound/amount animation. Check green savings display at zero and large
   balances, thicker white oval outline, centered dark-edged card and its white
   backing visible only above the oval; check the restored full-height card
   with the earlier stepped dark/white corners
   and the lower-right white Bells badge with green text shifted left about 16 pixels.
   Check at 1280x720 (16:9) and 1280x800 (16:10 with the existing aspect mod),
   including top/right margins, clipping, icon proportions and frame pacing.
   Keep any later 16:10 placement correction separate from the default package.
   Verify purchases count down with ka-ching, sales count up, rapid changes settle
   correctly, and first entry/resident changes stay silent. Check leaving both
   shops restores cash; pockets still show cash.
   At Nook Stop and in the phone app (including outdoors), first verify the
   savings display appears without visiting Nook’s Cranny. Order special goods
   and previously cataloged furniture/clothing for yourself. Compare savings,
   pocket Bells and delivered mail; check order limits, cancellation, insufficient
   savings despite rich pockets, empty pockets with sufficient savings and Bell
   Boom prices. Verify the card HUD and downward animation/sound, closing to cash,
   reopening silently and switching residents. Gift payment is unchanged and
   needs a separate implementation; check the native gift flow for regressions.
   Restart with Shopping Card disabled/removed and verify native payment.

## Optional Shopping Card colors — October 9, 2026

- The user confirmed the 0.3.16 green appearance. Add a gold option without
  changing the Core version or transaction hooks. The original Bells caption
  material supplies linear RGBA (130,97,16,255); its rim material supplies
  cream (255,241,195,255). These exact colors are checked against vanilla for
  the gold counter/caption and rim/badge. The card face uses the same gold,
  with a darker gold edge (70,48,6,255), cream stripe/details and gold chip.
- Existing layout checks pass for both palettes: original panes/materials,
  font/animation resources, 24 panes/63 sections, 7,100-byte BFLYT, native 16:9
  placement and scoped resource-budget guard. No geometry changes.
- Gold passes packaged pixel decoding, smooth corner coverage, original seven
  textures preserved and actual SDK relocation/native texture-name traversal
  at both existing bases. Its exported pixel preview was inspected. GPU color
  and final appearance still need an Eden check.
- The existing suite/package check passes against the untouched 0.3.16 ZIP,
  including byte-identical green layout. Optional Gold/Green choices replace
  only that layout inside the existing Shopping Card folder; their archive
  entries and SHA-256 manifest are verified after writing. Green restores the
  user's confirmed appearance. No executable or module configuration included.

### Color options revision 2 — lighter bell-bag card

- The user requested a lighter card matching the bell bag rather than the
  counter. Sample the light body in the supplied Nook Shopping screenshot:
  median sRGB (249,191,116), approximated in linear RGBA as (242,133,45,255).
  Apply this to the gold card face only.
- Existing gold structure/native-palette checks pass. Comparing against the
  revision 1 optional ZIP confirms only foreground card texture colors change:
  descriptors, alpha coverage, backing, counter/badge, layout, original
  textures and other resources are identical. All changed RGB channels are
  lighter. The exported preview was inspected; live appearance remains pending.
- Package as a separate r2 archive, preserving revision 1 and the 0.3.16 suite.
  The packager verifies the green restore option against the original release
  and verifies every ZIP entry after writing. Core remains 0.3.16.

### Color options revision 3 — requested #F9C687 face

- Use the user's selected sRGB #F9C687 (249,198,135), converted to linear
  RGBA (242,144,62,255), for the gold card face. Only this palette entry changes.
- Existing gold layout checks pass. Comparison with the r2 optional archive
  confirms identical layout/resources, texture descriptors and alpha coverage;
  only the foreground card's RGB pixels differ. The decoded face center
  converts back to exactly #F9C687. The exported preview was inspected.
- The r3 archive preserves previous downloads and includes the byte-identical
  green restore option. Core remains 0.3.16; live appearance remains pending.

### Color options revision 4 — Hotel Blue and Poki Red

- Add Hotel Blue and Poki Red alongside the existing green and gold options.
  Hotel screenshot medians: counter sRGB #4575F5 -> linear (15,45,233),
  ticket #7094F9 -> (41,76,242), red accent #EB4E4C -> (212,19,18).
  Use these for counter, card face and stripe, with navy #354F83 edge.
- Poki bag screenshot median #F8A486 -> linear (239,95,61) supplies its coral
  card face. Pair it with deeper warm red #D96459 counter and #833F35 edge;
  preserve cream details and the gold chip. Both themes retain Bells/savings
  behavior. No Poki or hotel currency hooks are added.
- Allow a separate optional stripe color, defaulting to the detail color for
  existing palettes. All four pass existing layout checks. Preview each new
  theme from the serialized materials/textures and inspect locally. Native
  texture tests cover each new palette's face/edge/chip/stripe/details and
  independent byte-order decoding, original textures and SDK relocation/name
  lookup. Actual in-game appearance remains pending.
- Revision 4 includes all four choices with readable Hotel Blue/Poki Red folder
  names. Green must match the main 0.3.16 ZIP byte-for-byte; green/gold also
  match revision 3. The package verifies all entries and its checksum manifest.
  Previous archives and the Core executable remain unchanged.


## 0.3.17 localized NPC names

- Two new C++ observation hooks execute during native name-label preparation,
  alongside the unchanged five speed/input hooks. No name lookup, message
  rendering, voice, emote or delegate behavior is replaced by the hooks.
- Tests execute the original name-holder construction, source+128 validity and
  dispatch, and actual UTF-16 data/capacity getters through the real SIMD hook
  adapter. Only the resource lookup inside the name delegate is modeled.
- 6,916 name cases passed. Real special NPC and villager names from all 14
  supplied locale archives feed
  the native holder and compiled capture code. Russian grammar tags with embedded
  zeros are removed. No extracted name tables are included in the release.
- Host checks cover valid/malformed UTF-8, UTF-16 surrogate pairs, missing
  terminators, malformed/oversized tags, bounded conversion, Unicode list entries
  and unchanged regular/custom configuration layering.
- Compiled name matching covers Blathers and accented/Japanese/Chinese/Russian
  names, names in either INI, owned copies after native stack reuse, unavailable
  labels, mismatched source and generation, retiring entities, UI/player scope
  and language-independent internal IDs. Existing aliases remain available.
- The prototype still requires a live Eden check: add Blathers to the existing
  personal list, restart and compare his text without/with B/R; also check a
  listed villager and an unlisted speaker. Alternate-language name capture is
  locally tested but has not been confirmed in a live game.


## 0.3.22 unnamed dialogue

- Rename the public preference to `faster_unnamed`, preserving `faster_player`
  as an alias. Host checks cover both spellings, cross-file overrides,
  malformed values and mixed-alias duplicates retaining the earlier layer.
- Reuse the existing seven C++ hooks. Name capture observes the same native
  null-data/zero-capacity/initial-NUL branches that suppress the name pane.
  Nonempty malformed names remain named even when UTF-8 conversion fails.
- Native ARM checks cover all eight reported message groups plus an unlisted
  group and variable labels in talk/balloon windows, both alias spellings,
  disabled settings, absent sources, independent blue UI, unobserved name
  preparation and message-bank/label/speaker-generation changes at two bases.
- The new longer bank cases exposed overlapping mock string/vtable storage;
  move the pending-bank test buffer away from the fake vtable. Production
  code does not use those test addresses.
- Existing Core activation/configuration and 6,890 dialogue reveal/input/rate
  cases pass, along with 6,916 localized name cases across all 14 supplied
  languages. These are local ARM checks; the new category still needs an Eden
  test with the actual mirror/storage/Niko/amiibo/stretching/drop-off prompts.


## 0.3.23 combined Shopping Card funds

This revision supersedes the earlier savings-only affordability and no-fallback
notes. Shops, fitting/bag checkout and regular catalog orders now check the
combined total and spend Savings -> pocket Bell bags -> Wallet. Sales still
deposit to savings. Cash change/refunds use native routines and may repack
remaining cash between wallet and ordinary bags. Capacity stays conservative.

- Host policy: 5,040 boundary cases, all three source priorities, insufficient
  combined funds unchanged, zero/exact/max amounts and debit/refund conservation.
- Compiled callbacks: all eight residents, two bases, every supported payment
  caller, 500k example, partial wallet fallback, change and overflow.
- Actual native cash/savings wrappers, selection/debit/change, addition/overflow
  and encrypted money writes execute at two bases for all eight residents.
  Resident/parameter lookup and inventory iteration/insertion are modeled.
  Ordinary non-money inventory items survive all cash operations.
- HUD shows all sources and clamps the displayed target at 999,999,999. Full
  funds remain spendable, including withdrawing a total above the display cap.
  The original nine-digit layout and all color options remain compatible.
- Native cash feedback is formatted silently while its card is visible; the
  card's native transition supplies one animation/sound. Leaving card scope
  restores native cash feedback. Existing silent pocket refresh coverage remains.

Eden still needs mixed-funds purchase tests in Nook's Cranny, Able Sisters,
fitting bag, Nook Stop and the phone app. Include full/near-full inventory,
change, insufficient total, cancellation, multiple residents and sale proceeds.

Release checks: the devkitA64 build and `tools/check.py core shopping fitting`
passed. All five palette structure checks and suite manifest/resource checks
passed; generated assets retain the previous nine-digit layout bytes.

## 0.3.24 DIY Recipes app previews

The feature is built into Core, with no optional module or new RomFS asset.
Selecting a recipe opens a phone-owned instance of the native workbench detail
page. It borrows the current app list for native left/right navigation and
material/item lookup. A private virtual table disables the craft focus target
and rejects crafting actions; the nested craft-button layout root is hidden.
B runs the return animation, releases the borrowed list and restores the app's
category, selected cell and scroll position. Native workbench objects are intact.

Local checks execute the compiled integration at two ASLR bases, with modeled
UI allocation, loading and animation completion. The actual game code handles
recipe copying (including all six material entries), list count, left/right
wrapping, item lookup, favorite-command forwarding and craft-action rejection.
Additional cases cover sorted/filtered pointer order, one-item lists, invalid
selections, allocation failure, duplicate confirms, reuse, per-resident sessions,
active/already-removed child cleanup, and all instruction-fingerprint conflicts.
Core's 64 package combinations and configuration/compatibility checks pass with
the three added built-in hooks. A preview mismatch leaves amiibo and optional
modules independent.

The 0.3.24 pinned Docker build passes with `-Wall -Werror`. All host checks and
the Core, Shopping Card, Faster Dialogue, Faster Crafting, Pockets, Fitting Room
and Catalog regression groups pass against that build.

Live Eden verification remains pending for model/resource loading, visible
layout/layering, rotation, physical input, B during loading, repeated app/phone
closure and normal workbench crafting/cooking. The local harness does not
establish those results. See `research/recipe-preview/NOTES.md` for native bindings
and the live test list. No running game, installed mod folder or save was changed.

## 0.3.25 DIY preview callback binding correction

The user reported that A in the phone app did nothing in 0.3.24. The callback
scanner rejected tables with more than 32 rows, but native layout initialization
at main+03759DD0 creates 35 (0x23) button-event rows. Consequently it never
replaced the phone's empty confirmation callback. The scanner now accepts the
native row count while retaining bounded dimensions.

The original test fixture supplied six rows and invoked the callback directly,
so it missed this integration failure. The revised fixture executes the actual
native layout-table constructor, asserts two columns/35 button rows and two
columns/five command rows, and feeds confirmation through native translation
(0375AB74), dispatch (0375AA40), and member-function delegates. Only allocation,
resource binding, source-index lookup and focus feedback are modeled at those
boundaries. The revised table fixture fails against the 0.3.24 executable and
passes against 0.3.25. Preview lifecycle/navigation/material/craft-rejection and
Core activation checks remain covered. The pinned Docker build and all host/Core
checks pass, including all 64 optional-package combinations at two load bases.
Live Eden input/rendering verification of this corrected build remains pending.


## 0.3.26 DIY preview construction/heap correction

The user reported a freeze on Flimsy axe with the phone grid still visible.
Their 0.3.25 Eden log shows a null instruction call, LR=83F1E0F8. With main
base 80163000, the native frames resolve to 03DBB0F8 (state-name assignment),
03363648 (first item-icon constructor), and 03492CAC (preview constructor).
The state-name array pointer is null before layout/resource binding occurs.

Native state initialization at 03DBAC90 uses TLS+0xA8 when a thread record
exists; it does not fall back to the default heap when that field is null.
A phone UI manager can inherit its heap (manager+0xB8 is null). The old scope
then left TLS unchanged and used libc malloc for the page. A null current TLS
heap reproduces a null array read at 03DBB0EC, immediately before the null call
in the supplied backtrace. The log does not contain the live heap values, so
this reproduces a sufficient cause rather than establishing allocation history.

The revised scope chooses the manager heap, then the current TLS heap, then
the heap manager's default heap. It restores the previous TLS value even when
that value was null. The page itself uses heap virtual+0x30 with alignment 8,
matching native page factories. Missing heaps/allocation failure restore phone
input instead of entering construction. Confirmation queues the operation;
creation and layout visibility changes run after native input dispatch returns.
Bounded stage messages identify heap selection and construction/binding/loading
progress if live verification exposes another dependency.

The regression executes actual native base/common preview construction, all
270 icon constructors, their state-machine allocations and state-name setting.
Only allocation backends, string assignment and low-level mutex/memory services
are modeled in this constructor path. It reproduces the original null-heap
fault independently, then checks successful construction using manager,
current and default heaps at both load bases. It verifies state arrays/names,
page bounds/ownership and TLS restoration. The previous model had stubbed out
icon construction and therefore missed this dependency.

The pinned devkitA64 Docker build passes with `-Wall -Werror`.
`python3 tools/check.py core` passes all host checks, all 64 module combinations
at two load bases, configuration/compatibility tests and the expanded native
preview regressions. Suite/source/custom ZIP integrity and suite SHA256 manifest
checks pass. Older release archives are preserved.

Live Eden verification of 0.3.26 remains pending. This fixes the reproduced
construction dependency; it does not prove rendering, rotation, asynchronous
resource loading or actual controller/B behavior in Eden. No installed mod,
running game or save was changed.


## 0.3.27 DIY preview initialization timing and active-page updates

The synced `logs/eden_log.txt` reports Core 0.3.26. Every selection logs
`allocating page` followed by `could not create native page`. The manager heap
is nonnull (214E07D318) and previous TLS heap is null. No preview constructor or
layout-binding stage is reached. This establishes allocation failure in the
live app, rather than missing heap selection or an item/model-specific fault.
The 0.3.26 null-TLS reproduction remains a defensive test; it did not establish
the live 0.3.25 allocation history. The app's UI pool was already shrunk, which
also explains why 0.3.25's libc-allocated page failed on internal allocations.

Native UI manager initialization at 0376D9A0 creates an ExpHeap (037C6D48),
builds/initializes its pages, then invokes heap virtual+0x28 = ExpHeap::adjust
(037C75D4). Adjustment returns unused capacity to its parent. The new Core hook
on page initialization (0376890C) identifies only the MenuRecipePage vtable and
prepares its native preview child during this setup interval. It leaves the
preview inactive (lifecycle 2) and activates it only after confirmation dispatch
finishes. This uses the app's reserved construction heap and native ownership;
it does not increase a global resource budget or allocate on first selection.
Failure leaves app input/layout available, with no layout-root hide/show calls.
The preview's layer is set one above the phone layout, matching native factories.

Actual child activation (03769260) clears the parent's +0x164 update flag. The
previous fixture omitted that behavior and incorrectly allowed the phone to
drive preview transitions indefinitely. Transitions now run from the preview's
own update override. After B's return animation and idle reset, native child
removal (03768C7C) returns lifecycle to 2. Restore the parent update flag first,
then detach from the parent's own update after child-callback iteration returns;
removal inside the child callback would invalidate that iterator. Input is
restored after detachment. The prepared child can be reused without further
UI allocations.

The expanded regression executes concrete ExpHeap creation from a buffer
(037C6FFC), allocation (037C77A0), and adjustment: preview-sized allocation
succeeds before shrink and returns null afterward. Base heap bookkeeping and
the parent's backing-block resize are modeled; the free lists/capacity are real
native code. Integration tests forbid allocations after modeled UI setup and
exercise actual child activation/removal with only callback registration and
engine entry/exit services modeled. Parent updates are forbidden while paused;
opening, navigation, B return, restored app updates, reuse and cleanup still pass.
The existing native constructor, data, dispatch and fail-closed checks remain.

The pinned devkitA64 Docker build passes with `-Wall -Werror`.
`python3 tools/check.py core` passes all host checks, all 64 module combinations
at two load bases, configuration/compatibility tests and the expanded native
preview tests. Suite/source/custom ZIP integrity, suite manifest hashes and
binary/source parity checks pass. Older releases are preserved.

Live Eden verification of 0.3.27 remains pending for full resource loading,
rendering/layering, rotation, controller input, app/phone reopening and ordinary
workbench crafting/cooking. The native tests do not establish these live results.
No installed mod, running game, save or synced log was modified.


## 0.3.28 DIY preview initial-state stall and app reopening

The synced Core 0.3.27 log reaches successful construction, layout binding,
child creation and `activating prepared page`; it contains no crash/backtrace.
The user reports that buttons stop responding while background music continues.
The last progress message alone does not establish a hang inside activation.

A native execution reproduction establishes the opening-state defect:
034942F0 constructs the preview's state table and leaves it in `cInit` (0),
with both request slots empty. Its 03494768 update sets a resident capability
flag on the first frame and never transitions to `cIdle` (1). The 0.3.27 compiled
integration waits for state 1 before requesting appearance, so the reproduction
leaves both request slots empty indefinitely with app input suspended. Earlier
activation fixtures incorrectly forced state 1 and masked this condition.

0.3.28 accepts settled initial state 0 and idle state 1, matching the native
appear-readiness contract, and calls the native +0xF0 readiness query before
requesting appearance. A regression holds layout readiness false and verifies
that no dataset is borrowed or appear request queued until readiness returns. Reused pages still wait for idle entry to finish
before borrowing the app's recipe list. A post-activation log records recipe
state and page lifecycle, distinguishing an activation failure from a later
transition failure. No crafting or navigation behavior was changed.

02E9D538 is the app's deactivation callback, not page destruction; native code
only removes its layout from the rendering registry. Discarding the session
there made subsequent app opens lose their prepared preview. The session now
retains its owned child and queues idle to cancel unfinished transitions.
A newly initialized phone page replaces any old resident session. Callback
binding does not depend on a previous session's address. Tests cover reopening
without additional allocations and replacement of a resident's page.

The expanded regression executes native state-table initialization and initial
updates, actual activation begin/end wrappers, recursive update eligibility,
callback registration and unregistration, and child removal. It verifies state
0 remains unchanged by activation, the compiled integration queues appearance,
and callback/active lists recover their sentinels after removal. These run at
both ASLR bases. The resource/rendering service boundary, native manager frame
scheduler and asynchronous transitions remain modeled. In particular, the
0.3.27 assumption of a permanently paused parent was too strong: 03768DB0
recomputes flags and advances lifecycle 3 to 4 on subsequent manager updates.
The existing tests that artificially keep lifecycle 3 cover immediate removal;
they do not establish live manager scheduling or deferred lifecycle-4 removal.

The pinned Docker build and `python3 tools/check.py core` pass. Suite, source and
custom-example archive integrity, suite manifest hashes and deployed/source
parity are checked before release. Older archives are preserved.

Live verification is still required for appearance, model rotation, controller
navigation/B return, repeated app/phone reopening and ordinary workbench
crafting/cooking. This release fixes a reproduced opening stall; local tests
cannot establish that every live preview issue is resolved. No running game,
installed mod, save or synced log is modified.

## 0.3.29 DIY preview appearance-to-selection stall

The synced 0.3.28 log confirms successful activation (`state=0 lifecycle=3`)
and `appear requested`, then ends without a selection message or native crash.
The initial-state fix gets past the first stall, but the next wait is wrong:
the integration expects settled state 3 after appearance. Native 03494B08
instead requests state 5 (`cDisplayList`). Its update, 0349553C, does nothing;
the owner must supply the selection and request state 4 (`cChangePlay`). That
state loads the recipe and requests state 3 (`cPlay`) after its animation.

The new regression executes native state initialization, page-update virtual
02B0C720, deferred-request visitor 02B0E370, and state entry/update/exit handlers.
Against the 0.3.28 binary it records states 2, 5, 5, 5, 5 and fails. Version
0.3.29 selects from settled state 5; the expected opening sequence is now
2, 5, 4, 3, 3. The existing modeled flow fixtures also use state 5 instead of
artificially setting state 3 immediately after appearance.

The native transition regression checks the selected index, model reload
request, empty deferred slots and enabled preview input. It also exercises B's
play exit/display-list entry, native idle cleanup, return of app input and
reopening of the same page. UI type identification, icon/detail rendering,
sound and animation services are modeled. Manager scheduling and actual
rendering remain outside this test. Added fingerprints cover the native appear
and selection updates; conflicts disable only the preview feature.

The pinned Docker build and `python3 tools/check.py core` pass, including host
policy checks, all 64 module subsets at two ASLR bases, Core configuration and
preview regressions. Release archive integrity, manifest hashes, deployed Core
parity and source parity are checked before delivery. Older archives remain.

Live Eden verification remains required for visible appearance, item rotation,
controller navigation/B return, app/phone reopening and workbench crafting.
No running game, installed mod, save or synced log is modified.


## 0.3.30 DIY preview background, model publication, craft button and B

The synced 0.3.29 log reaches `showing`. The user's screenshot shows populated
materials, the phone grid inside the captured model area, and “Need materials!”;
B then stops input. Opening-state regressions alone did not cover these paths.

Native WorkCraftInfoSeq owns the info UI, recipe icons and quantity/dialog
children; its +0x1c1e8 child is a quantity selector, not a 3D renderer. The
workbench owner separately publishes the selected recipe to the shared
ModelOnUI context (0347A600), and owns WorkCraftBgSeq. Core now prepares the
native background before heap shrink and uses the native model publication
routine on initial selection and navigation. It saves/restores the model data
when returning, hides/restores the phone layout, and retains native BG layer 14
and info/capture layer 23. Installed WorkBenchBG/WorkBenchInfo assets are loaded
normally; Core ships no replacement of either, including no fixed aspect ratio.

Craft-button +0x1b0 was incorrectly treated as a layout root. Native 03471FF0
shows its layout roots at +0x2e0; visibility is now changed on that root's
controller. The test poisons the previous wrong field. Craft rejection and
removal of the focus target remain independent safeguards.

Native cDisplayList reverses content channel 3 without hiding the entire layout.
Core now uses the same channel-busy query as 0349530C before idle and detach.
The regression keeps the root visible, holds a non-null animation busy, then
releases it and verifies input restoration and reuse. Native model publication,
BG state initialization/updates, recipe navigation and context restoration run
in the ARM harness at two ASLR bases. Resource loading/rendering and animation
services remain modeled; the tests do not prove visible 3D rendering, rotation,
16:10 appearance or controller behavior in Eden.

Validation: pinned Docker build, `python3 tools/check.py core`, archive integrity,
manifest hashes, deploy/source parity. Live Eden retest remains required for
model appearance/rotation, absence of the craft button, 16:10 layout, B back to
the same recipe grid, app reopening and normal workbench crafting/cooking.
No running game, installed mod, save or synced log is modified.


## 0.3.31 DIY app startup crash in button hiding

The synced 0.3.30 log records the first fault at 78.001 seconds: execution jumps
to heap address 0x214E360B60, with Core return offset 0x85EC. Disassembly of the
shipped ELF maps that return to the visibility virtual invoked by BindPreview.
The button +0x2e0 array contains text-binding records, not nested layout roots.
Reading a record's +0x1b0 as a visibility controller calls heap data as code.
The old regression fixture invented a root at this position and masked the bug.

The corrected fixture places a real binding/adapter/payload/method-table shape
at +0x2e0 and a heap-data call target in the previously miscast field. Running
it against the unchanged 0.3.30 ELF reproduces a CPU exception during app setup.
The revised C++ follows the same typed text-binding interpretation used by
Fitting Room's PricePane: SetPaneText identifies a Pane; SetGroupText identifies
a bounded member group that must first be resolved to a Pane. It then walks
at most 16 parents to RootPane/L_CraftBtn_00 and clears only Pane +0x58 bit 0.
Unknown bindings leave visibility unchanged; crafting/focus rejection remains.
No guessed visibility virtual is called.

The rebuilt 0.3.31 passes direct/group binding coverage, unrelated-flag
preservation, unknown-binding handling, opening/model/navigation/B/reopening
regressions and all Core checks at two ASLR bases. Archive integrity and source/
deploy parity are checked before delivery. Live Eden rendering, 16:10, rotation
and B behavior remain unverified. No game install, save or synced log is changed.


## 0.3.62 extended dialogue speed settings

- Both INI layers accept canonical whole numbers 1..10 for base_speed and
  boost_speed. Defaults remain 2/4. Host coverage checks all 100 combinations,
  lower-boost normalization after layering, duplicate restoration, and rejected
  zero/out-of-range/overflow/signed/fractional/NaN/infinite values.
- Keep the existing seven C++ hooks and flags 1/3. These identify base/boost;
  both main and renderer callbacks scale the live fast rate by the selected
  configured multiplier / 2. No numeric multiplier becomes an engine flag.
- ARM coverage exercises all 100 pairs loaded from each INI form at two ASLR
  bases, five live fast rates, press/release, both original renderer forwarding
  stores, renderer completion and scripted overrides. It also checks 4/8
  automatic NPC/UI/unnamed selection without stacking, and maximum-speed
  forced/protected/parser-mode/message-state behavior against native code.
- The original 2/4 regressions and localized-name checks remain required.
  These are local emulation checks; perceived speed, Animalese and emote timing
  at the extended rates still need an Eden test. Fixed waits remain native.

## 0.3.64 villager crafting recipe dialogue

- `NNPC_Reaction_DIY` Event54 and `NNPC_Reaction_Cooking` Event6 set
  `cItemName` with `ItemTarget=16` (the villager's current craft). The existing
  script-item hook now interprets only this target as a recipe offer. Gifts,
  sales, other script targets and the player's own crafting keep catalog checks.
- Read the event-owned NPC's save view using the type check, save-index getter,
  read-lock contract and current-recipe field from native
  `EventFlowQueryIsNpcDIYRecipeCollected` at `00a53e40`. Query the actual recipe
  through the existing validated DIY/cooking knowledge functions; do not infer
  a recipe from its output item. Unknown data suppresses the marker.
- Added compiled ARM coverage for learned/unlearned recipes crossed with
  cataloged/uncataloged results, DIY/cooking/Spooky fence, all eight remapped
  players, three noun forms, missing NPC/save/type/history and invalid recipes,
  lock acquisition/release and ordinary dialogue scope at two ASLR bases.
- No new hooks or layout changes. Catalog remains an independent module;
  Crafting is not required. Live villager DIY/cooking dialogue needs Eden testing.

### HHP restaurant chef coverage (no runtime change)

`NNPC_RestaurantCook` Event180 runs `NpcSetRewardRecipe(DIYNpcItem=true)`
before Event179 runs `SetItemName(TagType=0, ItemTarget=1, ID=0)`. The native
reward action's restaurant-chef override at `00ac8e18..00ac8e94` reads the
restaurant cooking recipe and constructs a recipe card via `02512588`.
`00ac8ee4` publishes that card at `05305f5c`; target 1 copies it unchanged via
`00a47078` / `00a47b14`. It therefore reaches the existing recipe-card branch
in DialogueScriptItemStar, not the target-16 crafted-result exception.

The focused restaurant regression executes native cooking-card validity/ID
initialization and payload encoding, reward publication and target-1 copying,
then checks the compiled star callback and native learned-recipe getter. General
ItemAccessor setup and its runtime metadata caches are excluded. Three cooking
recipes, eight remapped players and both learned states run at two ASLR bases
with no event-owned NPC required. Existing dialogue/ground checks run as well.
No added hook, message whitelist, version bump or new binary is needed. This
establishes code-path coverage; a live chef conversation is still unverified.

## Unreleased configuration defaults and dialogue range (2026-10-10)

Removed the unreleased `faster_player` alias; it now follows unknown-key handling
and cannot change `faster_unnamed`. Automatic UI and unnamed dialogue boosting
now default to true in code and the shipped Faster Dialogue INI. Dialogue base
and boost defaults remain 2/4, with both accepting whole numbers 1..30. Values
outside that range inherit the prior setting; boost is still raised to base
when necessary. Crafting now defaults to a peak of 6, retaining its 60-update
ramp and existing 1..30 range. Personal custom files retain precedence.

Validation: pinned Docker build succeeded. All host regression checks passed,
including every dialogue speed pair and removed-alias rejection. Core startup,
module layering and preview ARM checks passed. Dialogue ARM checks passed all
900 speed pairs at each of two ASLR bases (166,472 extended-speed checks), plus
6,672 existing dialogue cases and 6,916 localized-name cases. Crafting passed
30,700 native cases and 84 quantity-shortcut cases. Name-classification tests
explicitly disable automatic categories where they need to isolate NPC matching.
The emulator needs execution outside the macOS sandbox for JIT access.

No release archive or version bump is included. The new default feel and higher
dialogue speeds still need live Eden testing.

## Unreleased Core logging controls (2026-10-10)

Core now accepts `log_versions=true`, `log_settings=true`, and `log_debug=false`
in both optional Core INIs. Preferences use the existing regular/custom parser
rules, including per-key inheritance and duplicate rollback. The runtime logger
uses explicit categories; warning/error output cannot be disabled. Default
startup emits only Core/module versions and effective settings. Module settings
are reported after both files, even when code is disabled, including the NPC
list and normalized dialogue speeds. Successful installation/resource messages,
framework startup chatter and gameplay/input/purchase traces are debug-only.
Caption fallback, compatibility/fingerprint errors and partial-startup reports
remain visible. Pre-mount debug chatter is suppressed because Core preferences
are not available yet. Special dialogue tracing still needs a diagnostic build
and now also requires `log_debug=true`; documentation/package instructions agree.

Validation:
- Pinned Docker build succeeds; source diff whitespace checks pass.
- Host tests cover all eight logging switch combinations, required warning/error
  categories, inheritance, invalid values, duplicate rollback and atomic file
  rejection alongside all 65,536 amiibo timeout values.
- New compiled ARM logging tests verify the exact quiet startup surface, all
  eight switch combinations, regular/custom precedence, mandatory errors and
  warnings with every switch off, disabled-module settings, actual NPC values,
  normalized speeds, unchanged installed hooks and once-only reads at two bases.
- Core, Shopping Card, Faster Dialogue, Crafting, Pockets, Fitting Room and Catalog
  regression groups all pass across focused invocations. Additional shop catalog
  and fitting runtime tests pass. Dialogue retains 166,472 extended-speed checks;
  crafting passes 30,700 cases; final fitting catalog coverage passes 26,077 cases.
- Standalone Pockets/Catalog fixtures now verify quiet default gameplay output
  while preserving caption-fallback warnings and their behavior assertions.
  Existing detailed Core-based fixtures explicitly opt into debug via the real
  compiled settings parser. Fitting input tests call the actual vtable adapter
  rather than a private helper that is now inlined. A stale Shopping Card test
  now counts only its installed module's hooks and shared resource allowance,
  rather than assuming Core always installs the optional DIY feature.

No version bump or release archive was created. Live Eden log appearance remains
unverified; the local checks execute compiled ARM with modeled engine services.


## 0.4.0 release test build (2026-10-10)

Bumped Core and all six feature markers to 0.4.0, updated current installation
examples and release notes, and made compiled Core/logging fixtures read the
release version from the source. Future-version fixtures now use 0.5.0.

Validation: the pinned Docker toolchain rebuilt the shipping configuration
with diagnostic dialogue tracing disabled. The full Core check group passed:
host regressions, compiled logging policy, all 64 module combinations at two
ASLR bases, version compatibility, regular/custom layering, and native recipe
preview/workbench/input regressions. Package-content checks passed for all
seven folders, feature versions, default Core settings, SHA-256 manifest,
reviewed resource hashes and all 16 fitting-room caption locales.

The binary suite, source and optional custom-dialogue example are packaged
separately as 0.4.0 archives; previous releases are preserved. This is a local
release test build for ACNH 3.0.3 on Eden. Live testing of the new defaults,
speed limits and logging is still required before public release.


## 0.4.1 DIY Recipes app A / Select hint (2026-10-11)

Added a fingerprinted hook to the native MenuRecipe footer builder
(`02E9C670`). When Crafting's `diy_app_preview` is enabled, a nonempty recipe
list receives the native A icon and existing localized KeyGuide `1201`
(Select). Category focus and open previews retain their native hints. No
layout replacements or new translation strings are required.

Preview opening, closing, and renderer refusal request a footer rebuild. The
rebuild waits until KeyGuide has consumed its pending records, preserving the
native Sort/Favorite/Close hints without duplicating or freeing engine records.
Descriptor capacity and available pool entries are checked before appending.
Disabling the DIY preview leaves this additional hook uninstalled.

The compiled ARM64 fixture executes the original phone footer builder,
KeyGuide descriptor copying and pending-to-displayed transfer at two ASLR
bases. It covers recipe/category/empty-list scope, preview A/B lifecycle,
renderer contention, deferred refresh with a pending batch and capacity limits.
The Core, Crafting and Pockets regression groups pass, including independent
settings, hook conflicts, existing preview rotation/navigation, workbench
quantity shortcuts and shared page input. Nine host-side test suites also pass.
The offline Docker build succeeds.

Core and all six module version markers are 0.4.1. Prior release archives are
preserved. The suite/source archives are verified after packaging, including
manifest hashes, installed binary/config parity and tracked-source parity.
Live Eden confirmation of hint positioning, localization and A/B behavior is
still required; these local checks do not execute GPU footer rendering.


## 0.4.14 Controls: Hold Y to hoover (2026-10-11)

Added the seventh optional feature module, Controls, with its own versioned
`controls.ini` marker. A successful first Y pickup reservation captures item,
crop or weed category. Held Y repeats native pickup only within that category;
release/new press clears it. All six crop species share one category. Empty,
unsupported and failed initial attempts do not arm it. Native targeting,
animations and capacity handling remain responsible for the actual action.

Five C++ hooks observe physical input, extend only the native field Y branch,
scope the native picker, exclude placed furniture during repeats and reject a
mismatched actual source before the native reservation service. Seven native
instruction fingerprints are checked before installing any Controls hook.
Latches are bounded to eight resident slots and tied to player/generation.
The source recheck protects against probe and commit selecting different items.
See `research/controls/NOTES.md` for bindings, assumptions and the live checklist.

Validation: the pinned offline Docker build passes. Core + Controls regressions
pass, including nine host suites, quiet default logging, all 128 optional-module
subsets at two ASLR bases, metadata/custom layering and native recipe-preview
regressions. Controls adds 149 compiled ARM cases at two bases: all category
transitions and crop species, unsupported/empty sources, initial/repeat refusal,
release/repress during blocked actions, player/generation isolation, changed
probe/commit source, unscoped service passthrough and atomic fingerprint failure.
Native physical-Y mapping, packed item decoding and runtime field-kind decoding
execute; loaded parameter tables, picker scheduling and reservation are modeled.

Core and every feature marker advance to 0.4.14. Packaging now contains Core
plus seven optional folders. Live Eden targeting, animation timing, inventory
capacity behavior, multiplayer and save/reload confirmation remain pending.

Package structure/resource/hash checks pass for eight mod folders, seven
versioned feature configs, the unchanged Core NPDM and inherited layouts.
The binary, matching source and optional personal-dialogue example archives
are produced separately; older releases remain intact.

## Pockets hold-A selection development test

- Pinned Docker build passes for ACNH 3.0.3.
- 48 new compiled/native ARM scenarios pass at two ASLR bases, plus 842 inherited
  Pockets cases: five-item sweep, stationary hold, backtracking, independent A
  taps, slot 39, active controller row, blocked/consumed input, native pending
  clicks, screen/mode changes, suppressed dispatch and unrelated page dispatch.
- Original native selection code rejects additions beyond the selection limit
  and mode 9 storage capacity, preserving the prior mask and queuing native
  error 0x69/state 12 for storage-full rejection.
- Feature registration validates native producer/consumer fingerprints. The
  Exlaunch installer, actual navigation, rendering and live frame timing remain
  outside the emulator fixture and require an Eden playtest.

Test 1 failed in the user's shed put-away playtest: only the initial A-selected
item was selected. Eden loaded the replacement Core and enabled Pockets without
an installation error. Its log_debug=false setting hid the feature's ready/debug
messages. Test 2 adds bounded, automatic HOLD_SELECT_TRACE records for live
picker mode, gates, held input and focus before/after native dispatch. It is a
diagnostic build; no behavioral fix is claimed from the modeled navigation test.

Test 2's synced Eden trace identifies the failure: shed mode 9, limit -1,
browsing state 3, and focus changes all match the implementation, but held A
arrives as 0x10010 (A+right as 0x10018). The incorrect conflict mask 0x303e0
includes bit 16 and cancels the gesture on every A press. Adding this observed
bit to the ARM fixture reproduces test 1/2's failure. Test 3 corrects the mask
to UI action IDs 5..11 (B/X/Y/L/R/ZL/ZR), retaining navigation bits 0..3 and
ignoring the extra bit 16. The diagnostic trace remains bounded for live
confirmation; it is not yet a user-confirmed in-game fix.

Test 3's pinned build passes. 72 compiled/native ARM cases pass at two bases,
including the observed slot-10-through-14 input sequence, all four directions,
all seven conflicting action buttons, release/re-arm behavior, and the prior
selection/limit/capacity cases. The imported 842 Pockets cases also pass.

The user confirms test 3 works for held-A selection in shed put-away.
Test 4 adds deselection: each hold captures the opposite of its first clicked
item's selection state, retaining that target until release/cancellation.
Native first-click destination takes precedence when A and navigation arrive
together. Temporary always-on diagnostics are removed after the live fix.
The pinned build and 88 compiled/native ARM cases at two bases pass, including
five-item deselection, mixed selected/unselected gaps, backtracking, switching
direction after release, blocked deselection, and simultaneous A/navigation in
both picker modes. All 842 imported Pockets cases pass. Live inverse behavior
and selling remain unconfirmed.


## 0.4.16 Controls hoover setting (2026-10-11)

The user confirms hold-Y hoovering works in-game. Added `hold_to_hoover = true`
to Controls, independently of `hold_to_advance_dialogue`. Both regular and
custom INIs support it; missing values default to true, omitted custom values
inherit, invalid values retain the prior value, and duplicate keys roll back
only that setting. When false, all five pickup hooks and their fingerprint
checks are skipped, leaving native field Y behavior intact. Dialogue remains
independently configurable. Settings are logged after both config layers.

The pinned offline build passes. Ten host suites and the Controls regression
group pass: 181 hoover cases, 1,024 independent Controls dialogue cases, 712
Hurry scenarios and 772 catch scenarios. The new cases exercise all four
feature combinations, regular/custom overrides, invalid overlays and disabled
pickup with conflicting native instructions at two ASLR bases. All 128 module
subsets, shared configuration layering and default logging checks also pass.
Package content/resource/hash checks pass. Release 0.4.16 includes matching
suite and source archives; older archives remain intact.


## 0.4.18 Catalog museum creature stars (2026-10-11)

Catalog now prefixes undonated fish, bugs and sea creatures with the existing
filled star. The three native catch-tag branches are hooked before their owned
name copies. Shared item dialogue, pocket/home-storage balloons and ground
balloons use the current island museum rather than personal caught/catalog
history. Any resident's donation counts; every display reads current state.
The feature retains Catalog module gating, controller/player/mode guards,
bounded UTF-16 handling and exact 3.0.3 instruction fingerprints. Missing
runtime tables/providers/views or failed lock acquisition leave names unchanged.

The pinned offline devkitA64 build passes. The new ARM suite passes 2,476 cases
at two ASLR bases, including all 80 fish, 80 bugs and 40 sea creatures, donor
ff/0/7, all eight active residents, catch and generic dialogue, pocket/storage
scope, native donation lookup/comparison, lock pairing, unavailable resources,
special flags, alternate island resolution, last record and missing species.
Synthetic museum data is read-only to ARM and compared before/after; no live
save is used. Research/ABI evidence and fixture service boundaries are recorded
in research/museum-stars/NOTES.md.

All host checks and the Catalog check group pass, including restaurant/villager
recipes, UI-result sources, variation labels, Miles binding/counting, native
counted names and 26,077 fitting-room cases. Additional regressions pass:
2,248 ordinary catalog-star cases, 14,216 furniture variant cases, 3,882 pocket
recipe cases, 14,538 storage/ground cases and 1,926 generic dialogue/ground cases.
Core activation passes all 128 module subsets at two ASLR bases, configuration
layering and isolated hook conflicts. Catalog's expected installed-hook count
increases from 16 to 19.

Live Eden verification remains pending: undonated and donated catches in each
family, first and repeat catches, pocket refresh after donation (including by
another resident), localization, storage/ground display and Catalog disabled.
The 0.4.18 suite is an integration test build; no emulator deployment or
in-game success is claimed. Older release archives are preserved.
