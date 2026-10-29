# HD minimap

A minimap in a corner of the TV picture, drawn with the HD game's own GamePad map art: island maps,
dungeon floor plans, treasure charts and the game's map markers. It follows the game's map rules, so
no GameCube copy is needed.

Based on **[Minimap HD](https://github.com/Chironeyes/The.Wind.Waker.HD-Minimap.HD)** by [@Chironeyes](https://github.com/Chironeyes) (MIT). That Cemu pack's documentation is the specification of
this mod, and its frame, wind arrow and shortcut icons are used here (see `NOTICE.md`). This mod is
its own implementation for the port; it does not use the pack's patches.

## What it shows

| Where | Minimap |
| --- | --- |
| Great Sea, discovered sector, near its island | The sector's island map on the see-through (lightly darkened) panel, with Link (or the boat) and the wind direction |
| Undiscovered sector | No map; a faded map with a question mark |
| Out at sea, away from the island | Nothing; the game's own compasses |
| Sailing, with an owned treasure chart | Optionally the chart beside the minimap: the chart art centred on the treasure's X, as the game's chart screen shows it, with its name in the game's language (Triforce charts once deciphered) |
| Dungeon with the Map | The plan of the controlled character's current floor with its doors; Link's room bright, unvisited rooms dimmed |
| Dungeon with the Compass | Unopened chests on this floor and the boss icon at the boss door (until the boss is beaten) |
| Earth / Wind Temple | The controlled character; the other character when on the same floor |
| Dungeon without the Map, other places | No minimap |
| Pause menu, menus, first person, aiming, conducting, cutscenes, loading | Hidden |

## Controls

Controller buttons (Wii U names; the port's Controls settings map them), all changeable in the mod's
options:

| Input | Action |
| --- | --- |
| On foot: D-pad left | Show or hide the current map family |
| On foot: D-pad right | Compact or expanded |
| Dungeon on foot: D-pad down | Toggle zoom and camera-relative orientation (configurable) |
| Dungeon aboard: D-pad down | Compact → expanded → hidden → compact |
| Dungeon aboard: L stick click | Toggle zoom/orientation while visible |
| Marine sailing: D-pad down | Compasses → minimap → minimap and chart |
| Marine chart view: L stick click | Next eligible owned chart |

Aboard, D-pad left/right remain the game's cannon/crane controls. Dungeon visibility,
size and zoom are shared across foot, swimming and boat travel. Island and dungeon
choices are separate; marine view and chart choices persist independently of foot
visibility and temporary coverage loss. Maps require local coverage on foot, while
swimming and aboard. Menus and cutscenes take priority.

The dungeon plan follows Link, or Medli/Makar while controlled through Command Melody,
with altitude-based floor updates and a small boundary margin to avoid flicker.
Panels enter/resize over 200 ms and leave over 250 ms. Floors slide vertically inside
a fixed frame; the wind arrow gently pulses on island maps.

The shortcut icons beside the HUD's D-pad move and fade with it. On foot, left
shows the map toggle; right shows expand/reduce while visible; down shows dungeon
zoom. Aboard, only the bottom map shortcut is custom. An icon appears only while
its D-pad button retains that action in the options. If the HUD's D-pad cannot be
read, the corner feedback icon is used.

## Settings

Position and both sizes, overall opacity, faded opacity, fade on movement (Link and camera input), black
background and its opacity, parchment behind island maps (off: the map is see-through), smooth island maps (on: the port enlarges
the game's 100 × 100 island art three times with Lanczos-3 and sharpening on your machine; off or on
an older port: the original art), wind arrow, dungeon maps, whether the dungeon zoom turns and with
what (the camera, the default, or Link), and the four buttons. Changes apply after a
restart, like every code mod option.

## Requirements and data

- A port with the game layout art service (`WWHD_HUD_GAME`, the `layout-pane:` / `layout-texture:`
  queries; port SDK guide "The game's layout art"). Older ports show no map.
- The map pictures are not in this package. The port reads them from your installed game while it
  runs, in your game's language. The mod only names the game's layouts, textures and panes.

## Known limits (0.2.0)

- The treasure ribbon uses native localized names, with a numeric fallback if the
  message cannot be read. Eligible GamePad selections are followed; synchronization
  back to the GamePad remains open after investigating its native selection path.
- The sailing map stays above the game's compasses. Native compass replacement and
  its return delay await the port lead's decision; see [SDK proposal](SDK-PROPOSAL.md).
- Link and boat markers are never drawn on treasure charts.
- Tower boat controls and boat-to-swimming state retention were checked. Command
  Melody, airborne companions and sea chart handoffs need further gameplay validation. The possible Tower room omission is not confirmed resolved.
- This version is a parity follow-up, not a claim of complete Minimap HD 1.3 equivalence.

## Checked in the game

Earlier 0.1.x testing covered USA/EU, Metal/Vulkan and multiple frame rates. That
coverage does not validate these new changes. The 0.2.0 campaign uses an isolated,
newly created USA quest, native stage warps and test-only Map/Compass flags. See
[parity report](PARITY.md) for exact scenes, evidence and remaining gaps.

## Tests

`python3 -m unittest discover -s hd-minimap/tests` compiles and runs the host tests of the rules in
`src/hd_minimap.h`. They cover sectors and island coverage, art and pane names, floors, fading,
buttons (including those left to the game aboard), the D-pad icons, sailing views, plan fitting and framing,
the Forsaken Fortress plan turn, its outside and searchlights, and the dungeon zoom's turn. No game data is included.

## Credits and licensing

Based on [Minimap HD 1.3](https://github.com/Chironeyes/The.Wind.Waker.HD-Minimap.HD)
by Chironeyes. Context rules, timings, controlled-character identity checks, floor
hysteresis and chart-message ranges were translated from its MIT-licensed source;
no assembly or game assets are shipped. Copyright (c) 2026 Chironeyes. The complete
MIT copyright and permission notice is retained in [NOTICE.md](NOTICE.md), including
attribution for the frame, wind arrow and shortcut artwork. This mod's code remains
MPL-2.0; adapted portions retain that MIT notice.

## Changes

- **0.2.0:** independent remembered contexts, dungeon boat controls, controlled-character
  focus, stabilized altitude floors, animated presentation and wind, Fortress outdoor
  chest capture, multi-floor room eligibility, native localized chart names and
  read-only GamePad chart following.

- **0.1.2:**
  - Aboard the boat, D-pad left and right stay the cannon and grappling hook (D-pad left no longer also
    hides the minimap); D-pad down shows and hides it. No map buttons on the boat in the Tower of the
    Gods. A button held through a cutscene or dialogue no longer acts when it ends.
  - Forsaken Fortress: Link, doors and chests now land on their rooms in the turned plan of the second
    and third visits, as on the GamePad. Doors facing east, west or a diagonal turn as on the GamePad in
    every dungeon.
  - Minimap HD's icons beside the HUD's D-pad (map, expand or reduce, the dungeon compass, the sailing
    view), moving and fading with it. They replace the brief icon in the minimap's corner.
  - The dungeon zoom turns with the camera by default, like the GamePad's zoomed map (option "Dungeon
    zoom turns with": camera or Link, as before). Turned with the camera, Link's arrow shows where he
    faces on the turned plan.
  - Forsaken Fortress: on foot outside the fortress on the later visits, the minimap shows the plan's
    outside with Link and its three searchlights, searching or stopped, as on the GamePad, instead of
    the island.
  - Dungeon plans are framed as on the GamePad, so each floor keeps its place (floors line up when
    changing floor) and doors, chests and the boss icon keep their size against the rooms.
- **0.1.1:** no longer experimental.
- **0.1.0:** first version.
