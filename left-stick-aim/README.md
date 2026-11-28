# Left Stick Aim

Aim in first person with the left stick, like on GameCube. For controllers without a gyro, or
for players who prefer not to use it.

- Active in first-person views: looking around, telescope, bow, boomerang, hookshot and
  grappling hook, on foot and on the boat. The right stick still aims too.
- Click the left stick (L3) to switch between this and the normal controls. The mod starts
  switched on after each restart.

## Limitations

- While the mod is on, Link can't walk with the left stick in those views.
- The game never receives L3 while the mod is enabled.
- The ship's cannon and crane are unchanged.
- Don't combine it with another L3 stick swap (for example a Steam Input layer).

## Technical

Entry hooks on `daPy_Execute` (registers the filter), the subject camera and
Link's first-person procedures, on foot and on the boat. One input filter
(`wwhd_input_filter_register`) moves the left stick to the right stick while one of those hooks
ran in the last two logic steps. No replacements, files, HUD or setup tools.

Source: `src/mod.c`. Licence: MPL-2.0 (see LICENSE).
