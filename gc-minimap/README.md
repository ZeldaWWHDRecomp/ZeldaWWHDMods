# GameCube minimap

An optional mod for the native Wind Waker HD port: a GameCube-style island map in the
lower left corner of the TV image while Link is on the Great Sea, with a pale frame,
outlined edge arrows and Link's yellow and red heading marker.

Prototype status: the Great Sea only (no dungeons or interiors), drawn by the Metal
renderer on macOS.

## What it does

- The map shows the island of the sea sector Link is in, follows his position and turns
  the marker with his heading.
- It is shown on the Great Sea stage (`sea`, and its `sea_T` / `sea_E` variants) and hidden
  during events and indoors.
- Islands with a detailed map: sea sectors 1, 4, 11, 13, 20, 23, 40, 41, 44 and 45 (the
  sectors whose room data has map bounds).
- The map art comes from a local cache built from the player's own files, in this order:
  1. smoothed vector traces of the original GameCube maps (needs the player's GameCube disc);
  2. the original GameCube maps as they are (same source);
  3. the HD game's own sea chart layers, recolored in the GameCube style (needs only the
     Wii U game).

The mod only reads Link's position and the stage name; it never calls game functions or
writes game memory. Switched off, it does not read game state, load textures or draw.

## Turning it on

```sh
WWHD_MOD_GC_MINIMAP=1 ./build/cmake/wwhd
```

The switch must be exactly `1`. The log shows `[gc-minimap] mod enabled
(WWHD_MOD_GC_MINIMAP=1)` once the mod is active. Without a map cache (below) the mod runs
but draws nothing.

`WWHD_GC_MINIMAP_CACHE=<folder>` selects the cache folder; the default is
`~/Library/Application Support/wwhd/gc-minimap`.

## Building the map cache

The map cache is made from the player's own game files and stays on the player's machine.
It must not be committed or shared: it contains data from the game. The tool refuses to
write into a Git checkout or into the game folder.

```sh
python3 tools/gc-minimap-assets.py \
  --port path/to/ZeldaWWHDRecomp \
  --game path/to/ZeldaWWHDRecomp/game \
  --output ~/Library/Application\ Support/wwhd/gc-minimap
```

- `--port`: a checkout of the port; the tool uses its `tools/shaderprep.py` to read the
  game's archives (it compiles the port's small Yaz0 helper on first use).
- `--game`: the extracted Wii U game (the folder with `code/` and `content/`).
- `--output`: the cache folder.

This writes the HD chart layers of all 49 sea sectors and the map bounds of the sectors
that have them.

Optionally, for the original GameCube maps:

```sh
  --gc path/to/your/GameCube/disc.iso --dtk path/to/dtk
```

- `--gc`: the player's own GameCube disc image of the game (ISO or RVZ).
- `--dtk`: the [decomp-toolkit](https://github.com/encounter/decomp-toolkit) executable,
  used to read the map textures from the disc.

The tool then decodes each sector's original map (`gc_minimap_bti.py`) and traces it into
smoothed vector outlines (`gc-minimap-vector.py`, needs Pillow and numpy). The tracer
checks that every original pixel keeps its color before it writes anything; it smooths the
outlines but adds no detail.

## Building it into the port

For port v0.2.2 (tag `v0.2.2`). From the root of a port checkout that already builds, with
`MOD` pointing at this folder:

```sh
cp "$MOD"/src/* runtime/src/mods/
git apply "$MOD"/patches/port-v0.2.2.patch
cmake --build build/cmake
```

No new recompiler hooks are needed.

### What the patch changes in the port

- `runtime/src/true60.cpp`: in the per-process execute hook (`025DF940`), after Link's
  execute, the mod takes a read-only snapshot of his position, heading and sector.
- `runtime/src/gfx/metal_main.mm`: draws the map into the TV image after the scan copy.
  The GamePad image is unchanged.

### Sources

| File | Contents |
| --- | --- |
| `src/gc_minimap.cpp`, `gc_minimap.h` | the switch and the snapshot of Link (shared between game and render thread under a lock) |
| `src/gc_minimap_math.h` | sea sector from a position, position on the map |
| `src/gc_minimap_hud.mm` | the Metal map panel and the cache loader (HD chart layers are detiled with the port's Latte address library) |
| `tools/gc-minimap-assets.py` | builds the local map cache |
| `tools/gc_minimap_bti.py` | decodes GameCube CI4/CI8 map textures |
| `tools/gc-minimap-vector.py` | traces a GameCube map into vector outlines |
| `tests/` | unit tests |

## Tests

```sh
c++ -std=c++17 -I src tests/gc-minimap-math.cpp -o /tmp/gc-minimap-math && /tmp/gc-minimap-math
python3 -m unittest discover -s tests -p 'test_*.py'    # the vector test needs numpy and Pillow
```

## Limits

- The Great Sea only; dungeon and interior maps are not done.
- No boat marker yet, and the map does not follow every HUD rule of the game (pause
  screens). The edge arrows are drawn like the GameCube frame but do nothing.
- The map panel is Metal only. With the Vulkan renderer the mod runs without it.
- The Wii U executable's own minimap drawing is mostly empty in the HD game, so the mod
  draws its own panel instead of restoring the original one.
- Sector 28 has an original map texture but no map bounds in its room data; the mod hides
  the map there rather than guessing coordinates.
- With the dragon mod also on, its flight panel and this map share the lower left corner.
