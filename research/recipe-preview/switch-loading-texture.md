# Switch DIY app loading crash: unresolved preview backing texture

Report `01792012573_01006f8002326000.log`, supplied October 14, 2026.
The screenshot and user confirm a crash on the DIY loading screen, before A.
The SD Core exactly matches 1.0.0 revision 11, build
8E424E17D7E236124FBD73AA6E3875433F8F0EFA. Crafting has DIY preview enabled.
This code defect is also present in 1.0.1 before packaging revision 3.

## Evidence

- UILoad thread Data Abort at address 88, PC main+031AA0B4.
- Main build FF1D1C05670DB6021C85B624A710B963 (ACNH 3.0.3).
- 031AA0B0 loads the backing resource at object+10 into X8; 031AA0B4
  dereferences X8+88. X8 is zero in the crash.
- Return main+03A2744C is the virtual resource getter inside 03A273F0's
  registration loop, invoked from main+03BA1B88 during UILoad finalization.
  Its subsequent callback is 03A16DB0, registering the returned texture view
  in the native `ui2d` graphics resource tracker.
- AddSurface appended `Capture^t` to MenuRecipe's txl1 table and used it for
  the new capture picture. MenuRecipe's original embedded BNTX contains 27
  textures, none called Capture^t. The original 15 txl1 names all resolve.
  WorkBenchInfo, whose capture design was used as a reference, DOES contain
  an actual Capture^t image in its BNTX.

Thus an unresolved texture reference is a concrete mod defect consistent
with the null backing resource. The report does not identify the unresolved
texture by name; the exact asynchronous loader-to-handle path has not been
replayed. It would be premature to claim hardware verification before retest.

## Correction

Leave MenuRecipe's texture table byte-identical to the original. The new,
independent picture material references its existing White_00^r image until
native DynamicCaptureUseName replaces the picture's texture during capture
rendering. Capture pane, picture, format, dimensions, userdata, ordering and
visibility remain unchanged. Original materials retain their records/indices.
If a custom layout lacks that named backing image, decline augmentation and
retain the normal grid rather than inventing an unresolvable reference.

## Tests and limits

recipe_texture_resource_arm_test.py runs native 3.0.3 texture dictionary lookup
against the actual MenuRecipe BNTX at two bases. The preserved pre-fix ELF's
patched layout introduces exactly one unresolved reference: Capture^t. Modeling
loader handle resolution from that lookup and executing the actual native
texture getter reproduces read 88 at main+031AA0B4. The corrected compiled
patch declares only existing textures, its selected backing view resolves,
and missing backing names retain the original layout. This models handle
construction; it is not a full asynchronous loader or GPU test.

recipe_preview_arm_test.py now checks unchanged txl1, existing backing
selection, original pane/material preservation, native capture binding, model
lifecycle, navigation and drawing ABI. Live Switch opening, preview rendering,
B/reopen and ordinary UI appearance remain required. Eden also needs a visual
regression check because its previous success did not detect the bad reference.

Packaged as 1.0.1 revision 3; the executable change requires replacing Core.
No save or SD files were modified. Crash report and installed executable copies
were read into /tmp only for investigation.
