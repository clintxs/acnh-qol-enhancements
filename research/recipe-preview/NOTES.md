# DIY Recipes renderer prototype — ACNH 3.0.3

## Scope and evidence

0.3.32 retires the 0.3.24–0.3.31 workbench-page transplant. Those versions
failed live tests with freezes, incorrect dispatch, missing models and app
startup failures. Their implementation history remains in Git and VALIDATION.md.
A stationary model inside the existing app is now the first milestone. Rotation
and left/right navigation are deliberately deferred until live model rendering
and B return work. No live success is claimed by the local checks.

This implementation retains MenuRecipe's material panel. It adds a hidden
capture surface to the loaded layout and temporarily hides the grid, scrollbar
and cursor only after native item-model allocation returns a handle. It neither
constructs a WorkCraftInfoSeq page nor loads WorkBenchInfo/WorkBenchBG resources.

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
| 04AB2230 | GOT → singleton storage → UI context |
| 032B78A8 / 032B9210 | UI3D controller construction / initialization |
| 032B8270 / 032B85D0 | Model state-1 entry/exit: balance camera and scene layers |
| 032BD990 | Item request, accepts controller mode 2 and null background page |
| 032BA5E4 / 032BA674 | Matching-mode release / handle cleanup |
| 03DBB160 | Synchronous state transition: old exit, new entry |
| 03A17390 / 03A174F0 | Picture type casts used by capture binding |
| 03A145F0 | Resolve DynamicCaptureUseName/Index and bind picture to capture pane |

## Ownership and input

Phone fields: resident byte +1F8, layout +210, selected index +218, recipe count
+8A350 and pointer array +8A358. Recipe item data is eight bytes at record +8.
Layout +3F8 points to a holder whose first pointer owns the native layout;
its +18 is the root pane. Layout +580 is the grid controller. Grid count +208
and row-pointer array +210 identify the actual pressed recipe row.

Native row 5 dispatch translates raw source +18 to event +8 and raw kind +0
to translated kind +14. Only existing callbacks equal to 02E99D94 are replaced.
Focus movement cannot queue a model. Setup readiness requires phone state 3
and both queued transitions equal -1. Press queues work; model calls happen on
the following app update, outside the native button-dispatch stack.

Core installs one shared PageInput hook. A handler returns true only while its
phone preview owns input; otherwise native input runs once. Pockets registers
its existing Quick Store observer without installing a second hook. The phone
Back hint at layout +570 → +A0 supplies the input row (+10) and native kind
(+14, B=3). Input snapshots have count +8 and rows +10, stride 48 hex, held
buttons +0 and consumed buttons +3C. B mask 20 hex is consumed on press;
all phone grid/category input is suppressed until B is released after closing.

Per-resident sessions retain no workbench-page pointers or cloned virtual tables.
Native phone deactivation releases the model and restores panes before the
original deactivation. Reopening retains the bound session; a new Bind resets it.

## Renderer and request ABI

UI context +28 is a **parameter store**, not the renderer. UI context +30 is
the 11C8-byte model controller. It must be idle (state +10=0, mode +50=-1,
handle +6C=0) before acquisition. Mode 2 is the native workbench item profile.
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
mode and background pointers. Do not merely clear the handle or mode.

## Capture layout

The retained in-memory BFLYT appends a hidden N_QolPreview_00 parent containing
N_QolCapture_00 and P_QolCapture_00. The pan1 capture carries author-defined
DynamicCaptureOn=R10G10B10A2 and CaptureBGAlpha=255. The pic1 references the
capture by DynamicCaptureUseName and DynamicCaptureUseIndex=0 and a new
Capture^t texture/material. usd1 entry offsets are relative to each entry.
Native binding writes the capture pane to typed picture +100 or alternate
picture +130. Missing binding declines A before taking renderer ownership.

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

## Validation boundary

Compiled hooks, resource transformation, actual native button dispatch, native
Pane lookup and native item request/release bodies execute in ARM64 at two
load addresses. Tests preserve original panes and a modified canvas/grid;
check focus movement, missing capture, unrelated press, B consumption, held-B
drain, renderer-busy/failed-allocation recovery, parameter restoration, repeated
open and deactivation. Each integration fingerprint is corrupted individually.
Core tests cover all 64 optional-module combinations and shared hook ownership.

GPU capture, native UI factory construction, camera activation and scene/entity
allocation are modeled boundaries. Passing tests establish the ABI/control flow
covered above, **not a rendered item or a freeze-free live game**. The next Deck
test must establish model visibility, materials remaining visible, B return,
reopening and ordinary workbench/Quick Store behavior afterward.
