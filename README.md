# QoL Enhancements

QoL Enhancements **1.0.2** is a modular C++ mod suite for **Animal Crossing: New
Horizons 3.0.3 on Eden**. One required Core executable (`subsdk6`) loads seven
optional feature packages. Each package supplies its configuration and any RomFS
resources it needs.

This README covers building and navigating the source. For installation and
player-facing features, see [the packaged user guide](docs/user-guide.txt). Shopping Card appearance
options are described in [SHOPPING_CARD_COLORS.md](SHOPPING_CARD_COLORS.md).

## Build requirements

Run the commands below from the repository root.

- **Docker** for the pinned devkitPro compiler image used by
  [tools/build.sh](tools/build.sh). The build runs with container networking
  disabled; the image must be available or downloadable by Docker.
- **Python 3.14 or later** for packaging and its standard-library Zstandard
  support.
- Your own extracted **ACNH 3.0.3** data. Packaging needs the original layouts in
  `../vanilla-3.0.3/Layout/`; the builders check their hashes before modifying
  them. Original game data is not included in the source archive.

The build targets program ID `01006F8002326000` and main build ID
`FF1D1C05670DB6021C85B624A710B963`. Native addresses and instruction fingerprints
are specific to this build.

The repository includes the target configuration in [config.json](config.json).
To regenerate it from a fresh ExeFS extraction, provide `main`, `main.npdm` and a
local `hactool` executable:

```sh
python3 tools/prepare.py /path/to/exefs --hactool /path/to/hactool
```

This preserves the original NPDM services, filesystem access and META flags,
adding the four process-mapping/session syscall permissions needed by Exlaunch.

## Build and package

Build the normal executable:

```sh
sh tools/build.sh
```

The script performs a clean build with the pinned compiler image. Outputs are
`project.elf`, `project.nso`, `project.npdm`, and the package-ready
`deploy/subsdk6` and `deploy/main.npdm`.

Generate release archives:

```sh
python3 tools/package.py
```

For version 1.0.2, this produces:

| Archive | Contents |
| --- | --- |
| `dist/qol-enhancements-v1_0_2_for_acnh_v3_0_3.zip` | Core, seven feature folders, installation guide and SHA-256 manifest. |
| `dist/qol-enhancements-v1_0_2_for_acnh_v3_0_3_source.zip` | Source, module configurations, captions, tools, tests and notices. |
| `dist/qol-enhancements-v1_0_2_for_acnh_v3_0_3_optional-files.zip` | Personal configuration example and five Shopping Card color replacements. |

Packaging verifies that module versions match Core and refuses to overwrite an
existing archive. Choose an unused packaging revision when generating another
set, for example:

```sh
python3 tools/package.py --revision 4
```

This appends `_r4` to the archive names without changing the suite version.
Generated outputs and archives are ignored by Git. Preserve older archives.

## Source layout

| Location | Purpose |
| --- | --- |
| [source/program/bootstrap.cpp](source/program/bootstrap.cpp) | Validates startup and activates Core after the game mounts its content. |
| [source/program/core/features/core_features.cpp](source/program/core/features/core_features.cpp) | Module assembly, dependencies and amiibo timeout enablement. |
| [source/program/core/modules.cpp](source/program/core/modules.cpp) | Configuration loading, compatibility checks and activation results. |
| `source/program/<module>/features/` | Feature decisions and behavior. Start here when reading a module. |
| `source/program/<module>/game/` | Native adapters, verified layouts and build profiles. Core uses `core/native/`. |
| `source/program/<module>/services/` | Domain services where a feature needs shared rules or a deferred lifecycle. |
| [modules/](modules/) | Packaged INI defaults and editable captions. |
| [docs/user-guide.txt](docs/user-guide.txt) | User guide, included as `README.txt` in the release and Optional Files ZIPs. |
| [examples/](examples/) | Personal configuration examples. |
| [tools/](tools/) | Build, packaging and resource-generation scripts. |
| [tests/](tests/) | Host policy tests, compiled ARM tests and package checks. |
| [docs/](docs/) and [research/](research/) | Architecture contracts, native behavior evidence and research notes. |

The main dependency direction is **feature policy → domain services/native
operations → adapters and build profiles → game functions**. Native checks,
addresses, ABI details and original handler calls belong below the feature policy.
The [architecture guide](docs/internal-architecture.md) explains ownership and
contributor conventions; [AGENTS.md](AGENTS.md) records the repository's working
rules.

### Where to start in each module

| Module | Feature entry point | Detailed reading guide |
| --- | --- | --- |
| Core | [core_features.cpp](source/program/core/features/core_features.cpp) | [Assembly and timeout](docs/native-first-core.md) |
| Pockets | [pockets.hpp](source/program/pockets/features/pockets.hpp) | [Actions and shortcuts](docs/native-first-pockets.md) |
| Faster Dialogue | [faster_dialogue.hpp](source/program/dialogue/features/faster_dialogue.hpp) | [Speed and message policy](docs/native-first-dialogue.md) |
| Controls | [controls.hpp](source/program/controls/features/controls.hpp), [traversal.hpp](source/program/controls/features/traversal.hpp), [swimming.hpp](source/program/controls/features/swimming.hpp) | [Input, movement and equipment](docs/native-first-controls.md) |
| Crafting | [crafting_features.cpp](source/program/crafting/features/crafting_features.cpp), [preview_features.cpp](source/program/crafting/features/preview_features.cpp) | [Crafting and previews](docs/native-first-crafting.md) |
| Catalog | [catalog_features.cpp](source/program/catalog/features/catalog_features.cpp) | [History and markers](docs/native-first-catalog.md) |
| Fitting Room | [fitting_features.cpp](source/program/fitting/features/fitting_features.cpp) | [Bag and checkout](docs/native-first-fitting.md) |
| Shopping Card | [shopping_features.cpp](source/program/shopping/features/shopping_features.cpp) | [Payment and balance routing](docs/native-first-shopping.md) |

## Configuration and resources

Core reads configuration once after content mounting. Restart the game after
changing modules or settings.

- Each optional module has a regular marker at
  `romfs/qol-enhancements/config/<slug>.ini` containing `enabled` and `version`.
  Its defaults are in [modules/](modules/).
- Personal settings use `<slug>.custom.ini` at the same RomFS path, preferably in
  a separate configuration mod so suite updates preserve them. Values load in
  **defaults → regular INI → custom INI** order. A custom file cannot activate an
  absent or invalid regular package marker.
- Core's `core.ini` and `core.custom.ini` are optional settings files, with no
  `enabled` or `version` metadata. They configure the amiibo timeout and logging.
- Pockets and Fitting Room captions live in their own
  `romfs/qol-enhancements/messages/` INIs, separate from settings.

An invalid or below-minimum enabled package is rejected; a newer package warns.
Disabling code through an INI does not remove its RomFS replacements. Resource
allocation support follows installed replacement packages, including disabled
ones.

For parser and overlay details, see [settings schemas](docs/settings-schemas.md).
For captions, shared input and resource ownership, see
[shared UI contracts](docs/shared-input-ui.md).

The suite version is defined in [release.hpp](source/program/core/release.hpp)
and each feature package's regular INI. Package minimum versions are declared in
[core_features.cpp](source/program/core/features/core_features.cpp). Generated
layouts come from the original game resources through the scripts in `tools/`.

## Verification

Build fresh outputs before running compiled ARM tests. The full runner executes
host policy suites and the maintained ARM/resource groups:

```sh
python3 tools/check.py
```

Select groups for a focused change:

```sh
python3 tools/check.py core pockets
python3 tools/check.py shopping fitting
```

Available groups are `core`, `shopping`, `dialogue`, `crafting`, `controls`,
`pockets`, `fitting` and `catalog`. Host policy suites run with every invocation.

Tests need a host **C++20 compiler**, Python with **Unicorn**, and the original
native code/data used by the fixtures. The current runner expects the surrounding
workspace layout, including `../Grab 5/working/main.raw`, Python dependencies in
`../Grab 5/working/python`, and extracted data under `../vanilla-3.0.3/`.
The repository's existing tests are not a self-contained clean-checkout setup.

To compare with an earlier preserved ELF, set `QOL_BASELINE_ELF`:

```sh
QOL_BASELINE_ELF=/path/to/baseline/project.elf python3 tools/check.py core
```

Baseline comparisons assume compatible source-side fixtures and configuration
versions. Keep the tested `project.elf` in place for the entire run; do not rebuild
while tests are using it.

Verify generated package contents and manifests with the matching revision:

```sh
python3 tests/package_test.py --revision 4
```

Offline tests execute compiled policies and native ARM paths with modeled engine
services. They complement Eden testing. See [VALIDATION.md](VALIDATION.md) for
recorded results and the module reading guides for specific limits.

## Diagnostics

Core's `log_versions`, `log_settings` and `log_debug` settings control normal
output. Warnings and errors are always logged. Detailed trace builds are available
for specific investigations:

| Build command | Purpose |
| --- | --- |
| `sh tools/build.sh --verbose-dialogue` | Bounded dialogue classification and speed traces. |
| `sh tools/build.sh --hurry-diagnostic` | Controls dialogue-advance diagnostics. |
| `sh tools/build.sh --auto-vault-diagnostic` | Automatic vault diagnostics. |

Build modes always clean first. Use `sh tools/build.sh` to return to the normal
build. Diagnostic packagers live in `tools/`; inspect their options before use.
Preserve the Eden log after reproducing an issue.

## License and reference material

See [LICENSE](LICENSE) and the included framework notices. Distribute matching
source with binaries and retain third-party notices. Generated game resources
and binaries are excluded from the source archive.

Native ownership and compatibility contracts are documented in
[native bindings](docs/native-bindings.md),
[feature activation](docs/feature-activation.md),
[pocket commands](docs/pocket-commands.md) and
[preview sessions](docs/preview-sessions.md).

Development history is retained in the
[archived README](docs/history/development-readme.md), the source notes in `docs/`,
and the investigation logs in `research/`.
