# Haunted sea

Now and then, at night far out on the Great Sea, the wind dies away and fog rolls in. Thirty seconds later the sea
turns on Link: Gyorgs rise from under the water and ram the boat, Seahats and Kargarocs swoop in from behind him,
cannon boats bombard him from the fog and, at night, Poes drift over the water while the Ghost Ship sails in the
distance. It lasts two to four minutes.

- **Escape it** by sailing away from the signs while the wind is dying, or by reaching an island.
- It only comes once Link has the sail and the Wind Waker, never in a file's first four minutes, and not again for
  twelve minutes after one ends.
- Gyorgs come even before Link has the Boomerang.

All monsters are the game's own. Nothing from the game is included in this package.

## Options

| Option | Choices |
| --- | --- |
| How often | Rare, Now and then (default), Often, or Always (on at sea day and night, without the signs) |
| Monsters | Sometimes (default), Often, Relentless: how quickly they come and how many are out at once |

## How it works

Once per logic step, after the actors run, the mod rolls for a spell once a minute while Link sails or swims far
from any island at night. During the signs it lowers the wind (`dKyw_custom_windpower`) and blends in fog
(`dKy_fog_startendz_set`); during the spell it spawns monsters with `fopAcM_create` in Link's room and process layer,
out of his view, and keeps the Gyorgs and Seahats after him.

It hooks `fpcEx_Handler` (return), `daGy_c::_create` and `daGyCtrl_Create` (entry and return, to let the Boomerang
check pass for its own Gyorgs), `daPH_Execute` (entry and return: its own Seahats attack a swimming Link too), and
replaces `dComIfGs_checkGetItem` (passes the Boomerang check only while one of its own Gyorgs is being created).

## Status

Experimental. Ported from the built-in Haunted sea of
[wwhd-recomp-mods](https://github.com/AsadSi/wwhd-recomp-mods), where it was played for hours. The port to SDK v2 is
new: see the pull request for what has been tested.

Licence: MPL-2.0 (see LICENSE).
