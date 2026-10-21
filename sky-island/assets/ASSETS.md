# Assets

Every file in this folder is original artwork made for this package. None of it is taken, traced or
converted from the game.

| File | What it is | Author | Licence |
| --- | --- | --- | --- |
| `island.glb` | the floating island (the stage's room 0): grass top and terraces, cliffs and rocky underside, waterfalls, floating rocks, and the room's collision | the ZeldaWWHDRecomp project lead | MPL-2.0 |
| `ruins.glb` | the island's ruins, the round plaza with its emblem disc, stepping stones and the bridge to the arena (room 2, drawn with room 0) | the ZeldaWWHDRecomp project lead | MPL-2.0 |
| `island-distant.glb` | the island far off in Outset's sky: a low-detail copy of the island's parts, each with a small plain-colour picture of its own | the ZeldaWWHDRecomp project lead | MPL-2.0 |
| `arena.glb` | the walled boss arena (room 1) | ZeldaWWHDRecomp contributors | MPL-2.0 |

The island is original art of the project lead: a concept design of a floating ruin island and a 3D model made
from it, built for the game (reduced to a game budget, scaled to about 46 m across, its top, underside, ruins and
waterfalls as separate parts with collision of their own). Its shading follows the source model's colours; the
source model itself is not part of this package.

The models carry texture coordinates and vertex colours but no textures: the port draws the stage with materials
of rooms of the player's own game files when it builds the stage (Forest Haven's grass, cliffs, vines, falling water,
foam and spray for the island; Hyrule Castle's stone and its gold crest for the ruins and the plaza; the Tower of the
Gods' paving for the arena). The island's trees are the game's own trees, placed as actors. No game models,
textures, collision files or archives are included.
