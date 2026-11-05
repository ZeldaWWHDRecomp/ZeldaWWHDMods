# Source provenance

Ported from [BeZide93/ZeldaWWHDRecomp](https://github.com/BeZide93/ZeldaWWHDRecomp/tree/634e3ba7dbb6ee2929bc4db59887c8f05cabcab5/mods/enemy-hp-bars), commit `634e3ba7dbb6ee2929bc4db59887c8f05cabcab5`, version 0.1.1.

The mod ID and options are preserved. This mod has no persistent data files. The catalogue compiles `src/mod.c` with the SDK pinned by the repository. Existing gameplay regression tests are retained under `tests/`. No runtime patch or binary from the recomp repository is included.

## Validation scope

The migrated entrypoint compiles against the unchanged SDK pin `fab80fcec42a25000a97137e2ee4eb2042b858c7`. USA and EU translation/native compilation and the retained synthetic regression checks were run locally. No real game files or device session were used for this migration. Existing gameplay limitations remain; package validation is not an in-game acceptance test.

## Build and catalogue validation

Use a clean SDK checkout at the commit in the repository's `sdk.json`.
The catalogue builds the single entrypoint `src/mod.c`; source files and the licence
are included by the existing repository packager.

```sh
export WWHD_SDK=/absolute/path/to/pinned/ZeldaWWHDRecomp
python3 enemy-hp-bars/tests/build_and_test.py
```

The local regression runner requires Python 3, PowerPC-enabled Clang/LLD and a
native Clang compiler. Generated files stay under the repository's ignored
`build/` directory. It does not require game files. For catalogue publication,
commit the sources first and run the repository's documented `tools/guard.py`,
`tools/package.py` and `tools/validate.py` against the pinned SDK. The local
runner's package is a test artifact, not the published catalogue download.

The maintained catalogue generates the download URL, size and checksum.
Contributors edit `catalogue.json`; do not hand-edit `index.json` or commit ELF/ZIP
files. Publication is separate from opening or merging a source PR.

## Changes in 0.1.1

Fix the invisible overlay: the host publishes its new logic counter after the
enemy hooks run and before drawing the HUD. Version 0.1.0 incorrectly required
the enemy snapshot and HUD counters to be identical, rejecting every normal
completed pass. Existing settings are preserved when updating the package.

Native method/profile IDs come from the repository's `story_enemies.cpp`,
`story_enemy_ownership.cpp` and `story_enemies_more.h`. Layouts and region
relocations use the existing SDK. Source license: MPL-2.0, as the repository.
