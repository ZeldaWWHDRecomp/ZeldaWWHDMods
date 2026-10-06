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

The patches are made for the port's **v0.2.2** release (tag `v0.2.2`, commit `488b1fd4`).
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
