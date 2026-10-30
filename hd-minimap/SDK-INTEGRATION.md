# Port compatibility (0.2.1)

Version 0.2.1 uses two optional port SDK services for map mods: the boat-compass
lease and native chart selection (`wwhd_hud_boat_compasses`,
`wwhd_hud_boat_compasses_rect`, `wwhd_map_chart_selected`, `wwhd_map_chart_select`,
`wwhd_map_chart_cancel`). They are in the port SDK from devel `fab80fce` and the next
port release; see the port's `docs/mod-sdk-map-hud.md`.

## Newer ports (with the map/HUD services)

- **Boat compasses.** While sailing with the minimap or a chart shown, the map takes
  the place of the native boat compasses instead of sitting above them. It is anchored
  to the compass area the port reports, with your left/bottom margin options applied.
  The map leaves over 250 ms and the compasses return 200 ms after that; reversing a
  transition keeps its current position. Menus, cutscenes, first person and the boat
  tools stay under the game's control.
- **Treasure charts.** Choosing a chart with L stick click also selects it on the
  GamePad; the game's chart carousel animates the change. Choosing a chart on the
  GamePad still updates the minimap. Leaving chart view, a HUD reset or an invalid
  menu cancels a choice the game has not yet taken.
- If another mod already holds the compass area, or the game's chart menu is not
  available, the request is refused and the older-port behaviour below is used.

## Older ports

- The map stays above the native compasses.
- Charts follow the GamePad in one direction only: the minimap follows the
  GamePad's selected chart, but L stick click does not change the GamePad.
- The package still loads: the services are detected at run time through the
  existing `wwhd_setting_get` call (`hud.boat-compasses.version`,
  `map.chart-selection.version`), so no new host function is required.

On both, the mod never writes the game's HUD or menu memory. The port owns the
native compass and chart-carousel lifecycle and brings the compasses back itself
when the lease ends (missed renewal, mod disabled, state load, HUD or stage change).

## Also in 0.2.1

- Animations advance in game logic rather than at draw time, so their timing is
  the same at 30, 60 and 120 FPS.
- The faded question-mark map for an undiscovered sea square near an island now
  goes through the normal panel flow, like the island map: it uses the compact or
  expanded size, respects show/hide and the boat's sailing view (it is no longer
  shown while the map is hidden or the boat view is set to compasses), slides in and
  out, and takes the compass area like any other sailing map.

## Checked

Host tests (`python3 -m unittest discover -s hd-minimap/tests`) cover chart
request acknowledgement, conflicts and cancellation, visibility, coverage, separate
chart exits and the single compass release after 250 ms at 30/60/120 Hz. The module
builds for USA and EU. In the game (USA, Metal), on a port with the services: the map
in the compass corner, L stick click and GamePad chart changes in both directions,
exit reversal without a compass flash, and the compasses returning after the delay.
On an older port: the above-compasses fallback. EU gameplay, Command Melody and
companion flight were not replayed; see [PARITY.md](PARITY.md) for the other limits.
