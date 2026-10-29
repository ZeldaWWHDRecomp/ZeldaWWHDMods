# Supported map integration proposal

Status: proposal for the port lead; no port or SDK changes are included here.
The existing guest SDK can draw private TV elements and read layout art. It has no
supported ownership mechanism for native boat compasses or native chart selection.
Native alternatives were investigated first; see [NATIVE-CALLS.md](NATIVE-CALLS.md).
Chart names use the existing native message loader, so no message SDK addition is needed. The guest mod must not write native HUD memory.

## Boat compass ownership (gap 13)

Proposed call: `s32 wwhd_hud_boat_compasses(u32 replace)`.

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

Proposed calls: `s32 wwhd_map_chart_selected(void)` and
`s32 wwhd_map_chart_select(u32 native_chart_index)`.
Return 1 when queued, 0 for an already selected chart, and a negative status for
unsupported, missing or invalid state. The setter validates ownership, deciphering
and chart kind, then queues the game's
normal chart carousel event on its game thread. It must preserve the outgoing,
selected and processed chart lifecycle; initializing the screen directly is unsafe.
Reject draw-phase calls and missing/replaced menu instances, and expose no HUD pointers.
No native selection write is made by this mod. It currently follows eligible menu
caption changes and remembers chart identity separately from the eligible-list position.

An uncommitted request belongs to the calling mod and is canceled on explicit
`wwhd_map_chart_cancel()`, disable/unload, state load or HUD/stage replacement.
Cancel leaves the native selection unchanged. After native commit, the selection
is an ordinary player choice: subsequent GamePad/native selection changes it, and
unloading the minimap must not restore an older cached choice over newer input.
No temporary chart pane ownership survives a call; native destruction handles its
own carousel and child resources. Host lifecycle tests must exercise cancellation
before commit, replacement during animation and later GamePad input.
