# Declarative settings schemas (stage 3)

Controls, Pockets, Crafting, Catalog, Core and Dialogue now declare their
preference names and destinations in `SettingsSchema` arrays. The shared
`core/settings_schema.hpp` owns file validation, line handling, key lookup,
warning counts and per-file duplicate restoration. This stage starts from
`abba993` on `refactor/internal-feature-interfaces`; main, package syntax,
defaults, release version and gameplay behavior remain unchanged.

## Declaring preferences

A boolean needs one schema entry after its member/default is declared:

```cpp
core::settings::Boolean<Settings, &Settings::autoVault>("auto_vault")
```

Nested member paths support Core's logging preferences without special parser
branches. Number entries name their parser; each numeric parser declares the
shared decimal policy explicitly. Schemas use `static_assert(ValidSchema(...))`
to reject empty or repeated names and missing apply/restore callbacks. The
parser uses arrays indexed by schema entries rather than manually assigned bit
masks. It allocates no heap memory and takes one prior-settings snapshot per
file, including Dialogue's list.

| Preference | Range | Out-of-range policy | Leading zeros |
| --- | --- | --- | --- |
| Crafting speed | 0–30 | Clamp at 30; validate the entire decimal | Reject multi-digit forms |
| Dialogue base/boost speed | 1–30 | Reject | Reject |
| Core amiibo timeout | 0–65535 | Reject | Accept |

Decimal arithmetic checks the bound before multiplication, including unsigned
integer overflow. No malformed value publishes a numeric result. Dialogue's
NPC list remains a domain parser: it replaces the prior list, expands aliases,
retains valid entries alongside entry-level warnings, and restores both count
and contents if the key occurs again. Dialogue still normalizes boost/base
only after both files have been applied.

## Overlay and activation contract

Defaults are declared on each Settings type. The loader still processes the
regular file before its custom overlay. Omitted and invalid fields inherit;
any duplicate key restores that field to its value before the file, even when
the first occurrence was invalid. Each later occurrence warns and is ignored.
Unknown keys and malformed lines warn separately. Size above 4096 bytes or
invalid UTF-8 (including NUL) rejects the whole preference overlay before any
mutation. Existing BOM, whitespace, comments, case and boolean syntax remain.

Module preference schemas skip `enabled` and `version` because `ParseConfig`
and the existing loader validate activation metadata first. Core's optional
settings files use `Metadata::None`, so those names remain unknown keys there.
The metadata parser and module loader are unchanged: a custom file cannot
activate an absent or invalid regular package marker.

## Verification

The existing settings suites remain unchanged. `settings_schema_test.cpp` adds
schema validation, nested destinations, valid/invalid duplicate sequences,
metadata modes, exact file-size boundaries, atomic encoding rejection and
numeric-policy edge coverage. `tools/check.py` includes it with the host suites.

A deterministic comparison against the stage 2 parsers (`abba993`) applies
5,000 successive overlays per module, comparing warning counts and all
preference values, including full NPC storage. All 30,000 overlays match.
The replay script and result are saved with stage 3 evidence under ignored
`dist/refactor-stage3/`; run the script from the repository root.
The pinned Docker build completed without compiler warnings or errors. All
15 host suites and all 41 ARM/resource scripts passed, including 978 pocket
callback comparisons against the stage 0 ELF. Main remains at `35ec0c8`;
all 2,976 files covered by the stage 0/1/2 preservation manifests still match
their checksums. Final stage 3 ELF/NSO/NPDM copies and hashes are saved under
`dist/refactor-stage3/final-build/`.

Live Eden validation remains part of later integration qualification.
