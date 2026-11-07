# Phantom stalker

Now and then, at night on an island of the Great Sea, fog closes in and Phantom Ganon comes down out of the dark
behind Link. He hunts Link around the island for two minutes with his energy balls and dark shots, in his Ganon's
Tower form.

- **Escape him** by getting back in the boat, or wait him out until the fog lifts. He can't be beaten: the sword
  only knocks him down for a while.
- Windfall and Outset stay safe.
- It only comes once Link has the sail and the Wind Waker, never in a file's first four minutes, and not again for
  twelve minutes after one ends. There are no warning signs.

Phantom Ganon and his effects are the game's own. Nothing from the game is included in this package.

## Options

| Option | Choices |
| --- | --- |
| How often | Rare, Now and then (default), Often, or Always (on every island but the towns, day and night) |

## How it works

Once per logic step, after the actors run, the mod rolls once a minute while Link is ashore on an island at night.
During a spell it blends in fog (`dKy_fog_startendz_set`), makes one Phantom Ganon (`daFganon`, the kind that waits high
up and comes down when Link is near) with `fopAcM_create` in Link's room and process layer, 1500-2000 units behind him,
and each step keeps his home point on Link, his room Link's and his size 1. When he waits more than 8 seconds for a
missed energy ball to die, the ball is ended as if it had flown too far. The spell ends after 120 seconds, when Link
rides the boat or leaves the island, or when the stage removes Phantom Ganon (as when it restarts after Link is
knocked into the sea); then he is deleted and the fog lifts. No new one
comes in a spell's last 20 seconds. An end waits while an event runs, while he is in a cutscene of his own, and while
he is in his copy attack (its copies find him through a pointer the game never clears), so deleting him never leaves
the game stuck or a copy behind.

Reflecting his energy ball sets an event flag in the save (0x3F20) that turns off one of the game's hints. The mod puts
it back as it was each step while he is here, so no save keeps it.

His effects live in Ganon's Tower's scene particle bank, Pscene080, which the sea does not load. The mod loads it into
the emitter manager's free slot 2 the way the game loads a stage's bank (`getArchiveFile`, a solid heap from the game
heap, a `JPAResourceManager` over the bank) and frees it when the stage goes (`dPa_control_c::removeScene`). The log
gives the bank heap's size.

Hooks and replacements:

- `fpcEx_Handler` (return): the step.
- `dPa_control_c::removeScene` (entry): frees the bank with the stage.
- `JPAEmitterManager::createSimpleEmitterID` (0x02821448, **replaced**): a scene effect the stage's own bank lacks is
  made from slot 2 when the mod's bank has it; every other call goes to the game's function unchanged.
- `JPASetRMtxTVecfromMtx` (0x028249B0, **replaced**), the helper every inlined `JPABaseEmitter::setGlobalRTMatrix`
  calls, so it covers all particle matrix updates: skips a missing emitter (a pointer below 0x1000). Phantom Ganon's
  energy ball sets the matrix of its hit effect's emitters without checking them, which halts the game when they could
  not be made. Every other call goes to the game's function unchanged.

Only one mod can replace a function, so this mod can't be on together with another mod that replaces either of these
two.

## Status

Experimental. Ported from the Phantom stalker of [wwhd-recomp-mods](https://github.com/AsadSi/wwhd-recomp-mods), with
Phantom Ganon as the stalker (the Shadow Beast and Darknut looks there need their own renderer and are not part of
this port). See the pull request for what has been tested.

Licence: MPL-2.0 (see LICENSE).
