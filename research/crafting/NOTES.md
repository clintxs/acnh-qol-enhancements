# Automatic crafting/cooking boost (ACNH 3.0.3)

All addresses below are offsets in the original `main.raw`. Behavior is C++
Exlaunch inline hooks, independent of Faster Dialogue. The new Faster Crafting
module's ordinary `enabled` setting defaults to true; Core provides the same
regular/custom INI loading as every other module. No additional settings parser.

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

| State | Counter | Load | Hook after load | Rate storage |
| --- | --- | --- | --- | --- |
| Crafting/customization | +c08 | 022fc690 | 022fc694 | +1928 |
| Cooking | +db8 | 022799f8 | 022799fc | +1920 |

The original calculation clamps this counter to a 1..3 floating-point rate.
The callback supplies W8 = 3 immediately after the load. Original instructions
still calculate and store the rate, and pass it to native animation setter
03750760. No counter, input bits or unrelated animation rates are overwritten.
No global input hook or hard-coded frame duration is involved.

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
GP/SIMD registers except the intended W8. Native exit instructions execute to
their rate-reset call. Corrupt each fingerprint independently to check that
neither crafting hook installs and other modules still activate.

Core regression exercises all 128 feature subsets and shared INI layering.
These checks are local ARM execution, not an Eden play test. Crafting,
customization and cooking need live confirmation.
