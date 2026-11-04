# Dark Link

Sword hits charge a transformation with a dark charcoal body and equipment, red eyes,
a translucent black native Magic Armor aura and boosted sword damage. A red native MagicBar
appears below the normal magic gauge. Current version: **1.0.0**.

The body uses separate cool near-black ambient and charcoal diffuse colours,
with neutral albedo so native lighting can reveal the contours. Final body RGB
is scaled to 1.25% in the native blend stage to suppress shader contributions
that bypass that palette. Opaque/cutout routing, depth and alpha stay intact;
unsupported blend states fall back to black albedo. Eyes stay red.
This adds no specular lighting, extra light or shader replacement. The final
contrast still needs checking with the actual game shaders on device.

The black shell uses the midpoint of its native fade animation instead of
its fully visible end. Soft coverage and the independent texture animation stay
native; the fade curve determines the exact opacity (not a fixed 50% alpha).
This visual change still needs in-game confirmation.

## Controls and settings

- Press **L3 + R3** together, or tap **DARK**, to transform when the gauge is full.
  Press/tap again to cancel early and retain the remaining charge.
- Default capacity is 100; each damaging sword hit adds 5. Inactive charge does
  not drain. Blocked hits do not charge the gauge.
- Default transformation duration is 20 seconds, or 40 seconds with the larger
  magic meter. Both sizes take the same number of hits to fill.
- Sword damage defaults to 200%. Capacity, hit gain, duration, damage, automatic
  transformation, free transformation, diagnostics and HUD visibility are configurable.
- Tap **Edit** to move and style the touch button. With Enhanced Controls 0.4.0+
  installed, both mods share one editor for all five buttons. Each also works alone.

Manual cancellation preserves the remaining percentage. Natural expiry, death,
stage/owner changes and disabling the mod still clear its state. Menus and
cutscenes pause transformation time. Native artwork is read from the player's
installed game at runtime; no extracted game assets are distributed.

## Installation

After catalogue publication, install **Dark Link** from **Mods → Community**.
For local testing, install the package generated below through the Mods menu.
Complete code-mod setup, enable the mod and restart when requested.
The mod keeps its existing ID, option names and saved layout filenames.

## Build and catalogue validation

Use a clean SDK checkout at the commit in the repository's `sdk.json`.
The catalogue builds the single entrypoint `src/mod.c`; source files, licence
and original artwork are included by the existing repository packager.

```sh
export WWHD_SDK=/absolute/path/to/pinned/ZeldaWWHDRecomp
python3 dark-link/tests/build_and_test.py
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

The state, native-hook and HUD tests cover charge retention, both gauge sizes,
native appearance restoration and the shared touch editor. The USA synthetic
runtime executes the translated guest code; USA and EU both undergo translation
and native compilation. Real-game rendering and platform testing remain separate.

For the optional combined editor test, build Enhanced Controls in another checkout:

```sh
python3 dark-link/tests/test_shared.py --enhanced-root /path/to/controls-checkout
```

## Provenance and limitations

See [MIGRATION.md](MIGRATION.md) for the exact source commit and [HISTORY.md](HISTORY.md)
for the detailed fixes and previous on-device findings. The current native gauge
appearance was reported working before migration; migration itself has not been
retested in-game. Code and contributed artwork use [MPL-2.0](LICENSE); artwork
provenance is listed in [assets/ASSETS.md](assets/ASSETS.md).
