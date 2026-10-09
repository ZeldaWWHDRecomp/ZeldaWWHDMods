# Valoo dragon ride

An optional mod for the native Wind Waker HD port: Link learns a new Wind Waker song,
**Call of the Sky**, in a short side quest on Dragon Roost. Played on the Great Sea, the
song calls Valoo, who flies in, catches Link with the Grappling Hook and carries him
across the sea on a rope. The player steers the flight and lets go to glide down with the
Deku Leaf.

Prototype status: it plays through, but the visuals and the flight limits still need
polish (see [Limits](#limits)).

## What it does

- **Valoo's Gratitude** (side quest). After the Dragon Roost boss and once Link owns the
  Deku Leaf, a letter from Medli arrives (shown in an on-screen panel). Medli waits at the
  high entrance of Dragon Roost. She asks Link to restore three wind chimes around the
  island (harbor, upper cliff and outer lookout); striking each with the sword reveals two
  notes of the melody. Back with Medli, conducting the melody teaches the song.
- **Call of the Sky**. Six beats: **↑ → ↑ ← ↓ →**. Link conducts with the normal baton
  animation and glow, a wind gust follows the baton, and a short melody plays (synthesized
  by the mod, no game audio). It also works from the boat; boat control comes back after
  the performance. The song works anywhere on the Great Sea once learned.
- **The ride**. Valoo (his flying model and animation from the game's own files) flies in
  over Link. Link throws the Grappling Hook automatically, the hook catches Valoo's claw
  and Valoo lifts him to about 4000 units above the sea. Then the player steers. A camera
  behind Valoo follows the flight. Valoo's flight sound, a wind layer and the rope sounds
  are the game's own sound effects.
- **Flight panel**. A text panel in the lower left corner of the TV image shows the phase,
  the controls, the altitude and the flight range, and during the quest the next step.
- **Letting go**. A releases the rope; Link falls normally and the Deku Leaf opens with
  its usual button and magic cost. Valoo flies off and disappears.

No game files are added: Valoo, the hook, the rope, Medli, the chimes and all sounds come
from the game's own data at run time; the panel uses system fonts.

## Turning it on

Start the game with `WWHD_MOD_DRAGON=1` (exactly `1`; unset, `0` or anything else is off):

```sh
WWHD_MOD_DRAGON=1 ./build/cmake/wwhd
```

The log shows `[dragon] mod enabled (WWHD_MOD_DRAGON=1)` once the mod is active. Switched
off, every hook calls the original game function directly: no resources are loaded, no
game memory is written and nothing is drawn.

## Controls

| When | Input | Action |
| --- | --- | --- |
| Quest invitation on the sea | D-pad Left | read Medli's letter |
| Near Medli (Dragon Roost, high entrance) | A / B | talk, accept / close |
| At a wind chime | sword attack, facing the chime | restore it, reveal two notes |
| Great Sea, song learned | open the Wind Waker (e.g. L + D-pad Up), hold the left stick right for six beats, play ↑ → ↑ ← ↓ → with the right stick | Call of the Sky |
| While conducting | B | cancel |
| Grappling and lift | A | cancel, Link falls |
| Flight | left stick left / right | turn |
| Flight | left stick up / down | climb / dive |
| Flight | A | let go (then the Deku Leaf button glides) |

Flight speed is 140 units per 30 Hz step. The flight range is from about 800 units of
clearance under Link (more over islands) up to 16000 units above the sea.

## Quest progress

The quest progress is kept per save slot in small text files next to the game's save,
`dragon-quest-slot0.txt` to `dragon-quest-slot2.txt` (phase and the chimes found). The
game's own save, its checksums and its story flags are never changed. The in-game
copy-save menu does not copy these files: when moving a save to another slot, copy its
file along. Starting a new game in a slot resets that slot's progress.

## Building it into the port

For port v0.2.2 (tag `v0.2.2`). From the root of a port checkout that already builds, with
`MOD` pointing at this folder:

```sh
cp "$MOD"/src/* runtime/src/mods/
cp "$MOD"/hooks/hooks_dragon.txt tools/recomp/
git apply "$MOD"/patches/port-v0.2.2.patch
python3 tools/recomp/recomp.py game/code/cking.rpx build/gen   # translate again with the new hooks
cmake --build build/cmake
```

The recompiler step is required: `hooks_dragon.txt` adds game functions that the mod wraps,
and the build fails to link (`f_..._orig` missing) until the game code is translated again.

### What the patch changes in the port

- `runtime/src/mods/climb.cpp`: Link's execute (`0240CDD0`) is already hooked by the port's
  climb mod; its hook now calls `dragon::execute` around the original.
- `runtime/src/true60.cpp`: the follow-camera hook (`025028B8`) calls `dragon::camera`
  around the original (the dragon view during the ride).
- `runtime/src/audio_out.cpp`: mixes the synthesized song into the output while it plays.
- `runtime/src/gfx/metal_main.mm`: draws the panel into the TV image after the scan copy.

### Sources

| File | Contents |
| --- | --- |
| `src/dragon.cpp`, `dragon.h` | the ride: song recognition, Valoo, grapple, lift, flight, camera, release |
| `src/dragon_quest.cpp`, `dragon_quest.h` | Valoo's Gratitude: letter, Medli, chimes, progress files |
| `src/dragon_song.cpp`, `dragon_song.h` | the synthesized Call of the Sky melody |
| `src/dragon_hud.mm` | the Metal text panel |
| `src/dragon_hooks.cpp` | the `hook_<address>` wrappers for the addresses in `hooks/hooks_dragon.txt` |
| `hooks/hooks_dragon.txt` | the recompiler hook list |

### Game functions used (USA executable)

| Address | Use |
| --- | --- |
| `0240CDD0` | Link execute (chained in the climb hook): input, quest, ride state |
| `025028B8` | follow camera (in true60.cpp): dragon view |
| `025E1F34` | Wind Waker beat judge: recognizes the six notes |
| `023E92A0` | Wind Waker action (forwarded) |
| `0212A830` / `0212A00C` / `0212A6E4` | Valoo actor create / execute / delete, only for the mod's own Valoo |
| `025D63E8`, `0212A728` | Valoo heap size and the hook model in Valoo's heap |
| `02129D28` | Valoo draw: hook and rope |
| `023FDA70` | Link's movement while carried |
| `02520460`, `025204C8`, `026066C4` | resource load / delete / lookup, redirected only for the mod's Valoo (flying model and animation from the Demo45 archive) |
| `025BA7B0`, `025BAC50` | save slot load / new game: per-slot quest progress |
| `0234C0B0`, `0234C23C`, `0234C1C4` | the extra wind chimes (create / execute / delete) |
| `02286084`, `02288B08`, `0228A254` | the extra Medli (create / execute / delete) |
| `025B7A2C` | a collection query, scoped to the extra Medli's creation |

The mod finds Valoo's actor profile in the game's profile table by its create function
rather than by a fixed ID, and tags its own actors so that the real story actors are never
touched.

## Limits

- Prototype visuals: Valoo uses an existing cutscene flight animation; Link uses the
  existing rope-hanging pose, and the hand/rope contact is approximate.
- Collision: the flight checks the game's background collision along a few sample lines
  around Valoo and Link (with a short look-ahead) and stops forward movement on a hit.
  It is not a full collision volume; thin or decorative geometry can be missed.
- The flight area is clamped to the Great Sea grid and to 16000 units above the sea. Only
  some routes across the sea have been flown; streaming of every island from every
  approach is not verified.
- The panel, letter and dialogue are mod panels, not the game's own message boxes.
- The panel is Metal only. With the Vulkan renderer the mod runs without it.
- The song is a mod song: it is not added to the game's song list.
- The mod depends on the addresses and memory layout of the USA Wii U executable and the
  port's runtime API; other versions or regions need the addresses checked.
