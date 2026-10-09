# DIY Recipes app preview — ACNH 3.0.3

The phone app and workbench use separate pages. `MenuRecipePage` has an empty
recipe-confirm callback; `WorkCraftInfoSeq` is the page that owns the large
workbench item preview. Reusing that page retains native material rendering,
model loading/rotation, recipe-specific display rules and shoulder navigation.

## Native bindings

All addresses are offsets from the loaded main module.

| Address | Role |
|---|---|
| 02E9CDE0 | Phone layout callback binding |
| 02E99D90 | Empty phone recipe-confirm callback (row 1) |
| 02E96A80 | Phone selection callback (row 0; retained) |
| 02E99D94 | Separate phone row-5 callback (retained) |
| 02E99D98 | Phone B/favorite command callback (retained) |
| 02E9FBB0 | Phone per-frame model/icon update |
| 02E9D538 | Phone resource cleanup |
| 03479AB8 | Native workbench child construction reference |
| 03492B50 | Common preview page constructor |
| 04A3B778 | Concrete workbench preview virtual table |
| 034930B0 | Preview callback binding |
| 03492FEC | Preview B/favorite command callback |
| 034940E0 | Preview per-frame model update |
| 034949A8 | Preview appear entry |
| 03494FF0 | Recipe selection entry: count, model/material load, animation |
| 03495408 | Return-to-list entry: reverse animation/reset |
| 034947D0 | Idle entry: clear borrowed dataset and model requests |
| 034969D0 | Native left/right index wrapping and item-change notification |
| 03473AAC | Refresh preview count from dataset |
| 03473C74 | Copy recipe data, including six material records |
| 03494D00 | Native preview action dispatcher |
| 0376890C | Page resource initialization |
| 03769260 | Activate a child registered with its parent |
| 03768C7C | Remove/clean up an active child |
| 0376D9A0 | UI manager initialization: build pages, then shrink heap |
| 037C75D4 | ExpHeap adjustment (heap virtual +0x28) |
| 03363544 | Item-icon constructor (270 per preview) |
| 03DBAC90 | Allocate state callback/name arrays from explicit/current heap |
| 03DBAF40 | State-name assignment; null names fault at 03DBB0EC |
| 04AB2698 | GOT pointer to heap-manager singleton storage |
| 04AB3350 | GOT pointer to thread-manager singleton storage |

## Data and ownership

The phone page is 0xF6380 bytes. Its selected index is at +0x218. Its current
recipe dataset is at +0x8A350: 32-bit count and recipe-pointer array at +8.
The preview page is 0x1C210 bytes, has count/index at +0x260/+0x264, layout at
+0x1C1F8 and dataset pointer at +0x1C200. Recipe records have stride 0xD0, item
data at +8 and six material entries at +0x2C with stride 0x18. Native copying
omits alignment padding; it is not a raw 0xD0-byte memcpy.

The implementation borrows the phone's dataset while its input and updates are
suspended. It does not reconstruct a recipe catalog, sort by recipe ID or move
the app's selection. A child is allocated on the native UI heap, registered in
the parent's pending-child list, initialized and activated through the native
page API. Version 0.3.27 prepares the child from the native page-initialization
hook, before the UI manager shrinks its construction heap. Identification uses
only MenuRecipePage's vtable (049E8178). Its child remains at lifecycle 2 until
confirmation queues activation; there are no first-selection allocations.
The synced 0.3.26 log showed a nonnull manager heap rejecting the page allocation.
The earlier null-TLS reproduction did not establish the live allocation history.
The native heap test now reproduces rejection after ExpHeap::adjust.
The app's layout roots are never toggled; failed preparation leaves it usable.
Version 0.3.26 explicitly selects manager+0xB8, current TLS+0xA8 or the heap
manager's default heap (+8), and allocates via heap virtual+0x30, alignment 8.
Native state-machine construction skips allocation if TLS exists but its heap
is null; it does not inherit the default in that case. The old early return
for a null manager heap could therefore crash while initializing the first
icon's state names. The supplied 0.3.25 crash resolves to this null-array site;
the native regression reproduces it with null TLS and checks the corrected
scope. Live heap values were unavailable in that log. The prior libc allocation
also bypassed native heap ownership and is no longer used for this page.
Parent cleanup removes the active child before clearing the session;
an already removed child is not cleaned up twice. Native ownership frees page,
layout, model and dialog allocations. Sessions are separate for each resident.

Native state 1 is idle, 2 appear, 3 play, 4 select recipe and 5 return to list.
The two deferred request slots at +0x1F0/+0x1F4 are selected by byte +0x200.
Wait for idle after initialization before lending the dataset, because idle
entry clears it. Child activation disables parent updates (+0x164), so the
preview's own update override drives these transitions. On return, remove the
active child through the native API after restoring the parent update flag
and returning from child-callback iteration. Detachment runs from the parent's
own update, then restores input. Retain the initialized child for reuse. Its
layout layer is one above the phone's. Open through the normal appear request, then select the app's
index. B requests state 5 and waits for its hide animation before returning to
idle and restoring the phone. This avoids the native full-screen disappear
path, which also manipulates the parent crafting screen/global dialogs.

The preview gets a private copy of its virtual table (including ABI headers).
Override layout binding and model update, return no craft focus target at
virtual slot +0x220, and return false from craftability slot +0x288. The latter
blocks action 2 in the actual native dispatcher before inventory/dialog/craft
code. Clear pending craft events as an additional defense. Disable the nested
craft button and hide its layout root through its visibility controller at
+0x1B0; merely disabling it leaves a visible grey button. Native workbench
virtual tables and objects are never altered.

Only the phone's row-1 confirmation delegate is replaced after native binding.
Native layout-table construction at main+03759DD0 creates 0x23 (35) button-event
rows with at least two columns, and five command-event rows. The 0.3.24 scanner
mistakenly capped rows at 32 and therefore left the confirmation callback empty.
Version 0.3.25 accepts the native button-row count. Tests now execute that native
constructor and confirmation translation/dispatch (0375AB74/0375AA40), rather
than inventing a smaller table and directly calling its confirmation callback.
The preview's B command delegate is replaced after binding, forwarding other
commands (including favorites) to the native handler. Native navigation and
rotation delegates are retained. On return, refresh the phone's selected item
and visible grid cells so favorite changes are reflected.

## Verification boundary

Instruction fingerprints cover all four hook sites and relevant native
construction, binding, state/data and teardown code before installing any of
this feature's hooks. A mismatch disables only the preview feature. Core's
amiibo fix and optional modules still activate independently.

`tests/recipe_preview_arm_test.py` runs the compiled C++ at two ASLR bases.
It executes native heap shrink/allocation and child activation/removal, with
engine service boundaries modeled. It executes base/common page, 270 icon and state-machine constructors with
manager/current/default heaps, modeling their allocation and string services.
It models asynchronous readiness, rendering and animations,
and executes the native recipe copy, list count, next/previous navigation,
item lookup, command forwarding and craft-action dispatcher. It covers empty
and invalid selections, allocation failure, repeated confirmation, reuse,
per-resident separation, cleanup, and fingerprint conflicts.

Live Eden verification remains required. Check furniture, tools, clothing,
wallpaper/flooring and cooking recipes; rotate using the workbench controls;
navigate a one-item filter and a multi-item sorted/favorites list; test B during
loading and after navigation; repeatedly reopen/close both the app and phone;
then verify normal workbench crafting/cooking and favorites still work. Native
resource loading, UI layering and rendering are not proven by the ARM harness.
