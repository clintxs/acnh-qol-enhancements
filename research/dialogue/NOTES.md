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
source at +C0. This is also the source used by Animalese at 01B02E34. Native
01B02E84..01B02EBC checks the delegate at source+28 via virtual +10 and calls
virtual +0 with a 12-byte output to obtain the current entity handle. The C++
classifier follows that same delegate; it never starts/stops audio.

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
is NPC. Balloon/unknown windows, missing/invalid delegates, stale handles,
retiring entities, other entity kinds and missing/overlong IDs remain at base.
Missing speaker pointers alone are never classified as UI. Identity is read
on each eligible update; no last-NPC cache survives a change of message.

The field paths are traced from supplied native code. ARM tests use modeled
entities/delegates and do not establish that every live context (particularly
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
