# Wind Waker HD native port: mods

Game mods for the native Wind Waker HD port
([ZeldaWWHDRecomp](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp)).
This repository is private for now.

## No game files

This repository contains source code, integration patches and documentation only. It has
no game files, no assets extracted from the game, no keys and no saves. A mod that needs
game data builds it locally from the player's own copy of the game, outside any Git
checkout, and that data is never committed or shared.

## Mods

| Mod | Folder | Switch | What it does |
| --- | --- | --- | --- |
| Valoo dragon ride | [`dragon/`](dragon/) | `WWHD_MOD_DRAGON=1` | Learn a new Wind Waker song, Call of the Sky, in an optional quest on Dragon Roost; play it on the Great Sea and Valoo picks Link up with the Grappling Hook and flies him across the sea. |
| GameCube minimap | [`gc-minimap/`](gc-minimap/) | `WWHD_MOD_GC_MINIMAP=1` | A GameCube-style island map in the lower left corner on the Great Sea, with Link's position and heading. |

Both mods are off unless their switch is set to exactly `1`. Switched off, they leave the
game unchanged: every hook calls the original game code and nothing is drawn.

## Target port version

The patches are made for the port's **v0.2.2** release (tag `v0.2.2`, commit `4bb65149`).
Other versions may need the patches to be adapted.

## Built-in modules, not mod-manager packages (yet)

These mods are compiled into the port as built-in runtime modules: the player builds the
port from source with the mod added. They are not packages for the port's mod manager
yet. Their on-screen panels are drawn by the Metal renderer, so the HUD parts work on
macOS with the Metal renderer only; the Vulkan renderer runs the mods without their
panels.

## Applying a mod

Each mod folder has the same layout:

- `src/`: the mod's sources; they go into the port's `runtime/src/mods/` (the port's CMake
  build picks up every `.cpp` and `.mm` file there, no CMake change is needed);
- `patches/port-v0.2.2.patch`: the small edits to the port's own files that call into the mod;
- `hooks/` (if present): extra recompiler hook lists; they go into the port's
  `tools/recomp/`, and the game code must then be translated again;
- `tools/` (if present): helper tools for players and developers;
- `tests/` (if present): unit tests.

In general, from a clean checkout of the port at `v0.2.2` that builds on its own (see the
port's README for the build):

```sh
PORT=path/to/ZeldaWWHDRecomp        # git checkout v0.2.2
MOD=path/to/ZeldaWWHDMods/<mod>

cp "$MOD"/src/* "$PORT"/runtime/src/mods/
cp "$MOD"/hooks/*.txt "$PORT"/tools/recomp/ 2>/dev/null   # only mods with a hooks/ folder
git -C "$PORT" apply "$MOD"/patches/port-v0.2.2.patch

# only when the mod has a hooks/ folder: translate the game code again with the new hooks
python3 "$PORT"/tools/recomp/recomp.py "$PORT"/game/code/cking.rpx "$PORT"/build/gen

cmake --build "$PORT"/build/cmake
```

Then start the game with the mod's switch, for example
`WWHD_MOD_DRAGON=1 ./build/cmake/wwhd`. Each mod's README has the details.

Both mods can be applied to the same checkout, in either order: their patches do not
overlap.

## Guest catalogue packages

`heart-ticker/` is a guest SDK package. `sdk.json` pins the public port commit
used for its declarations and builder compatibility. The existing dragon and
minimap folders above remain legacy integration sources; they are not packaged
by this workflow.

Build with PowerPC-capable clang and lld against a clean checkout of the pinned
SDK (the checkout must match `sdk.json` exactly):

```sh
python3 tools/test_guard.py path/to/pinned-port
python3 tools/package.py --sdk path/to/pinned-port --out build/packages
```

The output contains a ZIP and a local `index.json`, with exact size and SHA-256.
Set `WWHD_MOD_CATALOGUE` to that generated index's absolute path to test it in
the port. The root `index.json` is a snapshot for local development; regenerate
its checksums after changing the package or compiler. Its package URL points
into `build/packages`, and no binary payload is committed. For hosted downloads,
pass `--base-url https://your-host/path`; hosting and publication are separate
maintainer steps. There is no public release or automatic deployment.

CI builds one PowerPC ELF, packages its manifest/source/licence, checks the source
and ZIP payloads with the pinned port release guard plus mod-specific checks,
and uploads only packages and a generated local index as a workflow artifact.
The artifact can be extracted and used offline. HTTPS URLs are required for a
remote catalogue. Guest translation uses the installed game's USA/EU address
mapping and native trust; Android guest mods remain unsupported.

The package writer preserves original artwork in `assets/` and `textures/`, and
authored content in `content/` (or the manifest's relative `content_dir`). These
use the existing `kind: guest` manifest and catalogue schema version 1; no new
catalogue delivery kind is needed. The SDK phase 2 runtime validates PNGs and
uses one trust fingerprint for the whole combined package, including its ELF,
manifest, artwork and content. Code mods off means neither part applies, and
content activates only after the guest module loads on restart. The current
pinned SDK and heart-ticker pilot do not require phase 2; a future combined
catalogue entry must declare a minimum port version that includes those services.

Artwork must be the modder's own work. The guards scan artwork and content too;
an `assets/` path or a `.png` suffix never exempts game-resource magic or forbidden
payloads. Game-derived maps belong only in the player's per-mod Data folder,
generated locally by `run_tool`, and never in a catalogue package or artifact.

Future minimap setup will select a shared GameCube source (`game_path`), run a
package-local map preparation tool (`run_tool`) into the mod's data directory,
and translate its guest ELF (`build_guest_mod`). Dragon needs guest translation
and options; both need the phase 2 HUD service before catalogue delivery. Game
assets and generated map caches stay on the player's computer.
