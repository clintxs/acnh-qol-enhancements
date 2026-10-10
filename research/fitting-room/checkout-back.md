# Wear-choice B shortcut, ACNH 3.0.3 / QoL 0.3.72

Main-relative addresses; SHA256 and build identity match runtime_arm_test.py.

- Fitting controller 031141CC transitions dialog kind 4 -> 5 after payment
  confirmation; kind 5 answer 0 sets command 4 (send), answer 1 sets command 5
  (wear). Actual payment/delivery remains downstream at 0311BD8C -> 0311D3B0.
- 0311BC4C builds the shared dialog descriptor. Its callback is owner+1C4D0.
  02CFA190 copies that pointer to shared dialog+2388. Singleton 05590D38,
  controller vtable 049CD650, active page+210. State 3 runs 02CFCD34, which
  waits for +220 choice or +23AF cancel, then exits normally.
- Page vtable 049CD3A8 uses 03C58D24 at +50. Replace only this writable slot
  (049CD3F8); pass all unrelated input through. The fitting row input slot
  remains separate. Both slots and native helper fingerprints validate first.
- Two-choice buttons at page+578 (No) / +588 (Yes); style 2 uses +8 copies.
  Native 02CFB318 maps clicked buttons to indices 0..7. 02CFCFEC uses the
  normal answer table 040809D0: indices 0/1 -> 0; 2/3 -> 1. Default focus in
  02CFDD68 uses +23AD to select first versus second. This question defaults
  to the second, Yes. Guard active delegate, owner, kind, page, state, pending
  transitions, uncommitted choice, two populated choices and no third choice.
- B binding at page+570 -> +A0 supplies controller row and native key ID 5.
  UI masks use B=1<<5, A=1<<4 (not the raw HID masks). Validate row 0..7,
  context block/masks, consumed buttons, and reject simultaneous held keys.
- Focus manager page+210 -> +48 owns the per-row cursor; cursor+10 is the
  focused target. Each button+218 is its cursor target, +14B0 its row mask.
  031017E0 takes {page, button, row}, changes focus through 03C48B78 and its
  normal callbacks. The first press consumes B but never publishes a choice.
- On a fresh B with No already focused, temporarily map this row's held B to
  A for one call to the existing native page input, then restore the raw mask
  and synthetic A consumption. B remains consumed. The next frame naturally
  releases synthetic A. No save writes, direct checkout or global remapping.
- PressEdge requires an observed release; held-on-open, blocked presses,
  transition frames and repeated held B cannot act as a second press.

fitting_back_arm_test.py executes the compiled wrapper, native page/input and
cursor routines, native button-index callback and original answer handler at
zero and relocated bases. Rendering and UI event adapters are modeled; tests
cover first/second B, held B, already-No, A/Yes, alternate button copies,
blocked/consumed/mixed input, dialog ownership and transitions. Core tests
cover all 64 module subsets and validation conflicts. Live Eden animation,
audio and full checkout remain user verification.

## Bag-only shortcut (0.3.73)

At the existing fitting Dialog trampoline, kind 4 / answer 1 is positive
purchase confirmation. If a nonempty bag has all ten persistent outfit indices
(owner+1C370 + category*12) equal to -1 and command is idle, switch kind to 5
and answer to 0 before the existing preflight. Native 031141CC then queues
command 4 and returns false (no next popup). Native Play dispatch still performs
checkout later. No direct payment or delivery occurs inside the dialog callback.

Any nonempty or unresolved outfit index preserves the original wear question.
No-bag, negative confirmation and unrelated dialog states remain unchanged.
Funds/capacity/stale-stock failures follow the existing preflight error or close
paths. purchase_arm_test.py exercises this transition and exact bag delivery at
two bases, failures/cancellation, plus all ten categories (valid and unresolved
indices). Native save services are modeled; live Eden confirmation is pending.
