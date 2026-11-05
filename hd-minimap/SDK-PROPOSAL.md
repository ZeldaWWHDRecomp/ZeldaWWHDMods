# Supported map integration proposal

Status: historical proposal, now implemented in the port SDK (devel `fab80fce` /
next port release; see the port's `docs/mod-sdk-map-hud.md`) and used by minimap
0.2.1. The original proposal below records why native guest calls were insufficient.
[Port compatibility](SDK-INTEGRATION.md) describes what 0.2.1 does with and without
these calls.
Older ports still lack these two services; their existing behavior is retained.
Native alternatives were investigated first; see [NATIVE-CALLS.md](NATIVE-CALLS.md).
Chart names use the existing native message loader, so no message SDK addition is needed. The guest mod must not write native HUD memory.

The last two sections are new proposals from minimap 0.2.4 (floor-change markers); they are not implemented.

## Boat compass ownership (gap 13)

Implemented call: `s32 wwhd_hud_boat_compasses(u32 replace)`.

- `replace = 1` renews a lease for the calling mod for the current logic step.
  The host suppresses the native boat compass draw and reserves its normal position
  for the requesting mod. Return 1 if the lease is active, 0 when there is no eligible
  boat HUD, and a negative value if unsupported or another mod owns it.
- The host owns the visibility/geometry changes. The guest never receives pane pointers.
  Preserve the game's visibility gates, menu/first-person/cutscene rules, layout identity,
  animations, and cannon/crane controls. Do not reparent native panels.
- `replace = 0`, a missed renewal, package disable/unload, state load, HUD destruction,
  stage replacement or an exception revokes the lease. Revocation restores native
  presentation through its own update path, without restoring stale saved transforms.
- Map entry hides the compass before the first map frame. Ordinary map exit completes
  in 250 ms; compass return starts after a further 200 ms. Reversals use the displayed
  fraction. HUD suppression cancels presentation without drawing stale contents.
- Capability discovery and arbitration are required. Older ports retain the current
  map-above-compasses placement. Other parity work does not depend on this call.

Host tests should cover overlapping requests, missed renewal, disable, state load,
HUD replacement, hidden-to-hidden handoff and reversal around both timing boundaries.

## Two-way chart synchronization (gap 9)

Implemented calls: `s32 wwhd_map_chart_selected(void)` and
`s32 wwhd_map_chart_select(u32 native_chart_index)`.
Return 1 when queued, 0 for an already selected chart, and a negative status for
unsupported, missing or invalid state. The setter validates ownership, deciphering
and chart kind, then queues the game's
normal chart carousel event on its game thread. It must preserve the outgoing,
selected and processed chart lifecycle; initializing the screen directly is unsafe.
Reject draw-phase calls and missing/replaced menu instances, and expose no HUD pointers.
The guest makes no native selection write. It delegates requests to the host;
only older ports use the caption observation fallback. Chart identity remains
separate from the eligible-list position.

An uncommitted request belongs to the calling mod and is canceled on explicit
`wwhd_map_chart_cancel()`, disable/unload, state load or HUD/stage replacement.
Cancel leaves the native selection unchanged. After native commit, the selection
is an ordinary player choice: subsequent GamePad/native selection changes it, and
unloading the minimap must not restore an older cached choice over newer input.
No temporary chart pane ownership survives a call; native destruction handles its
own carousel and child resources. Host lifecycle tests must exercise cancellation
before commit, replacement during animation and later GamePad input.

## Mirrored UVs for native half art (floor-change markers)

Status: proposed (HD minimap 0.2.4).

The GamePad's floor-change markers (`DungeonMapUp_00`, `DungeonMapDown_00`) draw
`MapFloorUpDown_00^t` in the picture pane `P_Up_00` with UVs from 0 to 2 on a mirrored
texture: the texture is half an arrow and the hardware mirror makes the whole one. A
guest can reproduce that only by drawing the texture twice, the second copy with
`u0 = 1, u1 = 0`. `wwhd_hud_emit` rejects that element (`geometry()` requires
`u1 >= u0` and `v1 >= v0`) and marks the mod's whole draw list invalid ("HUD draw list
dropped"), so the map and its icons vanish. Minimap 0.2.4 therefore draws its own plain
triangle (`assets/floor-arrow.png`) with the game's size, rotation and alpha.

Proposal, either of:

- accept `u0 > u1` and/or `v0 > v1` in image elements and sample the texture mirrored on
  that axis (the bounds checks stay `0..1` on both ends); or
- add a flags field or element kind for mirrored X/Y sampling, keeping the 72-byte ABI.

Capability discovery is needed so that older hosts keep the fallback triangle. With it, the
minimap would use the game's own arrow art and drop the packaged triangle.

## Localized layout text (floor-change labels)

Status: proposed (HD minimap 0.2.4).

The same markers label the floor they lead to with `T_Floor_00`, whose text comes from
the layout's message file (`DungeonMapUp_00_msbt` / `DungeonMapDown_00_msbt`, labels
`T_FloorU_00_<floor>` / `T_FloorD_00_<floor>`), different per language (English `B1`,
`1F`; Spanish `S1`, `PB`; French, German and Italian their own). The guest has no way to
read a layout message or to know the running language, so 0.2.4 writes English labels.

Proposal: a read-only structured query, like `layout-pane:`, returning a layout
message's UTF-8 text for the running language, e.g. key
`layout-message:DungeonMapUp_00/T_FloorU_00_2F`. A query of the running language's
code would also do, with the mod carrying its own tables, but reading the game's text
keeps every language right without copies.
