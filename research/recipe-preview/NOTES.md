# DIY Recipes renderer prototype — ACNH 3.0.3

## Scope and evidence

0.3.32 retires the 0.3.24–0.3.31 workbench-page transplant. Those versions
failed live tests with freezes, incorrect dispatch, missing models and app
startup failures. Their implementation history remains in Git and VALIDATION.md.
The user confirmed visible models in 0.3.37, including a wooden simple bed,
after correcting capture order. 0.3.38 added right-stick rotation, but its
transparency change hid the whole model. 0.3.39 restores opaque capture and the user confirms visibility again, but
rotation has no visible effect. 0.3.40 queues the native transform update and
uses #E5DFC7 for the item-layer clear. The user reports no visible change.
0.3.41 observes actual native transform attempts and clear passes; it is a
diagnostic release and does not claim to fix either behavior. 0.3.42 protects
the preview pose at the native setter and overrides the color before its stack
copy. The user accepts the 0.3.43 cream background. 0.3.44 corrects a missing
carrier level in drawable ownership; the user confirms turning and tilt work.
0.3.45 preserves native poses for walls, floors, rugs and fences and doubles
movement speed for other items. The user reports that 0.3.45 looks good.
Left/right recipe navigation is implemented in 0.3.50; native initial orientation
is restored in 0.3.53. Workbench tilt remains deferred.

This implementation retains MenuRecipe's material panel. It adds a hidden
capture surface to the loaded layout and temporarily hides the grid, scrollbar
and cursor only after native item-model allocation returns a handle. It neither
constructs a WorkCraftInfoSeq page nor loads WorkBenchInfo/WorkBenchBG resources.

## Deferred follow-ups — October 9, 2026

The user accepts the current 0.3.45 result and explicitly requests documenting
these items for later. This is a backlog, not authorization to implement them now.

1. **Hotel request banner placement/layering — implemented in 0.3.48.** The DIY app's “Required: X”
   banner is partially covered by the opaque 3D capture surface. In the native
   workbench preview, the banner appears over the lower-right corner of the
   preview. Make it fully visible in that corner in DIY app preview mode while
   retaining the app's normal banner placement when returning to the grid.
   Reference screenshots show the same cardboard chair request:
   [workbench placement](references/hotel-request-workbench.jpeg),
   [ordinary app grid](references/hotel-request-app-grid.jpeg), and
   [app preview overlap](references/hotel-request-app-preview-overlap.jpeg).
2. **Sensible initial item orientation — implemented in 0.3.53, live test pending.** Use the game's native initial preview
   positioning/orientation data when available. Backpacks are a reported example:
   the workbench initially presents their front toward the camera. Investigate
   the per-item/carrier poses described above before choosing category-specific
   angle overrides. Opening an ordinary item should start at its sensible native
   orientation, then allow the existing right-stick rotation and tilt. Retain the
   accepted fixed poses for wallpapers, floors, rugs and fences.
3. **Recipe navigation inside preview mode — implemented in 0.3.50.** Left/right on both the left stick
   and the D-pad should move through recipes in the app's current list/category
   order, updating the 3D item and existing crafting/material information while
   keeping the preview open. The right stick remains the model rotation/tilt
   control. B should return to the DIY app grid at the current recipe.
4. **Vertical tilt in workbench previews.** Extend the DIY preview's right-stick
   up/down tilt to native workbench 3D previews as well. The user suggests a
   separate setting, enabled by default and optionally disabled. Keep this
   independently configurable from the DIY app preview feature. In 0.3.46,
   `workbench_preview_tilt=true` is accepted as a reserved preference; its
   implementation remains deferred.

The user approved grouping the existing DIY app previews and Faster Crafting
into one optional **Crafting** submodule in 0.3.46; see the
[Crafting grouping note](../crafting/NOTES.md#possible-crafting-submodule-grouping--october-9-2026).
DIY previews move from Core to Crafting and use `diy_app_preview=true`.
The implementation now lives under `source/program/crafting/`. The follow-up UI
changes are tracked below; workbench tilt remains deferred.

The attached reference images are retained here so later work does not depend
on the original files remaining in Downloads. The original backlog update made
no runtime changes; 0.3.46 changes module ownership/settings only and leaves
these UI follow-ups deferred.

## 0.3.33 loading-crash correction

The synced 0.3.32 log faults in native Pane lookup (main+0027D224, return
0027D228) with the compiled Bind callback at project+78F8. Resolving the live
main base 80DDD000 makes the supposed root 855D6770 equal main+047F9770:
a SafeString virtual table. No model request has occurred. The regression now
models the actual wrapper/accessor distinction and executes native recursive
lookup during every compiled Bind, rather than replacing Find with a name map.
It reproduces the old invalid traversal, then checks the accessor output path,
invalid/unbuilt roots and missing accessors. Core declines lookup unless the
returned root is named RootPane and its child-list sentinel is initialized.

## 0.3.34 shortcut input correction

The 0.3.33 live log reaches app binding with valid surface/grid pointers but
input_row=-1, and the user reports A does nothing. Native 02E936EC allocates
shortcut kind 5 into +570 and kind 6 into +578 (02E93798..02E937C8). Requiring
kind 3 at +570 was copied from pocket-page Back routing and does not apply to
MenuRecipePage. The test now executes native shortcut setup rather than assigning
a kind-3 descriptor. It fails against 0.3.33 and checks the corrected routing,
including invalid/missing shortcuts and an input row assigned after binding.

## 0.3.35 direct A input

The live 0.3.34 log shows valid input_row=0 and press_bindings=1 but no A queued
or declined. Injecting a row-5 event in the regression did not establish that
native controller A generates that event. The early readiness guard also lacked
logging, so absence of a log cannot distinguish those cases.

A now comes from the shared snapshot (mask 10 hex); MenuRecipePage's actual
virtual table entry 049E7F10 points to 03C58D24. Row-0/1 delegates are wrapped to
observe focused recipe membership while invoking their original callbacks.
Native 02E96A80 maps scrolled physical rows to the selected recipe; native view
updates are retained. Row-5 delegates are untouched. Input activation requires a
fresh press after release, a focused recipe, ready phone and unblocked/unconsumed
input. Native 03C4B880's page/global blocker is mirrored before queuing. Diagnostics
report A input before readiness rejection, including phone state and pending slots.

## Native bindings

All addresses are main-module offsets in build FF1D1C05670DB6021C85B624A710B963.

| Offset | Observed role |
|---|---|
| 00267D64 | nn::ui2d::Layout::Build, resource pointer x6, options x7, ninth bool on stack |
| 0026803C | Layout loader: resource-accessor virtual +50, then Layout::Build |
| 0027D1D0 | Recursive native Pane name lookup |
| 02E9CDE0 | MenuRecipe callback binding |
| 02E96A80 | Recipe focus-enter/selection callback, row 0 |
| 02E99D90 | Empty focus-leave callback, row 1; **not recipe confirm** |
| 02E99D94 | Empty recipe press callback, row 5 |
| 02E99D98 | Existing phone B/favorite command callback |
| 02E9FBB0 | Phone per-frame update |
| 02E9D538 | Phone deactivation; retains page/layout for reopening |
| 0375AA40 / 0375AB74 | Native button dispatch / raw-event translation |
| 03C58D24 | Common page input, shared with Pockets Quick Store |
| 02E936EC / 02E93798 | Native MenuRecipe shortcut construction / kind-5 allocation call site |
| 04AB2230 | GOT → singleton storage → UI context |
| 032B78A8 / 032B9210 | UI3D controller construction / initialization |
| 032B8270 / 032B85D0 | Model state-1 entry/exit: balance camera and scene layers |
| 032B9510 / 032B9E08 | Native parameter animation / final model rotation setter |
| 039392B0 / 039F63F0 | Layer draw color copy/override / render-target drawing |
| 032BD990 | Item request, accepts controller mode 2 and null background page |
| 032BA5E4 / 032BA674 | Matching-mode release / handle cleanup |
| 03DBB160 | Synchronous state transition: old exit, new entry |
| 03A17390 / 03A174F0 | Picture type casts used by capture binding |
| 03C66298 | Native wrapper +58 → accessor virtual +28 → output layout |
| 03A145F0 | Resolve DynamicCaptureUseName/Index and bind picture to capture pane |

## Ownership and input

Phone fields: resident byte +1F8, layout +210, selected index +218, recipe count
+8A350 and pointer array +8A358. Recipe item data is eight bytes at record +8.
Layout +3F8 points to an array of layout-instance wrappers. The wrapper is
**not** a native drawable layout: its +18 contains a SafeString virtual table.
Wrapper +58 is an accessor whose virtual +28 writes the native layout to an
output pointer (verified in 03C6627C). Native layout +18 is the root pane.
Layout +580 is the grid controller. Grid count +208
and row-pointer array +210 identify the actual pressed recipe row.

Native focus callbacks (row 0 enter, row 1 leave) are observed while their original
functions run. Only native recipe focus/leave entries are replaced; row-5 press
entries remain native. Focus movement cannot queue a model. Setup readiness
requires phone state 3 and both queued transitions equal -1. A from the shared
controller snapshot (mask 10 hex) queues work on a fresh unconsumed edge; model
calls happen on the following app update, outside native input/dispatch stacks.

Core installs one shared PageInput hook. A handler returns true only while its
phone preview owns input; otherwise native input runs once. Pockets registers
its existing Quick Store observer without installing a second hook. The phone
shortcut at layout +570 → +A0 supplies the input row (+10) and native kind
(+14, expected 5). Routing is refreshed while idle. Input snapshots have count
+8 and rows +10, stride 48 hex, held buttons +0 and consumed buttons +3C. B mask
20 hex is consumed on press; grid/category input is suppressed until B is released
after closing. Initial held A cannot open a preview on app entry. Page/global
blocking follows 03C4B880: page input +54, +58 mask, application +418 → +10 mask.

Per-resident sessions retain no workbench-page pointers or cloned virtual tables.
Native phone deactivation releases the model and restores panes before the
original deactivation. Reopening retains the bound session; a new Bind resets it.

## Renderer and request ABI

UI context +28 is a **parameter store**, not the renderer. UI context +30 is
the 11C8-byte model controller. The active phone already reserves mode 26/state 3;
native virtual +200 at 049E8378 returns 26 through 02EA0754. Base app frame 02B0C720 calls PhoneFrame,
then reserves its reported mode (21..31 use state 3). The 0.3.35 live log
confirmed A queues the correct recipe but the idle-only guard refuses it.

Accept only idle mode -1/state 0 or phone mode 26/state 3, with handle +6C=0
and no requested/active background page. Save the mode; publish mode 2/state 1
and report mode 2 through the phone's writable virtual slot while owned.
Otherwise the base frame immediately overwrites mode 2 with 26. The tiny
constant-return function itself must not be trampolined: neighboring code starts
only eight bytes later. Mode 2 is the native workbench item profile.
Controller +58 is the requested background page, +60 the active visibility
page, +68 the mode flag, +69 camera-enabled, +70 model type, +74 loading flags,
+8C item data and +9C recipe index.

Publish the item to parameters +2C, show it through +12, set flags +40=100,
recipe +42=FFFE, +44=0, update flag +48=1 and dirty +2140=1. Save and restore
+12 and the complete +2C..+53 range. Request controller state 1 synchronously
through 03DBB160(controller+8, 1); wait until the next app frame to create:

```cpp
struct ItemRequest { // 24 bytes, AArch64 alignment
    uint32_t display;              // 0
    uint32_t itemLow, itemHigh;
    uint16_t recipe, padding;       // FFFE, 0
    void* visibilityPage;           // nullptr
};
```

032BD990 sets type 4, requests entity kind 42 hex, copies item data and sets
loading flags. The null visibility-page branch is valid native code. Release
uses 032BA5E4(controller, 2), which checks mode, removes the handle and transitions
to state 0. The state-1 exit balances camera/layer changes; state-0 entry resets
mode and background pointers. When borrowed, restore mode 26 and enter state 3
through ChangeState before returning to the app. Native state 3 entry 032B8924
and exit 032B89D8 also balance the camera reference at camera +68 -> +3C.
The app's normal close then releases mode 26. Do not merely clear the handle or mode.

## Capture layout

The retained in-memory BFLYT inserts hidden N_QolCapture_00 as the first root
child and appends a hidden N_QolPreview_00 parent containing P_QolCapture_00.
Both siblings are explicitly shown after allocation and hidden on close. The
pan1 capture carries author-defined
DynamicCaptureOn=R10G10B10A2 and CaptureBGAlpha=255 (restored in 0.3.39). The pic1 references the
capture by DynamicCaptureUseName and DynamicCaptureUseIndex=0 and a new
Capture^t texture/material. usd1 entry offsets are relative to each entry.
Native binding writes the capture pane to typed picture +100 or alternate
picture +130. Missing binding declines A before taking renderer ownership.

0.3.36's valid handles and cleared loading flags narrow its blank output to
drawing. Native 03A2E5D0 copies the current framebuffer crop via 0384A8E0 when
CaptureBGColor is absent, then draws capture children through 0027E140.
WorkBenchInfo's N_Capture_00 is its first root child, has no children and sits at
(-430,-64); P_Capture_00 displays it later. Thus the scene image is already in
the framebuffer, and capturing after MenuRecipe's backgrounds loses it. 0.3.37
matches the native early-capture/late-picture ordering. 03462284's capture pane
factory only resolves pane references; it does not connect a separate 3D scene.

The patch validates section sizes, hierarchy, names and table offsets. It adds
records to texture/material tables while preserving all existing indices and
record bytes, and preserves all other sections byte for byte. No installed
aspect-ratio layout file is replaced. The authored surface is 850×750 at
(-430,-64) in the native layout coordinate space; its suitability for different
items and 16:10 still requires live inspection.

Layout::Build retains the BFLYT pointer. A bounded cache of 16 aligned 64 KiB
buffers persists for the process lifetime; buffers are never recycled over live
layouts. Concurrent loads do not block; contention, invalid data or cache
exhaustion returns the original resource. This uses static memory, not UI heap
page allocation. It costs approximately 1 MiB of BSS.

## Transparency and rotation (0.3.38)

03A2DAD4 sets capture flag +D2 bit 1 when CaptureBGAlpha is 255 and no
CaptureBGColor is supplied. Alpha 0 leaves this bit clear. The native parser
fixture proves that behavior, but does not establish useful alpha in the 3D
framebuffer. The 0.3.38 live test made the whole item invisible despite successful
loading; preserving source alpha therefore appears to hide its color output.
0.3.39 restores alpha 255, as in the working 0.3.37 build and WorkBenchInfo.
Do not supply CaptureBGColor: that selects a clear instead of copying the model.
0.3.40 colors the item-rendering layer before capture, as described below.

Native 03C4BAC4 dispatches float axis pairs at snapshot row +4 + kind*8;
kind 1 is right stick (+0C/+10), with consumed-axis bit 1 at row +40. Collect
right-stick input only during Showing on the page's actual shortcut input row.
Apply it once in the following phone frame, after native update; drop it when
blocked, consumed, loading or no longer owned. Use a 0.15 dead zone and three
degrees per update at full deflection. Yaw wraps; pitch stays within ±60° of the
native pose, preserving roll and position. Reopening resets to the native pose.

032B9E08(controller, Angle32[3]) validates the handle in the native entity
registry, its type and execution context, then writes component +2BC/+2C0/+2C4.
The vector is cached at controller +80; native 032BA000 passes that vector during
loading. 032B74F0 multiplies source radians by 2^32/(2π), establishing Angle32
units. Controller +78/+7C and helper 032B97A0 are position, not rotation.
The first rotation implementation called this helper directly and cached its
result only on success. The 0.3.39 live test showed no rotation. Direct writes
can be rejected in the UI entity phase. Later native parameter animation can
overwrite accepted rotation (see 0.3.42 below).

## Deferred rotation and item background (0.3.40)

Cache the angles at controller +80 and OR transform-pending bit 0 at +74.
Native late update 02B0E20C calls 032B9FCC, which retries position/rotation in its
entity context. For mode 2 it then calls 032BE2B0, setting item entity +398=1
before clearing the pending bit. Later RE establishes +398 is the material
publication gate, not a transform dirty flag. Preserve the material bits and allow further
input to update an already pending transform. A duplicate consumed input
callback must not erase movement waiting for the phone frame.

01513A48 establishes item camera/layer index 2. The layer manager is obtained
through GOT 04ABDB78 and its singleton storage; +20 is count and +28 the layer
array. Native layer draw 0386D730 passes layer +9C RGBA to 038025B0. Its ABI is
(float depth in S0, buffer X0, command X1, target W2, mask W3, color X4,
stencil W5, viewport X6, apply W7). Substitute #E5DFC7 normalized RGBA only
when color points to this layer's field, target is zero and mask includes color.
Publish only that address atomically while the app owns the renderer, clearing
it before release/deactivation. Preserve native color storage, depth/stencil,
viewport and other targets/layers. A missing manager/layer leaves ordinary
preview capture available. Capture alpha remains opaque; no capture clear is
introduced. This reaches a graphics API call locally; live GPU color remains
unverified.

## Live schedule/clear observation (0.3.41)

The synced 0.3.40 log establishes right-stick snapshot values and queued angles,
not native application. Its flags=1 status can reflect continuous new input.
The tests explicitly invoke 02B0E20C after the app frame and set entity phase 1;
this scheduling is a fixture assumption, not live evidence. Likewise the clear
fixture deliberately calls layer 2; it does not establish which live pass clears
the preview background. Dynamic capture's native draw 03A2C7AC calls 03A2E5D0 on
every visible draw, unlike static capture's dirty-flag branch at 03A2DF70.

Observe 032B9FCC with a trampoline that always calls its original once. An atomic
controller address scopes counters and eight before/after pending-bit reports;
include native entity phase/stage without modifying either. The clear observer
logs up to 16 distinct caller/match pairs with original color bits and arguments.
It never reads mutable Sessions. The atomic scope ends before release; totals
are printed from the UI update after 30 frames and before cleanup. The late-pump
hook uses the existing verified entry fingerprint. No extra update is scheduled.

The new fixture redirects native 02B0E20C's tail branch through the compiled
observer and executes the original pump. Diagnostic checks require failures to
remain failures, successful updates to remain successful, bounded output and
scope removal before release. Live trace data is required before selecting a
new entity-update or background-rendering path. Background/rotation remain
unfinished, and the post-release unmapped-read origin remains unestablished.

## Native pose overwrite and copied clear color (0.3.42)

The synced 0.3.41 log confirms 138 native updates, 77 pending attempts and 77
successes, at entity phase -1/stage 2. All 2,662 clear calls missed the published
layer-color address. Scheduling and input are established; pointer matching at
the low-level clear is wrong.

032B9510 runs from UI context update 03285C50 -> 03285C74. When controller +338
parameter animation is active, it builds a fresh pose with 032B74F0 and calls
032B9E08 at 032B9754, independent of the queued +80 angles. Protect the owned
preview at the setter boundary: supply controller +80 for both this call and
032B9FCC. Preserve the native setter's handle, phase and RTTI checks; clear the
atomic owner before releasing the renderer. Other controllers pass through.
The local fixture executes the native animation body, reproduces its overwrite
with the hook bypassed, and prevents it with the hook. Native component matrix
builder 006021A4 consumes both +2B0 base angles and +2BC additional angles;
the fixture checks its complete 3x4 drawable matrix changes and stays changed.

039392B0 gets the rendering layer from draw context +3D0. It copies layer +9C
RGBA to the stack and calls 039F63F0 at 039393DC. The latter clears through
038025B0 at 039F6E00, exactly the stack-color call observed live. Instead hook
the layer draw and pass a local copy of its 0x28-byte color options only for
the published item layer. Flags +10: clear bit 0x100 (global color precedence),
set bit 8 (per-draw override); +24 packs RRGGBBAA as E5DFC7FF. Native conversion
produces normalized RGBA. Preserve the original options, layer storage, other
layers and all downstream native render operations. Missing layer metadata
leaves ordinary rendering available. No capture clear or alpha change is used.

The ARM fixture executes 039392B0 through its stack-copy/color-conversion path,
then models the postprocessing boundary and executes the native GPU-clear body
with those copied bytes. It verifies original color options and layer memory
are untouched, including global-color precedence. Layer draw totals and a
single first-match report replace the failed low-level clear diagnostics.
These checks establish the native data paths, not live visual success. The
Deck test must confirm the selected render layer reaches the captured model,
rotation persists, and B/reopening remain stable. The prior post-release
unmapped-read origin remains unestablished.

## Displayed mesh matrix and display-color experiment (0.3.43)

0.3.42 still does not rotate visibly. Every logged pending update succeeds and
layer draw matches each frame. The screenshot's flat background is CCC9C0
(sampled away from edges), despite the normalized E5DFC7 scene clear.

019282FC resolves the parent handle at owner +234 through 0061CB58, with own
handle +230 supplied for access validation. It copies parent transform +68
position, base angles +2B0, scale and extra angles +2BC to the displayed mesh's
component. 019283F0 tail-calls 006021A4 to build its matrix. Parent-only matrix
tests miss this handoff. 0.3.43 hooks 006021A4 (48-byte return through x8),
matching the preview handle against the component owner's +230/+234. It passes
a local 0x2C8-byte copy with the atomic preview extra angles to the original.
No shared component writes, registry traversal or additional entity ticks occur.
The test deliberately leaves the mesh's angles stale and runs 019283F0 through
the hook, then verifies the full output matrix and unchanged component bytes.
This is a reproduced failure mode, not a confirmed live cause. Matrix call and
match counters are needed to establish that the live preview uses this boundary.

Scene clear RGB and final display RGB differ. 0.3.43 removes the packed-byte
options override, retaining original native draw flags; 039392B0 derives those
flags partly from whether a color override exists. A 039F63F0 hook substitutes
float HDR clear values only during the enclosing owned layer draw. Eight atomic
slots key command buffer, renderer state, view and draw-info. Exhaustion skips
the substitution. Slots clear on synchronous return; unrelated passes retain
their arguments. The first substitution and totals are logged.

The float values are empirical: inverse the ACES approximation
(x*(2.51*x+.03))/(x*(2.43*x+.59)+.14), fit each channel's exposure to normalized
E5DFC7 -> screenshot CCC9C0, then invert the requested E5DFC7 display target.
This produces approximately (1.5950871, 1.3879482, .8751216). The game's actual
curve has not been identified; screenshot compression, exposure and render-target
clamping can invalidate the approximation. Do not describe this as exact RGB
or a confirmed background fix. Only a new live screenshot can validate it.

## Validation boundary

Compiled hooks, resource transformation, native focus mapping/button dispatch,
snapshot A input, Pane lookup and item request/release bodies execute at two
load addresses. Tests preserve original panes and a modified canvas/grid;
check focus movement, missing capture, unrelated press, B consumption, held-B
drain, renderer-busy/failed-allocation recovery, parameter restoration, repeated
open and deactivation. Each integration fingerprint is corrupted individually.
Core tests cover all 64 optional-module combinations and shared hook ownership.

GPU capture, native UI factory construction, scene/entity allocation and mutex
operations are modeled boundaries. The lifecycle test now executes native ChangeState, state 0/1/3
callbacks and ReleaseModel against camera-counter fixtures; it verifies balanced
references across B, reopening and deactivation. Native base app update executes
and reverting its mode getter reproduces immediate mode reclamation. Passing
tests establish the ABI/control flow covered above, **not a rendered item or a freeze-free live game**. The next Deck
test must establish cream background, rotation feel, materials remaining visible, B
return, reopening and ordinary workbench/Quick Store behavior afterward.

## Carrier hierarchy correction (0.3.44)

The 0.3.43 live trace has MatrixMatches=0 despite thousands of matrix calls.
The immediate-parent fixture in 0.3.43 was insufficient: the actual allocation
chain has another level.

- Factory 006402E0 uses registration table 04B969C8. Registration 01930264
  selects constructor 01929CD0 for type 42; vtable 048FAC58 +38 selects
  initializer 0192A1F0. That initializer requests type 6 with the type-42
  handle as parent, storing up to four child handles at +274, count +394.
- Type-6 registration 0193AE38 selects constructor 01935790; vtable 048FC968
  +38 selects 019362F4. It prepares type-6F requests at +278 and +35C,
  with the carrier's handle at +364. Type-6F registration 01928280 selects
  constructor 01925120. Its drawable transform uses 019282FC / 019283F0.
- Thus the drawable's +234 parent is the type-6 carrier, not the type-42
  controller actor. The old scope cannot recognize this grandchild.

Publish carrier IDs after the original transform pump returns, while still in
its native scheduling path. Resolve only the owned root through the same
registry indexing as 032B9E08; validate its full handle, dead bit and kind 42.
Reject child counts above four; publish zero for absent slots. The render
matrix hook reads atomics and its supplied component owner, matching kind 6F
and the full carrier parent ID. It copies the component locally and preserves
native matrix output ABI through x8. No new hook or scene update is installed.
B clears the scope before releasing the controller. Accepted HDR background
values remain unchanged from 0.3.43.

The ARM fixture executes both native initializers with modeled allocation,
component construction/attachment and factory boundaries, and checks the
intermediate request kinds and parent handle. Its rotation test now uses
controller -> carrier -> drawable, reproduces the old scope's miss, verifies
all four published carriers, generation/kind/liveness/count guards, immutable
component bytes and B cleanup. Local execution does not prove live visuals or
asynchronous scene timing; those remain for the next Deck test.


## Native fixed poses and faster movement (0.3.45)

The user confirms 0.3.44 rotation and tilt work, requests fixed poses for
wallpapers/floors/rugs, adds fences, and requests twice the movement speed.

Workbench 0348D8BC calls the same CreateItemModel 032BD990 used here, with
display 0 and controller mode 2. The request contains item data rather than
category-specific angles. 032BB4EC/032B74F0 read the native controller's live
preview parameters (angles at controller +80). Root update 0192A590 separately
selects item/placement-specific preview parameters through 0192A0F4. The
019305D0 parameter constructor names its resources, including mRemakeFence
and mRemakeFenceScale; the fence entry's array count/data are +3B10/+3B18.
At 0192B394 the root converts selected degree-valued XYZ angles to Angle32;
it writes the carrier's extra angles at component +2BC/+2C0/+2C4 after computing
bounds. Other branches choose placement and scale. Drawable update 019282FC
copies the carrier transform, and 019283F0 builds the final native matrix.

Preserve this whole native path for fixed types rather than guessing or copying
one numeric pose from a particular recipe. Core reads validated live ItemParam
rows through GOT slot 04AC0428 (table +1F8/+1D8/+1E0). Match ItemKind CRCs for
RoomWall, RoomFloor, Rug and Fence, accepting English/Japanese enum spellings
and requiring UniqueID to agree. This is independent of the Catalog module.
Read the resource once when acquiring the preview; never on a render worker.

Publish a fixed-pose flag before publishing controller ownership. For fixed
items, drain but ignore right-stick movement, pass native rotation-setter
arguments through unchanged, and bypass the custom matrix-angle substitution
for the root, carriers and drawables. Thus native angles, position, scale and
per-carrier framing remain intact. Clear the flag after the active handle is
cleared during B/deactivation, before releasing the native renderer. Ordinary
items continue using the working 0.3.44 path at 6 degrees per update (was 3),
with unchanged dead zone, tilt bound and accepted HDR background. No new hooks.

ARM tests classify every real fixed item and a representative of every other
kind through the actual open/deactivation path, test malformed row/header/
identity and both enum languages, then verify fixed input and native setter/
root/carrier/drawable matrices. They also cover B cleanup, a rotating item after
a fixed item, 6-degree horizontal movement and a 60-update full turn. GPU output
and native resource-loaded poses still require the next live Deck test.


## Hotel request banner overlay — 0.3.48

The user now authorizes the Hotel banner fix. `MenuRecipe` has the native
`N_TradeBox_00` group under `N_InOut_00/N_Contents_00`, with `N_Align_02` and
`P_TradeBoxBase_00` beneath it. The opaque preview picture is a later root child,
so the old banner was partly hidden. No resource replacement or duplicate text
is needed: while Showing, move that same banner group after the preview picture
inside `N_QolPreview_00`. Compute its translated position from the current
background size/origin, intermediate translations and positive scales, placing
its bottom/right edges 16 layout units inside the preview. Native localization,
count, background sizing and visibility are retained.

Before each original phone update, restore the native parent, sibling slot and
XYZ translation, then place the updated banner again after the original returns.
B, deactivation and renderer failure preserve/restore the native grid hierarchy.
Unsupported or missing geometry declines only banner relocation. Intrusive list
neighbors are checked before each move. Pane translations at +30, scales at +48,
size at +50, origin at +5B, parent at +18 and child/sibling lists at +20/+08 come
from native constructor 0027BFB4/0027BFE4 and traversal 0027E208. Dirty bit +58:10
forces native subtree matrix recomputation (0027D550). Those instruction sites
are compatibility fingerprints; behavior remains readable C++.

Compiled ARM tests run native recursive Pane drawing, recording individual GPU
pane submissions at the boundary. At two bases they verify picture-before-banner
ordering, corner alignment with dynamic widths and changed preview dimensions,
native text/visibility, repeated native updates, B/reopen/deactivation restoration,
and no mutation on failed spawn or malformed geometry/links. Existing original
and aspect-modified layout resources remain byte-preserved. Live placement still
needs a Steam Deck check.


## Recipe navigation — 0.3.50

The user authorizes D-pad/left-stick previous/next in preview mode, crossing grid
rows and preserving the selected card on B. Phone +8A350/+8A358 is the current
recipe count/pointer array (already filtered/sorted); +218 is the recipe index.
Do not use the pooled button index as the recipe index. Clamp at list ends.

The original 0.3.50 implementation incorrectly treated input row +0 masks 1/2
as left/right; 0375F29C/0375F240 initialize a scroll enum, not button kinds.
This assumption is corrected in 0.3.51 below. Left stick is +4/+8, consumed-axis bit 0; right
stick remains +C/+10, bit 1. D-pad takes precedence; opposing directions cancel.
Require horizontal stick magnitude >0.5 and greater than its vertical component.
Observe blocking/consumption, preserve queued input on duplicate dispatch, and
perform navigation from PhoneFrame outside the input dispatch stack. Repeat
after 12 UI updates, then every 5 updates (four waiting updates between steps).
Keep one pending step while material/model loading is active; never accumulate
a held-input backlog. B/deactivation/blocked or missing input clears queued work.

The native GridRequest ABI is 0x30 bytes, matching MenuRecipe restoration
02E9C0D0 and filter changes 02E968A0. ResetRecipeGrid 03764290 owns viewport
clamping, pooled-row visibility/repopulation and native cursor state. Preserve
the current top row unless the selected row leaves the visible area, using live
grid column count +2E8 and visible item count +2F4. Pass the full list count,
selected index, first visible index and native app options; never overwrite
grid +430/+440/+444 directly. ActivateRecipeGrid 0335B4A0 requests state 2;
FocusRecipeGrid 03763E14 runs its native input-context/cursor path. The pooled
row lookup 03765104 and native RecipeFocus 02E96A80 refresh phone selection and
materials even when the same pooled row is reused after scrolling.

Workbench 0348DE80 uses ChangeItemModel 032BA168 to update the existing preview
actor. CreateItemModel 032BD990 refuses an existing handle. The item-change
request holds the 64-bit item and FFFE recipe sentinel; native flags +74:2/4
queue changes outside the entity phase and wait for model readiness. Reuse the
owned actor/camera, restore the initial controller angles and clear tilt, update
the parameter store and publish the new fixed-type flag before the native change.
This retains category-specific native poses and avoids renderer teardown between
recipes. Restore the Hotel banner before native details update, then place it
over the capture again. No new hooks or settings.

Compiled checks execute native grid reset, viewport clamping, recipe mapping,
input-list focus and item-change/deferred readiness at two bases. Graphics row
visibility/position writes, scene item rebuilding and material text rendering
remain modeled boundaries. Nonmonotonic item IDs verify list order; 35 recipes
with 32 pooled buttons cover both directions, page crossings and a partial last
row. Checks include tap/hold/reverse, duplicate/blocked/consumed/missing input,
list endpoints, B priority, cursor restoration and reopening. GPU appearance and
actual Deck scrolling/loading timing still need live confirmation.


## Horizontal input and scene pose — 0.3.51

Native 03C49700 routes direction enum 4/8 (logical kinds 2/3) to X; 03C49710
routes 1/2 (kinds 0/1) to Y. Use masks 4/8 for left/right. Test vertical held
bits together with analog Y: zero-button analog mocks hid the 0.3.50 bug.
All list/focus/repeat/consumption handling remains as in 0.3.50.

Workbench 032BF898 reads carrier base yaw through 0358DB20 and sets it through
0358DCB8 / virtual +160. It changes the real carrier pose. Our final 006021A4
matrix override changed drawing alone. Bone-attached effects resolve a parent
model and evaluate its bone world matrix (005FC650 -> 005FC344), then compose
with effect-local matrix +344; altering only the mesh's final matrix cannot
reliably align these attachments.

After native item-root update 0192A590, UpdatePreviewActor copies the published
secondary XYZ angles into each owned carrier component +2BC/+2C0/+2C4. This is
the same storage native controller setter 032B9E08 uses. Native root initialization
and category framing run first; walls/floors/rugs/fences skip the override. Validate
root/child full handles, kind 42/6, parent, dead flag, component owner and at most
four children. Update only inside the native actor update; no UI-thread scene
mutation or forced entity phase. Native drawable transform 019282FC copies
carrier base Euler, scale and secondary angles before model/bone processing.
Keep the existing scoped render hook only until scene propagation begins. Once
the scene owns the pose, use the real matrix: applying newer UI angles at draw
time would move the mesh ahead of its bones by one scene update. Reset this
state on recipe changes and closing.

The ARM fixture runs native root's steady-state mode-4 path and actual drawable
transform, then native effect attachment composition with modeled skeleton output.
Yaw/tilt changes produce matching native (unoverridden) and displayed matrices,
and the effect-local source point stays attached. Full skeleton/particle simulation,
GPU appearance and runtime update ordering remain for live validation. All new
native instruction dependencies participate in atomic installation fingerprints.

## Primary Euler rotation for item particles — 0.3.52

Live 0.3.51 navigation works, but particles remain detached. Its log reports the
scene pose active, ruling out fallback matrix-only rotation. Native item-effect
update 006904C0 gets the owner transform from +68 and directly reads primary
Angle32 XYZ at +2B0/+2B4/+2B8 (00690504 onward). It converts unsigned angles to
radians using 2*pi/2^32 and publishes them at effect component +310/+314/+318.
Secondary angles +2BC/+2C0/+2C4 never enter this path. The 0.3.51 fixture modeled
bone output from the complete drawable matrix and missed this separate state.

Use component virtual +150 (005F7B4C) to set primary XYZ and clear secondary XYZ.
Workbench yaw already writes primary Y via virtual +160 (005F7B74). For clarity,
virtual +140 used by 0358D994 is a position setter, not an XYZ angle setter.
Native drawable transform 019282FC copies primary and secondary rotation, so
both carrier-owned and drawable-owned effects now receive the viewing pose.
Primary and secondary use different combined Euler conventions; this change
uses the primary convention for combined yaw/tilt, rather than claiming matrix
equivalence to 0.3.51. Fixed item types still skip the entire override. No new
hook, input, speed or tilt-limit changes.

The fixture reproduces secondary-only drawing with zero native effect rotation,
then executes 006904C0 with only its final publication service modeled. Both
owner types publish the requested primary radians under yaw/tilt. Actual native
005F7B4C and 019282FC are executed, and existing bone composition/ownership tests
remain. Add compatibility fingerprints for the primary setter and effect read.
GPU particle appearance, full skeleton evaluation and scheduling need live
Steam Deck validation.

## Native starting orientations — 0.3.53

The user confirms attached particles in 0.3.52, but Log pack (ItemParam ID 7498,
BagBackpackWood0, ItemKind Bag) opens backward. CreateItemModel's controller +80
angles come from shared UI3D parameters; native root 0192A590 separately selects
workbench per-item poses and writes the carrier's secondary angles. 0.3.52
replaced these with the controller pose. No category yaw special case is needed.

Wait for root +39C state 0 after native framing (state 1 loads, state 2 applies
poses and returns to 0) and match full item data +360 to the requested item.
Copy the owned component, clear its owner to bypass our matrix fallback, set
scale XYZ to 1, and execute native 006021A4. Its primary rotation is Y-X-Z;
secondary rotation is X-Z-Y. Convert the complete rotation matrix into primary
angles with pitch=atan2(-m12,sqrt(m10^2+m11^2)), yaw=atan2(m02,m22),
roll=atan2(m10,m11). At a pitch pole, choose zero roll and yaw=atan2(-m20,m00).
Use main's atan2f PLT 03F03370, guarded by an instruction fingerprint. Convert
signed radians to wrapping Angle32. Native sine interpolation introduces small
matrix differences; all tested presets remain within 0.001 per matrix entry.

Scene-thread caches retain the converted origin per carrier, recipe epoch, full
generation-bearing handle and component. Recapture when native framing executes
again. UI publishes epoch/item/controller origin; it never resets mutable scene
cache storage. Subtract the controller origin from published input angles and
add that movement to the native carrier origin. Keep secondary angles zero to
retain the particle fix. Before scene propagation, leave the root matrix native
and add only user movement to a copy of carrier/drawable secondary angles. Thus
the fallback also retains the selected pose at zero movement. Navigation and
reopening publish a new origin/epoch. Fixed types still bypass all overrides.

At two bases, compiled integration checks all 53 unique rotations read from the
supplied StaticParam.pack ItemPreviewSwapActor.byml plus five pitch/roll poles.
Tests verify actual native matrix equivalence after conversion, cached stability,
nonzero-preset yaw/tilt and native item-effect radians, loading/item guards,
navigation origin/epoch reset, and reopening with a modeled reused handle. Native
category selection is modeled by supplying resource poses; native matrix/setter,
steady-state root/drawable and effect update execute. SDK atan2f is a modeled
import boundary. GPU appearance and live Log pack framing need Deck validation.
