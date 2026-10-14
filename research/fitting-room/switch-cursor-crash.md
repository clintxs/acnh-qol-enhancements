# Switch crash on Y: cursor target treated as a UI pane

Report: `01791988062_01006f8002326000.log`, supplied October 14, 2026.
Main is ACNH 3.0.3 `FF1D1C05670DB6021C85B624A710B963`.
Core is `8B64CD255CD855FE985B1D129A987948B84B126D`, shared by release
revisions 6–9. This predates the revision 10 ticket remap.

## Report evidence

- MainThread Data Abort, exception address `0x19`.
- Core base `0x51cf478000`, PC `0x51cf4834e8` = Core+`0xb4e8`.
- X2 is `1`; X24 (focused cursor target) is `0x1f4e4bb268`.
- Stack: Core+`6444`, Core+`a4fc`, main+`3c57ea8`.
- Released NSO code at Core+`b4e8` is `ldr x2,[x2,#0x18]`.
- The preserved pre-header-fix ELF's cursor code is byte-identical to the
  released revision 9 NSO. Its symbols identify `b380..b6a8` as
  `NativeSelection::PlayCursorTap`, `61c0..6864` as `ToggleBagSelection`, and
  `a280..a6d4` as the private fitting input callback.

The crash occurs after successful bag mutation while checking whether hand
feedback belongs to the selected row. It precedes animation playback and
payment. It does not establish the cause of a different user's checkout crash.

## Cause and correction

The code treated SelectCursorSequence+218 as an nn::ui2d::Pane and followed
+18 parent pointers to reach the selected row root. The value is a native
cursor target; its +18 payload is not a pane parent. The report shows the
walk reaching 1 and attempting to read 1+18.

Native FocusButton at 031017F4 loads the button from request+8; 031017F8
loads that button's cursor target from +218, then 03101800..03101808 tests
the target's resident enable mask at +14B0. Pockets' cursor feedback already
compares widget+218 directly with SelectCursorSequence+218.

Fitting now compares selected-row+218 directly with cursor+218. It never
reads the focused target's internals or treats it as a pane. Null/unmatched
identities skip cosmetic feedback. Existing cursor visibility, resident,
selected-row, layout/animation and bag-input guards remain.

## Regression and limitations

`fitting_cursor_crash_arm_test.py` constructs a representative non-pane target
whose +18 payload chain reaches 1. With the preserved old ELF, it reproduces
both the read address 19 and PC Core+B4E8 at two load bases. The corrected ELF
adds/removes the bag item and requests the animation, while invalid +18 payloads
and null/unrelated target identities are harmless.

`feedback_arm_test.py` now models real row/price-binding/pane-child resolution
and a separate cursor target. The prior fixture invented a pane-parent chain
for the cursor target and stubbed RowRoot, masking the type error. It no longer
stubs row-root or logical-index resolution.

Native UI allocation and animation playback remain modeled. The fix still needs
a live Switch test for rendered feedback and a live checkout test. Revision 11
also includes revision 10's X tickets/Y Bag remap. No save was accessed.
