# Crafting module (ACNH 3.0.3)

As of 0.3.46, Crafting replaces Faster Crafting and owns the existing DIY app
preview code formerly installed by Core. Its minimum package version is 0.3.46.
The marker is `crafting.ini`, followed by optional `crafting.custom.ini`; the
retired `faster-crafting` paths are ignored. Core alone installs no DIY hooks.

Defaults: `diy_app_preview=true`, `workbench_preview_tilt=true`,
`crafting_quantity_wrap=true`, `crafting_speed=3`. Preview and speed are active;
workbench tilt and quantity wrap are parsed/reserved only and still deferred.
Hotel banner layering/placement is implemented in 0.3.48. Initial orientation
and recipe navigation follow-ups remain open.

Speed is the peak of the existing 60-update logarithmic ramp, accepting integer
1..30 as of 0.3.49. Peak 3 retains the original table exactly; other peaks scale
its gain by `(peak-1)/2`, preserving timing. Above-maximum integers clamp to 30
without overflow, including decimal values too large for an unsigned integer.
Peak 1 holds at 1x. Native actor/readiness/input/exit gates are unchanged. Both action
hooks and all preview hooks live in the same Crafting marker, with independent
installation attempts so one feature's incompatibility does not suppress the
other. Disabling `diy_app_preview` bypasses preview fingerprints and hooks.

Settings use the shared regular/custom layering policy: omitted/invalid keys
inherit, duplicates restore the earlier layer, and malformed metadata or
encoding/size failures reject that file atomically. The reserved settings are
reported as deferred in startup logs. Module activation and settings loading
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
