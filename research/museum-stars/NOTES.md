# Undonated creature stars — ACNH 3.0.3

Scope: Catalog module; filled star for Fish/Insect/DiveFish not donated to the
current island museum. No caught/catalog history writes. No name-resource edits.

## Native evidence

- Original local main.raw SHA-256:
  b1ae8e2a0d82f9f38f711c132d1751b221baf9d6411b2be06ebc4bb502ac912d.
- ItemParam ItemKind hashes: Fish 9f46eb0d (80), Insect 91b9b8d3 (80),
  DiveFish e47038b8 (40). Models, eggs, trash, shells, fossils and art excluded
  from donation semantics. Unsupported item flags and unavailable tables fail closed.
- Native museum wrapper 01b172f8: global 05474040 -> manager+10 provider;
  manager+453 bit 0 selects provider+130 versus provider+0. Virtual +38 returns
  island data; the ordinary branch adds 70, the alternate branch does not.
  Museum save view is +1bf198; [view]+[view+8] is its data and view+20 its lock.
- Native query 026bf130 validates item lookup, preserves the complete item,
  normalizes relevant fossil forms (outside this feature), searches 1024 records
  at museum+1004 using native item equality 0267d7b4, and tests donor+3004 != ff.
  Resident identity is deliberately independent of the active player's history.
  Lock callbacks follow native ABI; failed acquisition produces no marker.
- Museum record structure independently agrees with NHSE Museum.cs:
  https://github.com/kwsch/NHSE/blob/master/NHSE.Core/Structures/Misc/Museum.cs
  (Native disassembly above is authoritative for live offsets and ABI.)
- SetTagFromSystem 00c56ee0 uses a 76-entry tag-name table at 0484fc48:
  indices 13=cChachFish, 14=cChachInsect, 57=cChachSeafood.
  Their constructors at c57ae8/c57b3c/c587fc receive SP+400 name and x21 item;
  x20 retains the action. Hooks at c57aec/c57b40/c58800 run before the owned
  substitution registrations (01aaf670/01aaf5a0/01aafa80). Frame size is 1b50.
- Existing pocket/storage and ground balloon hooks retain their controller,
  player and mode guards. Existing generic dialogue hooks use museum semantics
  through QueryCabinetCatalog. All new native assumptions have ARM fingerprints.

## Validation

museum_creature_arm_test.py executes compiled hooks plus the original native
museum reader and item comparison at two ASLR bases, with synthetic read-only
museum data. All 200 species are exercised in pockets and all three catch arms,
with donor ff/0/7; extra cases cover all residents, live donation changes,
ordinary dialogue, storage mode, locks, unavailable resources, special flags,
alternate island resolution, the last record and an absent species. Native
provider callbacks and wrapping/remake metadata caches are fixture services;
no live save is loaded. Hook register context and museum data must remain intact.

Live Eden check still required: catch one undonated species in each family,
reopen its pocket tooltip, donate (including via another resident), then repeat.
Check first and repeat catches, localized grammar/capitalization, storage and
Catalog-disabled behavior. No emulator deployment or live-game claim is made.
