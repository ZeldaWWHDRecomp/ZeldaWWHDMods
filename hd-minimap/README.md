# HD minimap

A minimap in a corner of the TV picture, drawn with the HD game's own GamePad map art: island maps,
dungeon floor plans, treasure charts and the game's map markers. It follows the game's map rules, so
no GameCube copy is needed.

Based on **Minimap HD** by Chironeyes (MIT). That Cemu pack's documentation is the specification of
this mod, and its frame, wind arrow and shortcut icons are used here (see `NOTICE.md`). This mod is
its own implementation for the port; it does not use the pack's patches.

## What it shows

| Where | Minimap |
| --- | --- |
| Great Sea, discovered sector, near its island | The sector's island map on the see-through (lightly darkened) panel, with Link (or the boat) and the wind direction |
| Undiscovered sector | No map; a faded map with a question mark |
| Out at sea, away from the island | Nothing; the game's own compasses |
| Sailing, with an owned treasure chart | Optionally the chart beside the minimap: the chart art centred on the treasure's X, as the game's chart screen shows it, with its number (Triforce charts once deciphered) |
| Dungeon with the Map | The plan of Link's current floor with its doors; Link's room bright, unvisited rooms dimmed |
| Dungeon with the Compass | Unopened chests on this floor and the boss icon at the boss door (until the boss is beaten) |
| Earth / Wind Temple | Medli or Makar when they are on Link's floor |
| Dungeon without the Map, other places | No minimap |
| Pause menu, menus, first person, aiming, conducting, cutscenes, loading | Hidden |

## Controls

Controller buttons (Wii U names; the port's Controls settings map them), all changeable in the mod's
options:

| Input | Action |
| --- | --- |
| D-pad left | Show or hide the minimap (on foot) |
| D-pad right | Compact or expanded (on foot) |
| D-pad down | In a dungeon: zoom on Link, turned with Link like the GamePad's zoomed map (option). Sailing: compasses only → minimap → minimap and chart |
| L stick click | Sailing chart view: next owned chart |

Aboard the boat, D-pad left and right stay the boat's own (cannon and grappling hook) and D-pad down
shows and hides the minimap: boarding with the minimap hidden starts with the compasses alone. On the
boat in the Tower of the Gods, every button is left to the game. A button held through a cutscene or
dialogue does nothing when it ends.

## Settings

Position and both sizes, overall opacity, faded opacity, fade on movement (Link and camera input), black
background and its opacity, parchment behind island maps (off: the map is see-through), smooth island maps (on: the port enlarges
the game's 100 × 100 island art three times with Lanczos-3 and sharpening on your machine; off or on
an older port: the original art), wind arrow, dungeon maps, and the four buttons. Changes apply after a
restart, like every code mod option.

## Requirements and data

- A port with the game layout art service (`WWHD_HUD_GAME`, the `layout-pane:` / `layout-texture:`
  queries; port SDK guide "The game's layout art"). Older ports show no map.
- The map pictures are not in this package. The port reads them from your installed game while it
  runs, in your game's language. The mod only names the game's layouts, textures and panes.

## Known limits (0.1.2)

- The treasure chart's ribbon shows the chart's number, not its name (the names are game text).
- Link's position is not drawn on the treasure chart (as in Minimap HD).

## Checked in the game

USA and European game builds; Metal and Vulkan; 30 fps, interpolated 60 fps and true 60 fps. Places checked:
Outset; sailing aboard the boat with treasure charts; Forsaken Fortress; Dragon Roost Cavern; Forbidden
Woods; Tower of the Gods; Earth Temple; Wind Temple (with and without the Map and Compass).

## Tests

`python3 -m unittest discover -s hd-minimap/tests` compiles and runs the host tests of the rules in
`src/hd_minimap.h`. They cover sectors and island coverage, art and pane names, floors, fading,
buttons (including those left to the game aboard), sailing views and plan fitting. No game data is
included.

## Changes

- **0.1.2:**
  - Aboard the boat, D-pad left and right stay the cannon and grappling hook (D-pad left no longer also
    hides the minimap); D-pad down shows and hides it. No map buttons on the boat in the Tower of the
    Gods. A button held through a cutscene or dialogue no longer acts when it ends.
- **0.1.1:** no longer experimental.
- **0.1.0:** first version.
