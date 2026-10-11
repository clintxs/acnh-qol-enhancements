# Conditional Faster Dialogue, 3.0.3

Target main build FF1D1C05670DB6021C85B624A710B963. All addresses are main-relative.
Original source identity matches the existing Faster Interactions research.
This is a C++ hook rewrite of the 0.3.0 two-tier behavior, with custom selection.

## Original native path and hook sites (see 0.4.7 addition below)

- 01AF9FB0 Print update selects active controller at 02435188 or application
  fallback. 01AFA00C is the common continuation, with x0 still that controller
  (possibly null), x19 the message manager and manager+28 the text object.
  Held buttons are the halfword at controller+8; B/R mask is 4002. The hook
  writes a canonical current Boolean to text+21B4, removing native latching.
- 01AFDD20 is the text update. At 01AFDEC0, native mode/forced/disabled checks
  have already passed; x19 is text. C++ sets w9 to 1/3, which the native store
  at DEC8 copies into text+21B7. Native DED0 sets w20=1, so a second hook at
  DED4 forwards the selected 1/3 flag before the original countdown clear.
- 01AFDEFC and 01A969E0 follow the native main/renderer fast-rate loads. C++
  doubles s0 only for the extra tier. Nonnegative scripted overrides bypass
  both sites. Native fast flag reaches both renderers (+205).
- No added forced-reveal path, parser loop, message/page advance, controller
  write, emote filtering, voice cleanup or code-cave payload is used.
- Flags 1/3 preserve the previous 0.3.0 fast-mode/voice parameter. Multipliers
  are nominal: Western rates commonly 1.2 -> 2.4 -> 4.8; other languages and
  FPS-adjusted native rates are preserved. The personal file supports 2/4.

## Speaker identity and UI classification

The manager at text+208 owns current message metadata and the active speaker
source at +C0. This is also the source used by Animalese at 01B02E34. The source
owns an entity handle at +8. Native 01B02E84..01B02EBC calls the delegate at
source+28 with a 12-byte output for voice POSITION (three floats), not identity.
The original classifier misinterpreted this output until corrected in 0.2.3.
See the constructor evidence and live log analysis below. No position delegate
is called by the corrected classifier; it never starts/stops audio.

01B000C8 resolves this handle through the entity registry: GOT 04AB27C0 ->
global slot -> registry, registry+1C0 -> table, table+8 -> entries, handle>>20
is the index. Native checks handle/generation at entity+230 and retiring flag
bit 1 at +248. NPC kind at +15C is 10; native also validates NPC RTTI via virtual +20
(01B00164..01B001E8). The hook checks that same initialized type (GOT 04AB37D0
and guard 04AB37D8), with an acquire load and no lazy initialization. Its SafeString at +3E0 has resource-name
data at +3E8, used directly by native voice setup. rcm/rct/dod/doc/kpp distinguish
Timmy/Tommy/Orville/Wilbur/Kapp'n independently of the dialogue bank or locale.
NPC IDs are matched exactly (case-insensitively). SpNpcParam has kpp and kppB
entries with matching birth date, packet ID and poster, so the friendly Kapp'n
alias explicitly adds both known profiles. Explicit kpp/kppB IDs stay exact.
The local SpNpcParam.csv and STR_SNpcName hints establish this alias; arbitrary
other model suffixes are not guessed.

Manager+20 is the presentation context. 01B041A4 maps current metadata into
context+2C: talk=0, dialog=1, balloon=2. Confirmed dialog windows are UI. In a
talk window, a valid player entity (kind 4F) is UI/self narration; a valid NPC
is NPC. Except for reviewed player banks, balloon/unknown windows, missing sources, stale handles,
retiring entities, other entity kinds and missing/overlong IDs remain at base.
Missing speaker pointers alone are never classified as UI. Identity is read
on each eligible update; no last-NPC cache survives a change of message.

The field paths are traced from supplied native code. ARM tests use modeled
entities and do not establish that every live context (particularly
catch/terminal/settings messages or offscreen speakers) exposes those paths.
Live logs/verification are needed; unsupported contexts safely use base speed.
The classifier governs only eligible text; Kapp'n song/travel scripting remains
native. No promise of accelerated songs, animations or every settings screen.

## SIMD adapter correction

Upstream inline_asm.s pushes GP then SIMD, so final SP points to SIMD, then GP
at +200. Its original InlineFloatCtx C++ declaration placed GP first. We match
the declaration to the assembly and assert offsets, retaining ordinary
InlineCtx and the adapter's LR entry convention. Regression tests execute the
compiled adapter, not just a fabricated context with the new C++ callbacks.
None of these sites depends on incoming NZCV after the callback; the native
instructions overwrite/test their own conditions before any conditional use.

## Validation boundary

The original native input/reveal/renderer instructions execute with compiled
C++ callbacks and real SIMD adapter at two ASLR bases. Hook allocation and
trampoline relocation, filesystem, metadata lookup, speaker delegates and
parser completion are modeled. Audio/emotes/rendering, controller behavior in
Eden, and actual in-game categories still require live testing. Diagnostic
build: tools/build.sh --verbose-dialogue (bounded SELECT logs).

## Live prototype feedback and diagnostic build

The 0.2.0 personal preset was tested in-game: blue dialog windows (ABD and
save/quit) receive the boost, while the listed NPCs do not. This verifies that
the personal file and UI selection load, but does not isolate the failing NPC
lookup step. No speculative lookup change is included in this diagnostic build.
The ASCII apostrophe in `Kapp'n` is accepted literally by the custom parser and
is covered by the host and compiled ARM preset tests. INI editor highlighting
does not govern this parser.

`tools/build.sh --verbose-dialogue` now reports a reason for each classifier
outcome, window/entity kinds, recognized ID and selected speed. Up to 128
transitions are logged; repeated frames do not exhaust the trace before an NPC
is reached. `npc-type-unready` distinguishes the conservative RTTI guard from a
missing source/handle or unmatched resource ID. The normal build stays quiet.
To collect evidence: replace Core with the diagnostic Core, keep Faster Dialogue
and the personal mod enabled, restart, speak to Timmy/Tommy or Orville without
B/R, then collect the READY and SELECT lines from the Eden log.

## 0.2.1 player/self category

`faster_player` defaults off and selects the same boost tier as UI/NPC/B/R.
A valid player entity now belongs to this separate category. Five reviewed
3.0.3 message banks also identify self text without an entity: FTR_WorkingTable,
SYS_Player, SYS_Get_Fish, SYS_Get_Insect and SYS_Get_DiveFish. Workbench labels
001/011/021 are crafting prompts; 031/032 are craft/customization results.
SYS_Player:509_01 is recipe learning. The catch banks contain catch narration.
Only exact bank basenames (optionally a directory/.msbt suffix) match; arbitrary
SYS_*, FTR_*, NPC, object or absent-source messages are not automatically self.
Known self banks take priority over a dialog window's blue presentation.

The classifier reads the CURRENT bank's SafeString data at manager+2488.
Native 01AF89A0..01AF89E0 gets the pending MessageID bank and active buffer;
01AF8A60..01AF8A84 copies that bank into the owned active buffer. 01AF8A98..
01AF8B50 compares/copies it into the resource cache at +25C0. Message-ref
metadata at +24D8 and its name at +24F8 are handled separately. Reading the
pending object at +170 or a global last-bank cache could classify the next
message early or leak a previous player boost into NPC speech; neither is used.
The new producer fingerprint and ARM test execute that original native copy.
MessageID getters and buffer termination/memcpy services are modeled; bank-copy
instructions, compiled selector and speed path execute. Live coverage still
needs verification; the outstanding NPC entity lookup issue is unchanged.

## 0.2.1 live failure and second diagnostic

The user tested a crafting table, fish catch and bug catch with no observed
automatic boost. The modeled native bank copy establishes one producer path,
but does not establish which metadata is present during these live interactions.
Rechecking native 01AF8800..01AF9C6F confirms +2488 is copied into the +25C0
cache and cleared during a presentation transition (state +88 == 3). No change
to the lookup is justified without observing its state at reveal time. In
particular, blindly falling back to a cached or pending bank risks stale matches.

The `player-bank-v2` diagnostic adds a bounded MESSAGE trace to the existing
HeldInput hook. It records the current +2488 bank, cached +25C0 bank, active
label +24F8, presentation window, source pointer, native text state, forced and
disabled flags, and whether text+210 equals manager+20. It performs no virtual
calls and does not feed cache data into speed selection. Unlike SELECT, this
trace is reached even when native reveal gates prevent the speed selector.
The compiled ARM regression checks cover that blocked path, unsupported window,
changing labels, bounded/unterminated strings, and repeated-frame deduplication.

`tools/package_dialogue_diagnostic.py` packages only diagnostic Core, its license,
instructions, hashes and a matching source ZIP. It requires the diagnostic ELF
marker and refuses to replace existing archives. Obtain READY/MESSAGE/SELECT
lines from the user before treating either player or NPC classification as fixed.

## 0.2.2 window fix from live evidence

The player-bank-v2 log confirms preferences loaded (`faster_player=true`, boost
4) and text+210 equals manager+20. Workbench FTR_WorkingTable:001 and fish
SYS_Get_Fish:Fish_02233 both have window=2, state=1, forced=0 and disabled=0.
SELECT returns unsupported-window with speed=2 before reading their bank.
The bank field assumption was correct; the earlier window filter was wrong.

0.2.2 admits native window kinds 0/1/2 to the exact player-bank check. Known
player banks select Player in a balloon; unknown balloon banks remain Unknown,
not UI/NPC. Unknown window kinds remain rejected. All other native reveal gates
and configured speed selection remain unchanged. ARM regression coverage now
tests every reviewed player bank at window 0/1/2 with independent player/UI
toggles, held/released input, unknown banks/windows, and protected/forced/mode/
scripted-rate cases. These checks reproduce the logged classification conditions;
the corrected build still needs in-game confirmation.

Timmy's SP_rcm_00_common:011/905 instead reaches invalid-entity in a talk window.
This is a separate failure after delegate resolution and registry/table lookup,
before reading NPC kind or name. Native 01B000FC..01B00178 was rechecked: table
index `(handle >> 20)` matches `(handle >> 17) & 0x7ff8` in byte-offset form.
No bank-based NPC fallback is added: shared banks must not confuse Timmy/Tommy.
The optional player-bank-v3 diagnostic includes the player fix and adds all three
delegate output words, resolved entity pointer, stored generation handle/flags,
and resolver function offset to SELECT. Changes to these fields trigger a new
trace, allowing null/stale/retiring outcomes to be distinguished without guessing
at another entity layout. NPC lookup validation itself remains unchanged.

## 0.2.3 speaker identity correction from player-bank-v3

The next live log confirms `player-bank` and speed 4 for FTR_WorkingTable and
SYS_Get_Fish in window 2. NPC failures instead show changing 12-byte outputs
from resolver 01DA973C. Example words 432BF0B6/419B62A2/435823B5 decode as
floats 171.9403/19.4232/216.1395: a spatial voice position, not an entity handle.
01DA973C is the member-function delegate dispatcher. The original source+28
assumption was incorrect, and modeled delegate output hid that error in tests.

The NPC component constructor establishes the real field directly:

- 01D9DC3C..01D9DC48 saves component+799C in the frame slot at x29-28.
- 01D9E11C restores it into x20.
- 01D9E224..01D9E254 sets source=component+7F60, reads the NPC entity's +230
  generation handle, stores it at x20+5CC (component+7F68 == source+8), and
  installs the TalkSource vtable at source+0.
- 01DA374C..01DA3764 passes the same component+7F60 source and entity+230 handle
  to native speaker-name/voice metadata setup (01B000C8).

The classifier now reads source+8 each eligible update and retains all registry,
generation, retiring, entity-kind, RTTI and bounded resource-name validation.
It invokes no speaker position/validity callbacks and keeps no speaker cache.
Four producer fingerprints cover the constructor offset chain and copy.
Regression tests execute original offset setup at 01D9DC3C..01D9DC48, x20 reload
at 01D9E11C and ARM 01D9E224..01D9E254 to produce source+8,
then execute the compiled C++ classifier and native speed path. The old position
callback is a test failure if invoked. Timmy/Tommy switches with their shared
SP_rcm_00_common bank and changing position coordinates are covered separately.

The optional `npc-handle-v4` diagnostic reports one stable owned handle instead
of three changing coordinate words. Movement no longer consumes the SELECT
transition budget. Normal and diagnostic builds both include this fix. The user
subsequently confirmed the corrected diagnostic build works in game.


## 0.2.4 shared configuration layering

Core now reads each module's regular INI followed by its optional .custom.ini.
Dialogue registers one preference parser used for both files; its old private
filesystem loader is removed. Metadata overlays are validated atomically before
preferences are applied. Omitted keys inherit, duplicate preferences restore the
previous layer, lists replace (empty clears), and boost/base normalization runs
after both layers. Enabled/version can be overridden for every installed module;
an absent/invalid regular marker still prevents activation. Native hooks and
speaker/message classification are unchanged from the confirmed NPC fix.


## 0.3.17 native localized name tag observation

STR_NNpcName at 03F7242A and STR_SNpcName at 03F6D1BE have direct references
in 01B0E6B4. That helper checks ordinary/special NPC parameter records and fills
a name object through the game's message resolver; no alias table is added.

The label preparation function at 01B04808 takes the presentation context.
At 01B04824 it reads context+0's manager and manager+C0's active TalkSource
into x24. It constructs a bounded native UTF-16 name holder on its stack,
then checks/invokes the source+128 name delegate at 01B048EC..01B04914.
This is distinct from the source+28 voice POSITION delegate.
01B04B5C..01B04B7C invokes holder virtual +18/+20. The original methods
005F5B28/005F5B30 load holder+8 data and holder+10 capacity; capacity is in
UTF-16 units (64 here), NOT the current string length. At 01B04B80 the name
text is in x19 and capacity in w0, before the engine's name-pane renderer.

Two SIMD-preserving C++ hooks observe 01B04824/01B04B80. The first clears the
previous name even if the delegate fails; the second copies complete, bounded
UTF-16 to UTF-8. The owned copy is tied to manager/source/entity handle. Current
speaker identity still resolves independently and validates live generation,
retiring flags, kind and RTTI before any localized matching. No extra engine
functions are called, no borrowed name pointer survives the native frame, and
name preparation, rendering, audio and emotes run unchanged.

All supplied names fit the new bound. Russian names have grammar tags (sometimes between words)
(group 50/type 22) whose payload contains embedded zero units. The converter
skips length-delimited MSBT tags before checking termination. Invalid encoding,
unterminated/oversized names and malformed tags yield no name, never a partial
prefix; internal ID matching still works. Lists accept valid plain UTF-8 and
ASCII-insensitive exact matching; full Unicode case folding/normalization is
not implemented. Old English aliases and Kapp'n profile expansion are retained.
Native constructor/getter paths and the actual callbacks are exercised with all
14 supplied name banks in ARM tests; live name capture is not yet confirmed.

## 0.3.22 unnamed gameplay category

`faster_unnamed` replaces the public `faster_player` spelling; both map to the
same preference and duplicate-key bit. Either spelling may override an earlier
file. Supplying both in one file is a duplicate and restores the previous layer.
The default remains false. Internal MessageKind::Unnamed replaces Player.

The existing native name-preparation hooks also observe tag presence. At
01B04B80..01B04B8C, null UTF-16 data, zero capacity or an initial NUL branches
to 01B04DA8, which clears talk presentation +31C. A nonempty raw name stays
Named even when UTF-8 conversion fails or exceeds the localized matching bound.
No new hooks or engine calls are needed. Both talk and balloon styles reach
name preparation through 01AF9DA4; that path subsequently dispatches all three
window kinds (01AF9DB4..01AF9DF4).

NameBegin clears the previous observation and snapshots context, source/handle
and bounded nonempty active bank/label SafeStrings (+2488/+24F8). NameReady
records Unknown/Named/Unnamed only if those owners and identifiers still match.
CurrentSpeaker revalidates them. Confirmed unnamed talk/balloon windows use
Unnamed, while blue dialogs use Ui. Named state blocks legacy self guesses.
Unknown state retains the previous five-bank/player-entity fallbacks, including
the legacy self-bank priority over blue windows. Empty cached names, unobserved
preparation and failed conversion are not evidence of an unnamed message.

The reported examples map to FTR_Storeroom, FTR_DressingTable, FTR_Chest,
OBJ_WherearenMncBox, OBJ_WherearenAmiiboDevice, OBJ_HotelAmiiboDevice,
OBJ_Radio_RadioGymnastics and OBJ_PurchaseBox. They require no added allowlist;
item names and other variable text do not change the presence test. Local ARM
checks cover all these groups plus an unlisted group and varied labels at both
bases, native empty-name branches, named/malformed names, missing sources,
message/source/generation changes, independent UI and legacy alias behavior.
The existing reveal gates, seven hooks and localized-NPC matching remain tested.
This classifier still needs an Eden test with those real interactions.


## 0.3.62 extended speed settings

The old speed parser accepted exactly 2 and 4. With defaults, configuring
base=4 / boost=8 rejected 8 and inherited boost=4, producing 4/4. Automatic
categories already used nominal 4x, which helps explain an unchanged impression.

Both settings now accept canonical whole numbers 1..10. Defaults remain 2/4;
invalid values preserve the earlier layer, and boost below base is normalized
only after both files load. No clipping, floating-point settings or unchecked
numeric conversion is added.

The seven hook sites remain unchanged. EligibleFast stores flag 1 for base and
3 for a higher selected boost; ForwardFast copies that flag after native w20=1.
Both remain native fast mode, retaining the existing voice parameter. Bit 1
selects the configured tier in MainRate and RendererRate; each multiplies the
live fast rate by that tier / 2. The default 2/4 rate calculation is unchanged.
Equal speeds use base flag 1. Numeric speeds are never written as engine flags,
and no global last-message scalar is needed by renderer callbacks.

The two rate hooks remain downstream of native scripted override checks;
forced/protected text, parser modes, message state and page/animation waits
retain their original paths. Whole-number speed 1 is nominal fast-rate / 2,
not restoration of all native unmodified input/timer/voice semantics. Higher
values affect eligible reveal rates, not fixed waits or overall conversations.
Local ARM checks exercise all 100 speed pairs in each INI form at two bases,
live-rate changes, both render paths, original tier forwarding, release,
automatic categories without stacking, maximum-speed gate/override cases and
the original 2/4 regressions. Eden confirmation of perceived speed is pending.

## 0.4.7 Y-to-Hurry prototype

Current speed settings accept 1..30. Hold Y (controller+8 halfword mask 0008)
for fixed nominal 30x reveal, independently of configured base/boost. Existing
EligibleFast runs only after native protection/mode gates. New flag 5 retains
native nonzero fast mode and selects 30/2 in both rate hooks. ForwardFast carries
it to both renderers; no parser loop, instant reveal or animation update is added.
Nonnegative native speed overrides bypass these rate hooks as before.

Hurry reads live input using native active-player getter 02435188 and its
04AB3960 -> global slot -> application+F5B20 fallback, both during reveal and
advance. It does not use the Print-only cached B/R input during waits. No
controller writes, persistent hurry flag, last-speaker scalar, or synthetic A.

New inline site 01AFA530 follows the advance-latch load into w22 in native
01AFA4A4. Only w22 changes to 1, leaving the native advance function to process
one page. Requirements: manager/text/context ownership agrees, manager FSM
(+48) is Advance=5, context+28 is ready=2, window 0..2, reveal state +21AC is
2 (page break) or 3 (message complete), countdown +2464 is -1 or 0, message
controller (context+8) is idle=0 or ready=2, and choice controller (context+18)
is idle=0. Opening/closing controllers and revealing/embedded-pause text reject
hurry. Speed selection requires manager Print=3 and the same owned ready
context/idle-choice check. Native FSM +58 is a default/resume state, not a
pending transition, and is deliberately not used as a hurry gate.

Native branches at 01AFA69C onward still clear a continued page via 01AFD78C
and enter Print=3, enter Choice=6 when +2475 is set, or invoke 01AFB6B4 to
close/continue a completed message. +AC keeps its native state-9 path. The
hook never invokes choice processing or forces Print while a choice is active.
The native FSM changes current state synchronously via 03DBB160. Incoming
NZCV is not live at the inline site (following branches test loaded registers).
All sites/dependencies are fingerprinted before any dialogue hook installs.

dialogue_hurry_arm_test.py executes compiled callbacks through the real SIMD
adapter, original reveal/renderer code, original advance function and native
FSM setter at two ASLR bases. Controller lookup, renderer page cleanup and final
UI notification are modeled. Checks cover both page kinds, all three windows,
choice/end handoffs, repeated held updates after handoff, live release/fallback,
manual A preservation, authored waits, protected text, renderer forwarding,
scripted overrides and GP/SIMD/stack preservation. This does not substitute
for live audio/emote/render/controller testing. No important/new-catch filter
or eventflow compiler is part of this prototype; all eligible messages hurry.

## 0.4.7 hurry-v1 diagnostic after live NPC failure

User reports no effect during NPC dialogue. The ARM fixtures validate the
hook mechanics against modeled manager/context/controller state; they do not
prove those gate values match a live conversation. Static review has not
established the rejecting condition. Keep the guard behavior unchanged until
live evidence identifies it; do not infer success from modeled fixtures.

CAS_VERBOSE_HURRY_TRACE is separate from the old broad dialogue trace, adding
no hook sites or Catalog tracing. Build --hurry-diagnostic. Input stage uses
the controller already selected by native Print, before reveal restrictions.
Speed/Advance sample active-controller input even when the common gate fails,
which the initial short-circuit omitted. Trace owner/context agreement, FSM,
context phase, window, reveal, choice/message states, timer and exact rejection
reason. The diagnostic does not write controller or persistent advance state.
Normal builds still sample the extra getter only for eligible hurry contexts.

The marker and HURRY lines intentionally bypass log_debug only in this explicit
diagnostic build. Three independent stage caches suppress repeated frames,
with 96-record budgets per stage per Y-held/unheld category (576 total), so
startup cannot consume held-Y evidence. Bounded message IDs stay within the
512-byte logger buffer, including unknown-state sentinels. No configuration
files or resources ship in the diagnostic Core ZIP. Restore normal Core after
collecting logs; no claimed fix or version bump accompanies this instrumented
0.4.7 build. Tests exercise blocked gates with shipping debug defaults off,
protected input, press/release/fallback, native hurry behavior, budget isolation
and long identifiers at two ASLR bases.


## 0.4.10 live hurry input and waiting-page corrections

The Oct 11 synced Eden log (Core 0.4.9, hurry-v1 diagnostic) captures SP_tkk/082.
At Print=3, raw controller+8 is 0010 while the user holds Y, but the old
mask 0008 reports y=0. Context phase moves from 1 to ready=2 during reveal.
At Advance=5, completed reveal=3, context=2, choice=0, timer=-1 and matching
owners, message-wrapper state 1 is rejected by the old message-transition
guard. It remains 1 while waiting with raw 0000, 0010 and 0001, then becomes
0 on manual advance. These are two independent blockers.

Use native held bit 0010, not platform HID's Y bit. Accept message-wrapper
state 1 alongside existing 0/2 paths; do not infer the message wrapper's
meaning from the separate choice controller's opening/active/closing states.
All ownership/context/window/text/timer/protection checks and the choice
controller's idle-only guard remain. Unknown message states above 2 reject.

ARM fixtures now default to live message state 1 and raw Y=0010. They replay
both Print context phases, held/released completed pages, and native choice
handoff. They also retain state-2 coverage and explicitly verify the old 0008
bit does not activate hurry, including B/R combinations. Diagnostic checks
expect buttons=0010 y=1, and still verify bounded logs with debug disabled.
These checks verify the observed state sequence against compiled hooks and
native advance instructions; Eden sound/emote/render validation is pending.


## 0.4.11 held Y across explicit mid-message input pauses

Live 0.4.10 confirmation: hurry works, except the Sunday turnip-price line
requires releasing and pressing Y again. The local eventflow identifies
Rcm_Common_TurnipChk -> TalkSNpc/rcm/SP_rcm_00_common:082:
"On Sundays we take a little break from buying turnips.<10:4> Please check
back in with us tomorrow." The native input-pause handler at 01AFE49C writes
reveal state 4. Print detects 4 at 01AFA1D4 and enters its normal wait/advance
sequence. The old hurry guard deliberately rejected reveal 4.

Allow reveal 4 at the existing Advance hook, keeping owner/context/window,
choice-controller, timer and message-controller checks. Native 01AFA790..7E0
resumes Print=3 and only clears a page for reveal 2; it does not finish the
message or open its queued choice for reveal 4. Native Print entry 01AF9F74
calls 01AFB490; its 01AFB4E0..4F4 branch changes reveal 4 to revealing=1,
retaining the current text. No direct reveal-state mutation or repeated
controller press is introduced by the hook. Timed waits continue naturally.

ARM checks execute the native pause handler, compiled hurry adapter, native
Advance and Print entry/resume branch at both bases, with and without a
queued choice. Continuous Y remains held throughout and selects the 30x tier
after resume. Additional checks cover release and active choice rejection
while paused, plus existing page/end/native-input/protection regressions.
Live confirmation of this fix remains pending; no new log was required to
identify the explicit pause in the supplied example.


## 0.4.12 locale-independent first-catch announcement stops

Player_GetDemo::Get_Fish and Get_Insect check cFirstGet and open respectively
TalkSys/SYS_Get_Fish:001 and TalkSys/SYS_Get_Insect:001 before their ordinary
species/pun message. Player_GetDemo_30_Diving::Get_DiveFish uses the equivalent
TalkSys/SYS_Get_DiveFish:001. These branches are independent of translated
text and already distinguish new catches without reading or changing saves.
The English announcements are "Yes!"; the same label 001 exists in all three
banks across the 14 supplied TalkSys locale archives. Some translations have
additional page/pause tags and different wording.

AdvanceHurryBlock now checks the current manager's bounded active bank +
label SafeStrings after normal readiness/ownership/choice/timer checks. Exact
matching of the three bank names with label 001 rejects only the mod's local
advance request (diagnostic gate=new-catch). It does not change native input,
automatic/scripted readiness, text speed, animations, save data or message
state. Manual advancement works while Y remains held, including subsequent
pages within the protected ID. The next message is checked afresh; no stop
latch persists into ordinary catch text or a later conversation. Other labels,
including collection-completion 002/003, are outside this requested exception.

New ARM checks read all 42 localized MSBT entries in temporary files, execute
compiled callbacks/native Advance at two bases, and cover repeated held Y,
all three ready reveal kinds, manual advancement, 30x reveal, queued choices,
next-message continuation, exact bank/label boundaries, missing/empty/overlong
IDs, disabled behavior and native automatic readiness. Fixtures now initialize
both active ID buffers every preparation to prevent stale modeled metadata.
Diagnostic coverage verifies the new reason with held Y and logging disabled.
In-game confirmation of the new stop remains pending.
