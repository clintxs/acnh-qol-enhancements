# Crafting module (ACNH 3.0.3)

As of 0.3.46, Crafting replaces Faster Crafting and owns the existing DIY app
preview code formerly installed by Core. Its minimum package version is 0.3.46.
The marker is `crafting.ini`, followed by optional `crafting.custom.ini`; the
retired `faster-crafting` paths are ignored. Core alone installs no DIY hooks.

Defaults: `diy_app_preview=true`, `workbench_preview_tilt=true`,
`crafting_quantity_wrap=true`, `crafting_speed=3`. All four settings are active as of 0.3.57.
Hotel banner layering/placement is implemented in 0.3.48. Recipe navigation is
implemented in 0.3.50; native starting orientations are restored in 0.3.53.

Speed is the peak of the existing 60-update logarithmic ramp, accepting integer
1..30 as of 0.3.49. Peak 3 retains the original table exactly; other peaks scale
its gain by `(peak-1)/2`, preserving timing. Above-maximum integers clamp to 30
without overflow, including decimal values too large for an unsigned integer.
Peak 1 holds at 1x. Native actor/readiness/input/exit gates are unchanged. Both action
hooks and all preview hooks live in the same Crafting marker, with independent
installation attempts so one feature's incompatibility does not suppress the
other. Disabling `diy_app_preview` bypasses DIY-specific fingerprints and hooks.
Workbench tilt shares the item actor trampoline and Core input dispatcher, and
can install independently with DIY disabled. Both preview settings false leave
the actor hook uninstalled; quantity wrap can independently use shared input.

Settings use the shared regular/custom layering policy: omitted/invalid keys
inherit, duplicates restore the earlier layer, and malformed metadata or
encoding/size failures reject that file atomically. All four active settings are reported in startup logs. Module activation and settings loading
still happen once after content mount. Core and Pockets/DIY share input only
when at least one consumer is active.

All addresses below are offsets in the original `main.raw`. Behavior is readable
C++ Exlaunch hooks. The native speed-path history below describes the original
Faster Crafting implementation retained by Crafting.

## Native paths

`Ftr_Working` event flow's `Workbench_Cmn_DIYdemo` calls
`PlayerWorkbenchMaking(false)` for crafting and `(true)` for customization.
Its native enter routine 022fc0a4 chooses those two event names. Player state
0xd1 (`State::cFtrWorkbenchMaking`) is registered at 0228c1b4..0228c20c; its
update wrapper 022a6b68 calls 022fc624 at 022a6bc0. Cooking uses state 0x12f
(`State::cCooking`), registered at 0228e66c..0228e6c0. Its update wrapper
022b17ac calls 02279980 at 022b1804; enter 022794f8 emits `PlayerCooking`.

Both wrappers return early when actor+17cf bit 0 is set or the animation
component has +ae clear or +b4 set (CMP/CCMP with fallback NZCV=0). Both inner updates gate automatic
input on actor+178 == 0 (local actor). Non-local actors retain their supplied
speed and bypass both hook sites. Player input bit 0 at actor+ed0 increments
the state's mash counter when actor+17ac is clear:

| State | Counter | Load | Hook before rate store | Rate storage |
| --- | --- | --- | --- | --- |
| Crafting/customization | +c08 | 022fc690 | 022fc6bc | +1928 |
| Cooking | +db8 | 022799f8 | 02279a14 | +1920 |

The original calculation clamps this counter to a 1..3 floating-point rate.
In 0.3.20 the callback replaces S0 immediately before its native store with a
logarithmic 1..3 ramp: `1 + 2*log(1 + 9*t)/log(10)`, where
`t = min(actor.stateFrames, 60)/60`. Version 0.3.21 stretches the ramp from
30 to 60 updates. A 61-entry float table supplies the curve
without a runtime math library. Native instructions store the rate and pass it
to animation setter 03750760. No counter, input bits or unrelated animation
rates are overwritten. While enabled the automatic curve owns the rate,
regardless of manual A presses. Disabled retains the native mash calculation.

The elapsed-state counter is actor+1c04. Both state entry wrappers (022a6b28
and 022b176c) call common state initialization 02275730, whose 022758a8 resets
that counter to zero. Both registered update wrappers increment it after the
inner update if the state has not changed: 022a6bec..022a6bf4 and
022b1830..022b1838. Their suspension/animation-busy returns precede these
increments. Thus each action restarts at 1x, reaches 3x after 60 updates,
and uses its own actor's counter without global ramp state or extra hooks.
Recipe readiness can still force 1x even while the native counter advances.
This is game-update timing, not wall-clock timing; FPS mods may change its
duration. These new counter instructions are fingerprinted before installing.

Crafting additionally queries recipe/animation readiness at 022fc6e8
(0264bce0). A false result takes 022fc980 and explicitly resets +1928 to 1.
This native gate remains intact. State exits 022fcde0 (crafting) and 0227a0e0
(cooking) call 03750760 with 1, restoring ordinary animation playback.

## Validation

`tests/crafting_arm_test.py` executes the original registered update wrappers
and native rate calculations with the compiled C++ callback through the real
SIMD-preserving adapter at two ASLR bases. Only the recipe readiness query and
the final animation setter boundary are modeled. Cases cover default enabled,
regular/custom disable, custom re-enable, missing marker, native manual A
boosting when disabled, several counters, input suppression, non-local actors,
both wrapper early exits, readiness failure, unchanged input/counters and all
GP/SIMD registers except the intended S0. Every curve sample is compared with
an independently calculated logarithm; saturation, diminishing gains, repeated
actions, native counter-reset/increment instructions and large counter values
are covered. Native exit instructions execute to their rate-reset call.
Corrupt each fingerprint independently to check that
neither crafting hook installs and other modules still activate.

Core regression exercises all 128 feature subsets and shared INI layering.
These checks are local ARM execution, not an Eden play test. Crafting,
customization and cooking need live confirmation.


## Deferred UI follow-up — crafting quantity wrap (October 9, 2026)

The user requests documenting this for later, without starting implementation.
This concerns the crafting quantity selector, a separate interface from the
DIY Recipes app previews. As reported, the selector starts at 1 and supports
quantities from 1 through 10.

Desired behavior:

- A fresh downward input at 1 wraps to 10.
- A fresh upward input at 10 wraps to 1.
- Holding the joystick may scroll through the range, but must stop at the
  endpoint instead of wrapping through it on input repeat. After reaching 10,
  release/return the stick to neutral, then press up again to wrap to 1.
  After reaching 1, release/return to neutral, then press down again to wrap
  to 10. Continuing the same held gesture must never trigger another wrap.
- Opening at the initial quantity of 1 permits a fresh downward input to
  select 10 directly.

Future acceptance examples: holding up from 1 reaches 10 and stays there;
release then up selects 1. Holding down from 10 reaches 1 and stays there;
release then down selects 10. Repeated held-input events at either boundary
must not be mistaken for a fresh input.

The native selector/input-repeat path, any recipe-dependent quantity limits,
module placement and settings have not been investigated or selected. This
entry records the intended interaction only; no runtime or release changes.


## Possible Crafting submodule grouping — October 9, 2026

Originally proposed, then approved for the implemented features in 0.3.46:
one **Crafting** submodule containing:

- The crafting quantity selector wrap tweak documented above.
- DIY Recipes app 3D previews and their follow-ups.
- Vertical up/down tilt for workbench previews.
- The existing Faster Crafting feature, with configurable speed settings.

The grouping is now implemented for DIY previews and faster crafting. Workbench
tilt and quantity wrapping remain deferred behind accepted/reserved preferences.
The workbench tilt default remains on, as recorded in
[the preview follow-ups](../recipe-preview/NOTES.md#deferred-follow-ups--october-9-2026).

## Speed settings follow-up — implemented in 0.3.46

The request for configurable speeds is implemented as `crafting_speed`, default 3,
controlling the existing ramp's maximum at 1x, 2x or 3x. Ramp timing remains 60
updates and crafting/customization/cooking share the preference. Separate speeds,
other peaks or ramp timing settings are not included in this change.

## Expanded speed range — 0.3.47

The maximum supported `crafting_speed` is now 6, twice vanilla’s 3x mash peak.
The default remains 3. Parsing saturates each decimal step at the named maximum
and validates the entire value before assigning it, so large positive integers
clamp safely while trailing garbage, zero and other malformed values inherit.
Duplicate keys still restore the earlier layer. The installer shares the same
maximum constant. Ramp timing and native gates are unchanged.

Host parser tests cover all six peaks, huge decimals, invalid suffixes and
layered duplicate rollback. ARM tests execute compiled crafting/cooking hooks
at both bases for every ramp sample at peaks 1..6, including regular/custom
clamping. The 6x peak still needs a live Steam Deck check.


## Expanded speed range and setter validation — 0.3.49

Maximum `crafting_speed` is now 30; default 3 and the 60-update curve are
unchanged. Above-maximum positive integers clamp to 30. This is an animation
rate peak, not a multiplier for total crafting time. Short actions may finish
before reaching the peak, and recipe readiness may still reset the rate to 1.

Native animation-rate setter 03750760 copies S0 to S8, then writes the supplied
rate directly to active animation tracks +A0 and the animation component +A8
(037507B4..037507C4). It has no 3x/6x rate clamp. The ARM fixture now executes
this full setter against two real-shaped track objects, stopping on return to
the state update. Every curve sample at all 30 supported integer peaks is
checked at both ASLR bases, including custom overrides and overflow-safe
clamping. Readiness, local-player and exit/reset behavior remain unchanged.
This establishes track-rate storage, not live animation completion timing.

Synced logs inspected for this request contained one 0.3.47 run with speed 6
and a newer 0.3.47 run with speed 3. They do not establish which run the user's
perceived speed comparison came from. Live 30x behavior needs a Steam Deck test.


## DIY preview navigation — 0.3.50

D-pad/left-stick previous/next now uses the app's live filtered/sorted recipe
array. The native grid restoration/focus routines maintain selection and scroll
position; the native workbench item-change routine updates the existing actor.
See [navigation implementation](../recipe-preview/NOTES.md#recipe-navigation--0350).
The user also confirms a noticeable speed difference with the 0.3.49 expansion.


## DIY preview input and attached effects — 0.3.51

Correct the horizontal logical button masks and ignore vertical navigation.
Publish the rotating pose into native scene carriers before drawable/bone
transforms so effect attachments share the displayed model's pose. See
[implementation evidence](../recipe-preview/NOTES.md#horizontal-input-and-scene-pose--0351).
Live particle behavior remains to be confirmed.

## DIY preview primary particle rotation — 0.3.52

The user confirms navigation is fixed, but particles still separate in 0.3.51.
The native item-effect update reads primary Euler angles directly; secondary
viewing angles used by the earlier patch affect drawing only. Write the owned
carrier's primary XYZ through its native setter and clear secondary XYZ. Native
drawable propagation supplies the same pose to drawable-owned effects. Fixed
types retain native framing. See [native evidence and regression coverage](../recipe-preview/NOTES.md#primary-euler-rotation-for-item-particles--0352).
Local checks now execute the actual particle orientation publication path;
live Steam Deck appearance remains to be confirmed.

## Native DIY starting poses — 0.3.53

The user confirms 0.3.52 fixes particles. Restore each carrier's item-specific
workbench orientation instead of using the controller's shared initial angles.
Convert the native matrix into primary Euler angles so effects retain the fix;
right-stick input adds movement relative to this cached origin. Cache/reset
follows recipe changes, native framing and full carrier ownership. Fixed types
and workbench behavior remain unchanged. See [pose conversion and coverage](../recipe-preview/NOTES.md#native-starting-orientations--0353).
Live Log pack appearance remains to be confirmed.

## Workbench tilt and preview handoff — 0.3.54

The user confirmed native starting poses in 0.3.53, then reported a brief reset
of the outgoing item during navigation. Keep scene ownership/carrier identities
while the next item loads, defer fixed-pose policy until it is framed, and retain
the last displayed primary angles if native replacement resets a surviving old
carrier. New full-generation handles/components cannot inherit that pose.

`workbench_preview_tilt` now installs behavior, default true. See the detailed
[preview notes](../recipe-preview/NOTES.md#workbench-tilt-and-transition-retention--0354).
Quantity wrapping remains deferred. No speed behavior changes.

## Quantity endpoint wrapping — 0.3.57

WorkBenchNumSelectPage is vtable 04A3A7D8 (constructor 0346A034);
KitchenNumSelectPage is 049DE8F0 (02DE7C14). Both use shared input 03C58D24,
page +578's native grid and grid +2E0's resident row. Grid rows are reversed:
quantity = count - row. Count is the native material-limited range (2..10);
never extend it or enable the generic grid's continuous-loop bit.

A neutral-observed latch admits one fresh vertical endpoint press. Blocked,
consumed, opposite, nonfinite or absent input does not queue a wrap. Direction
reversal without neutral cannot rearm. ResetQuantityGrid wraps 03764290 to
disarm cached pages on native cAppear/reopen, including pointer reuse. It does
not change other lists. The fourth shared-input subscriber is scoped to the
two exact quantity page classes; non-wrap input remains entirely native.

Wrap uses a native 0x30-byte GridRequest, 03764290 reset, 0335B4A0 activation,
03765674 selection (fires grid +370's quantity callback), and 03763E14 focus.
03469E1C / 02DE79FC store quantity-1 at sequence +1C2D8, refresh make-button
status/limit indicators, and call 03465370 for materials/count text. The ARM
fixture runs the crafting callback and native movement/clamping, reset,
activation and focus with graphical/material formatting boundaries modeled.
Both page identities, nonzero resident, endpoint wraps, long held repeats,
release/reversal, consumed/blocked input, native lower limits and reopening
are covered at two ASLR bases. Live controller feel still needs confirmation.


## Joystick release/repeat correction — 0.3.58

The 0.3.57 live report confirms d-pad behavior but shows stick overscroll after
wrap and occasional continued scrolling. Axis consumption alone is insufficient:
native 03C4B20C / 03C48C70 also read logical direction bits and their own repeat
state. Consume vertical bits 0/1 unconditionally plus left-axis bit 0 after a
wrap, continuing until both logical directions are released and the raw stick
returns within radius 0.15. This also covers a delayed synthesized direction.
Press remains at 0.5; crossing back below 0.5 must not rearm an already-used
latch. Conversely, preserve an armed latch on the way out of center so a slow
press still wraps. ResetQuantityGrid clears both latches on reopening.

Quantity regression now executes 03C4B20C and 03762978, with native repeat
intervals 12/4, before the existing real native list movement and callback.
Tests include holds after wrap (remain at the destination), interior-to-boundary
holds, sub-threshold pressure jitter, delayed logical bits and slow presses.
Controller-to-logical translation and rendering/text boundaries remain modeled.


## Fresh-wrap hold pause — 0.3.59

The user confirms 0.3.58, then requests immediate endpoint wrap followed by a
500 ms pause and resumed scrolling for a held direction. A hold that reaches
the boundary from the interior still cannot wrap without release. Preserve the
0.15 release / 0.5 press hysteresis and one-wrap-per-gesture latch.

Track pauseStarted with svcGetSystemTick; Horizon ticks are 19.2 MHz, as defined
in nn/os/impl/os_tick_manager_impl.os.horizon.hpp, so 500 ms = 9,600,000 ticks.
During the pause, mask logical vertical bits from the snapshot's held buttons
and neutralize the left-stick pair. This lets native dispatch observe release;
consumed masks alone strand native repeat in its release-required state. Fresh
snapshots restore physical input each update. At expiry, native dispatch resumes
its own repeat immediately; our wrap latch remains disarmed. Early neutral
cancels the pause. A fresh wrap, reopen/reset or invalid list cannot inherit an
old timer. A/B stay native and are never delayed by this pause.

Tests run real 03C4B20C timers and 03762978 / 0375FD8C movement at modeled
30/60/120 Hz with exact 500 ms boundary assertions, both directions and controls,
both exact crafting/cooking pages, hold-to-next-boundary stopping, early release
and timer restart, unchanged in-range input and consumed input at expiry.


## Fresh-wrap timing adjustment — 0.3.61

User confirmed the 0.3.59 behavior and requested approximately 200 ms instead
of 500 ms. WrapPauseTicks is now 19,200,000 / 5 = 3,840,000. The neutral-input
pause, release cancellation, one-wrap-per-gesture latch and native repeat
resumption are unchanged. Tests cover the exact deadline at 30/60/120 Hz and
restart after early release, with the same native repeat/movement fixture.

Native 03C4B20C uses update-count initial/repeat parameters from snapshot+0x18
and +0x1c, unless its input context overrides them. 200 ms is requested tuning;
we have not measured the live quantity page's native delay. The fixture's 12/4
repeat parameters are modeled inputs, not evidence of exact native defaults.


## Fresh-wrap pause adjustment — 0.3.63

User requests 300 ms after testing 200 ms. The deadline is now 5,760,000 ticks
(19.2 MHz * 3 / 10); no latch/repeat behavior changes. Quantity tests derive
frame counts from pause_ticks so the exact deadline remains covered at modeled
30/60/120 Hz, including early release and restart of the pause.


## Quantity wrap animation and directional feedback — 0.3.65

Replace the reset/activate/select/focus jump at each wrap with the ordinary
03762978 queued vertical-step fields (+3EC=2, +3F0=0 up/1 down, +3F4=-1/+1,
+3F8=0) consumed by 0375FD8C. Clear the queued kind afterwards because shared
page input runs before the native grid update. Native movement selects the
quantity, interpolates one row and calls 03765950's cScroll feedback. It sets
+468's selector +40 from direction, inverted by the quantity grid's reverse
flag. The playback boundary receives "Scroll" (the native cScroll name).

Prepare native GridRequest.loop (+17) at the normal quantity cAppear reset
calls, return addresses 0346B028 / 02DE8BF8. Match these callers and the
one-column/one-visible-row layout, copy the request and leave unrelated resets
unchanged. Keep cyclic neighbor binding throughout the selector lifetime;
toggling it at wrap time changes viewport padding and pops the number labels.
Existing neutral/pause and fresh-gesture gates prevent native cyclic movement
from wrapping again when a held control reaches an endpoint. The 300 ms timer
and native material limit (2..10) are retained.

LBtnListNumInput's two L_Btn panes are templates. 037611xx allocates a runtime
pool including four padding rows; +2F8 is active pool size, not two rows. The
native binders 03469B50 / 02DE7730 correct just one lap. Small material limits
can be shorter than the padded pool, leaving a visible neighbor unbound.
The scoped quantity row delegate normalizes each source index when one-lap
binding would be insufficient. It calls the original native binder on each
row, temporarily narrowing +2F8/+210, then restores both fields. Native
formatting, material styling and callbacks are retained; no text is recreated.
Binder bodies and both caller instructions are fingerprinted before install.

ARM checks execute native reset, activation, selection, button repeat, move,
interpolation, label binders and scroll feedback at two ASLR bases. Pool sizes
5/15 and every native maximum 2..10 cover both visible neighbors and settle.
Both native page identities, directional sound selectors, one-row distance,
300 ms boundaries at 30/60/120 Hz, early release/restart, held stopping, input
ownership and reopen guards are covered. Clock/controller samples, layout
writes, text formatting and final timeline/audio playback are modeled. Actual
animation appearance and audible sounds still need the Steam Deck test.

## Direct Set quantity from the workbench preview — 0.3.66

The user chose the large WorkCraftInfo 3D preview, rather than the recipe grid.
WorkCraftInfoSeq's common base registers Play state 3 at 03494B2C/03494D00/
03494E7C and MultiCreation state 9 at 03496160/0349634C (034942F0).
03493C20 preallocates its native quantity child at sequence +1C1E8.
The new input subscriber binds only during Play, clears before native Play
exit, requires a released/fresh X press and mirrors page/global input gates.
It queues state 9 through the native pending slot (+1F0, slot byte +200),
hides the preview footer through +239 bit 4, clears the pending input action
and sets material status to ready, matching the ordinary ready dialog path.
Native Play exit disables preview input; MultiCreation supplies recipe data
through virtual +2A0, configures/attaches the quantity child and handles both
crafting and B back to Play. Transit state 8 only waits for confirmation
dialogs to close; direct entry has no confirmation dialog to close.

The same virtual +288/+290/+2A8 checks as the native ready/multi dialog path
and +1C1F0 capability gate control input and the hint. Opening also requires
the existing child's virtual +F0 readiness. No material limits are synthesized.
The shortcut is part of Crafting independently of the optional wrap setting.

0346EC1C builds the workbench footer. AppendHint 02DE2B00 copies a bounded
X descriptor into the existing five-pane pool. Its private caption token
resolves to LayoutMsg DIYDialogConfirmNum/0001 (existing Set quantity in all
languages). The caption hook uses 02DE4940, where x2/x3 already hold bank/key,
and ends its 20-byte window before Pockets' hook at 02DE4954. Both remain
independent; no strings/layout replacements are shipped. Core's subscriber
capacity is five to accommodate Pockets, DIY previews, tilt, wrap and shortcut.

ARM checks execute native Play entry/exit, quantity open/cancel, footer builder
and descriptor copying. Engine readiness/type/recipe boundaries are modeled.
The fixture covers both bases, eligibility, release/held/blocked/consumed X,
A/B priority, footer capacity, localization routing and fingerprint conflicts.
Live visual/input behavior remains to be checked in Eden.

## Corrected quantity button and Favorite remapping — 0.3.67

User confirmed 0.3.66 works and corrected the input to Switch Y (Deck X).
Y was native Favorite in WorkCraftInfo. The preview subscriber now consumes
Y (snapshot mask 80) and uses it for quantity; X (40) queues native Play
action 3, matching the 03493018 Favorite delegate. Independent fresh-press
latches prevent either held input retriggering. Recipe-grid input is untouched.
The footer temporarily exposes button ID 6 for Favorite's synchronous native
descriptor copy and restores the key ID afterwards. Quantity uses ID 7 (Y).
Native Play update/virtual +298 still owns the actual Favorite toggle. The
ARM fixture additionally executes that dispatch and checks held-X suppression,
ineligible-Y consumption, both hint icons and native binding restoration.
