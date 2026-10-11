# Controls: Hold Y to hoover — ACNH 3.0.3

## Behavior

An ordinary Y press executes the native picker unchanged. A successful ground
reservation during that first attempt captures Item, Crop or Weed. Subsequent
held frames can enter the native pickup branch, but only commit the same kind.
All six crop species share Crop. Release clears the latch. Empty, failed and
unsupported initial attempts leave it unarmed. No inventory or save writes are
implemented by Controls; native pickup handles them.

## Verified native bindings

Addresses are main-relative in the local 3.0.3 executable. Reverse engineering
used disassembly and decompilation of the local executable, without distributing
its code or resources. Instructions are fingerprinted before installing hooks.

| Address | Role |
|---|---|
| `02461F30` | Input snapshot update; observe physical Y after native update |
| `02222A34` | Y action branch in dispatcher `02222950`; extend W8 bit 0 only |
| `02326E2C` | Native pickup selection/probe/commit |
| `023282D0` | Placed-furniture candidate handling; excluded during repeat |
| `021BAF28` | Ground reservation, before request/inventory work |
| `0267D6C4` | Decode packed source item ID |
| `0113DB50` | Resolve FgMainParam row's runtime field-kind enum |
| `04ABBC68` | GOT to loaded FgMainParam singleton slot |
| `04AC0428` | GOT to loaded ItemParam singleton slot |

Input is embedded at player+`EC0`; input+`48` identifies its owning player.
The first input pass maps controller Y (bit 4) to **physical action 38** in
64-bit held/pressed masks at input+`8`/`10`. The original mapping fragment at
`02462860..024629D0` executes in the regression fixture. Later context mapping
produces logical pickup action 3. Observing physical Y during snapshot update
avoids relying on that context action while an animation blocks pickup.

Latches are bounded to eight resident slots (player+`1EE8`), and tied to player
address and generation handle (player+`230`). A new physical press resets the
latch even if an ongoing animation prevents the first attempt. A held input
without a fresh press cannot arm it. No controller snapshots are modified.

The native picker supports probe argument 1 and writes an eight-byte source
item plus a three-float position. Repeats use its final argument 1 to exclude
late furniture/fence fallback paths, and suppress the earlier placed-furniture
candidate branch. A matching probe is followed by an ordinary native commit.
The final reservation hook checks the actual source again: probe and commit
can select different targets. A mismatched reservation returns before calling
the native service. The synthetic Y attempt is consumed even when no matching
source exists, preventing Y fallback actions such as ground-design removal.

Reservation classification/filtering is scoped to synchronous calls from the
picker hook. Other callers retain native behavior. The hooks assume the native
player action dispatcher executes on the game thread; nested picker scopes
restore their previous scope.

Parameter tables expose count at `1D8`, row pointer array at `1E0` and first ID
at `1F8`. Lookup checks loaded resources and ID/count bounds. FgMainParam's
runtime kind enum (`0113DB50`, cache at `050503B8`/`050503C0`) identifies crop
species as `12..17` and weeds as `1F`. Other field objects are unsupported.
An ItemParam row without a field row is an Item; dropped produce and weeds
therefore use the item category. Missing rows never receive a guessed category.

## Local coverage and limits

`tests/hold_y_arm_test.py` executes compiled Controls callbacks, native packed
item-ID decoding, native field-kind decoding and the physical Y mapping at two
ASLR bases. It covers category transitions, crop species, unsupported/empty
sources, failed initial reservations, release/repress during blocked actions,
resident/generation separation, a changed target between probe and commit,
unscoped service passthrough and atomic fingerprint rejection.

Picker scheduling, resource tables and reservations are modeled. These checks
do not prove live animation timing, target geometry, inventory handling,
multiplayer scheduling or persistence in Eden.

## Live Eden checklist

- Core + Controls only: first pick a dropped item, cross ripe crops and weeds,
  then reach another dropped item. Only the dropped items should be picked up.
- Start on each crop species; traverse a mixed field. Start on weeds; cross
  items/crops. Release and change categories, including during an animation.
- Empty initial press, unripe crop, blocked pickup and unsupported field objects
  must not arm the hold. Check flowers, trees, vines, ladders, fences, placed
  furniture and ground designs; ordinary single presses retain native behavior.
- Check dropped produce/weeds, mixed stacks, full pockets and nearly full
  stacks. Confirm native capacity refusal and no lost or duplicated objects.
- Walk/run, move between close targets, use tools, enter buildings, open/close
  menus, disconnect/reconnect a controller, change residents and reload.
- Combine with Pockets Y shortcuts, Controls dialogue hurry and Crafting quantity
  Y. Check local co-op and online play separately; neither is established here.
- Save/reload after successful actions and verify inventory/field state.

## 0.4.15 hold-to-advance dialogue

Controls now owns the fixed 30x Y text tier and native page-advance policy,
independently of Faster Dialogue. `hold_to_advance_dialogue` defaults true and
uses the same regular-then-custom overlay rules as the other configurable
modules. False skips all dialogue installation but retains hold-Y pickup.
Missing/disabled Controls cannot activate hurry; missing Faster Dialogue
preserves ordinary native reveal rates and B/L/R input.

`dialogue_hurry.cpp` retains the verified controller getter/fallback, ownership,
context, choice and readiness guards. The advance hook at 01AFA530 changes only
W22, preserving manual input, explicit input-pause resume and the exact three
locale-independent first-catch stops. No animation or save-state logic changes.
Five reveal hooks install once through dialogue::InstallRevealHooks, whichever
module requests them first. Without Faster Dialogue, callbacks preserve native
speed selection, rates and held-input latch unless eligible Y selects flag 5.
Faster Dialogue alone installs its seven hooks without an advance hook. With
both installed, shared sites are never patched twice. Each feature validates
its own dependencies before installation; pickup and dialogue can independently
fail fingerprint checks without suppressing the other Controls feature.

`controls_settings_test.cpp` covers layered defaults, strict booleans, duplicate
rollback and whole-file UTF-8/size rejection. `controls_dialogue_arm_test.py`
executes native reveal parity, Y/release, page/pause/choice/new-catch guards,
regular/custom activation and independent fingerprint rejection at two ASLR
bases (1,024 scenarios). Existing hurry, localized catch and pickup regressions
remain in the Controls check group. Live follow-up: install Core + Controls
without Faster Dialogue, verify vanilla text on release and 30x while held,
then disable the key and confirm pickup repeats still work. Repeat with Faster
Dialogue installed and customized speeds.
