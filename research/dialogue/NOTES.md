# Conditional Faster Dialogue, 3.0.3

Target main build FF1D1C05670DB6021C85B624A710B963. All addresses are main-relative.
Original source identity matches the existing Faster Interactions research.
This is a C++ hook rewrite of the 0.3.0 two-tier behavior, with custom selection.

## Native path and hook sites

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
