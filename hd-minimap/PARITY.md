# Minimap HD 1.3 parity follow-up — 0.2.1

This follow-up builds on the contributor's HD minimap work merged in PR #13; the
contributor's commits and authorship are unchanged.

This is an implementation and bounded validation report, not a full-playthrough
or complete-equivalence claim. Version 0.2.1 uses the map/HUD services of the port
SDK (devel `fab80fce` / next port release); see [port compatibility](SDK-INTEGRATION.md)
for what changes on newer and older ports. No guest workaround drives GamePad events
or resets compass state.

## Requested gaps

“Done” below means implemented in the cited commit. Gameplay limits are
stated separately; headless checks do not establish companion or boat gameplay.

| # | Requested behavior | Status and evidence |
| --- | --- | --- |
| 1 | Tower boat Down: compact/expanded/hidden; L3 zoom | **Done — `41f6830`**. Boat left/right stay native. A native Tower boat check verifies riding, compact/expanded/hidden cycling and L3 zoom; cycle/hidden rules also pass headlessly. |
| 2 | Shared dungeon state on foot/boat | **Done — `41f6830`, `1ba6c41`**. A capture verifies expanded/zoom survives native disembark into Tower swimming. One dungeon choice survives riding changes; context matrix checks hidden/compact/expanded and both orientations. |
| 3 | Independent remembered marine view/chart | **Done — `41f6830`, `103b5d4`, `6a7e010`, `583d9b8`**. First marine use inherits the last foot choice; subsequent boarding preserves the marine request; chart identity uses native remapped indices, not eligible-list positions. |
| 4 | Follow controlled Medli/Makar: facing/floor/center, return to Link | **Done — `41f6830`, `14d8da2`**. Native companion identity must match current control, exact temple stage and a valid pointer. Stable Link drives capture. Actual Command Melody was not reached; source/identity/floor rules reviewed and headlessly checked. |
| 5 | Continuous zoom/camera across role/floor changes | **Done — `a96de49`**. Retained normalized camera pose and shortest-angle interpolation; a fixed zoom span avoids per-floor resets. DRC zoom checked; companion handoff remains unplayed. |
| 6 | Airborne/gliding floors without flicker | **Done — `41f6830`, `1ba6c41`**. Height-table queries run every capture, independent of landing; +/-32 probes stabilize the accepted floor. Three-role synthetic boundary sequences pass; a scripted Link height change updates the rendered floor before returning to the ground. Medli glide and Makar flight were not reached. |
| 7 | Show/hide, resize, family handoff, vertical floor slides | **Done — `a96de49`**. Retarget from current geometry; separate chart exit payload; outgoing/incoming floor caches move markers with plans inside the fixed frame. DRC show/hide/resize sequences captured. Boat treasure exit and role/floor transition gameplay remain unplayed. |
| 8 | Localized chart names | **Done — `326e49e`, `6a7e010`**. Native numbered messages, correct live chart inventory, bounded UTF-16 conversion, numeric fallback. English/French native lookups and test-only in-game chart previews verified. See NATIVE-CALLS.md. |
| 9 | Chart synchronized both ways with GamePad | **Done — 0.2.1 on SDK-capable ports.** Observes canonical native selection and queues SDK selection on local input; uncommitted requests are cancelled on exit/replacement. Older ports retain one-way caption following. See [port compatibility](SDK-INTEGRATION.md). |
| 10 | Animated wind arrow | **Done — `a96de49`**. Gentle size pulse follows logic time and wind heading; island-only. Visible in Outset chart-preview captures. |
| 11 | Fortress exterior chests | **Done — `27adf89`**. Reads the game's four initialized map coordinates and stage-2 opened flags, with Compass gating; does not depend on the sea room's TRES list. Final scene campaign confirms four eligible markers become three when chest 0 is opened; before/after captures inspected. |
| 12 | Possible missing Tower rooms | **Open: needs a reproducible room/floor case.** `14d8da2` fixes primary-floor filtering of multi-floor artwork; `1ba6c41` retries failed image allocations and releases inactive caches. Synthetic multi-floor regression passes; Tower entrance plan rendered. The original unspecified omission is not claimed resolved. |
| 13 | Map replaces boat compasses | **Done — 0.2.1 on SDK-capable ports.** Logic renews the native compass lease through panel/chart exit, uses the reported rectangle, then releases once for the host’s 200 ms return delay. Older ports retain above-compasses placement. See [port compatibility](SDK-INTEGRATION.md). |

## Every 1.3 changelog entry

Source: [upstream CHANGELOG.md](https://github.com/Chironeyes/The.Wind.Waker.HD-Minimap.HD/blob/main/CHANGELOG.md),
1.3 dated 2026-10-26. Rows follow its order, including release/validation paragraphs.

| Entry | Status in this port |
| --- | --- |
| Release introduction: US support and a ZIP containing both patches | **Not applicable to our port packaging**: guest ELF uses the SDK's per-build translation; no Cemu patch ZIP. Behaviors are covered below. |
| US support 1: European v0 + American v0 in one pack | **Done — inherited SDK support; `326e49e` build guard**. Final guest modules build for USA and EU; only USA gameplay was rerun. |
| US support 2: same art/presets/interface with separate regional references | **Done — inherited assets/settings plus `326e49e`**. Canonical declarations are translated per build; no separate copied game art. |
| Fix 1: consistent island coverage on foot/swimming/sailing; repeated Fortress boundary transitions | **Done — `41f6830`, `a96de49`**. All contexts require coverage; the requested marine view survives fallback. Coverage helpers pass; actual sailing-to-swimming boundary handoff was not reached. |
| Fix 2: independent views/chart, temporary coverage loss, no hidden-to-hidden flash | **Done — `41f6830`, `103b5d4`, `a96de49`, `1ba6c41`**. Context matrix and zero-to-zero tween checks pass. |
| Fix 3: animated family resize; treasure finishes leaving into hidden foot map | **Done — `a96de49`**. Size tween persists across family changes; chart tail has its own captured data and fraction. Foot resize verified; boat tail gameplay unverified. |
| Fix 4: no compass return jump or flash behind entering maps | **Done — 0.2.1 with the supported compass lease**, including entry, exit and reversal; older-port fallback retains native compasses. |
| Fix 5: Tower boat plan/controls/frame on direct load and after marine charts | **Done — `41f6830`, `a96de49`** in guest logic. Current family resolves before input; no boarding-time dungeon reset. A native portable restart into the Tower boat is verified; handoff from marine charts is not yet exercised. |
| Fix 6: aligned resources avoid shifted frame/left-edge strip | **Not applicable to our port**: Cemu image-buffer alignment/patch layout is not used. The port's HUD texture handles and one fixed frame rectangle are retained; DRC/Tower/Fortress captures inspected. |
| Fix 7: camera-oriented continuity across character/floor changes | **Done — `a96de49`**; retained pose/zoom. Companion handoff gameplay remains unplayed. |
| QoL 1: controlled Medli/Makar and restoration of Link | **Done — `41f6830`, `14d8da2`**, with the explicit gameplay limit in gap 4. |
| QoL 2: Link/Makar/Medli airborne floor updates, including glide | **Done — `41f6830`, `1ba6c41`**, with source/table review and synthetic checks; actual companion flight unplayed. |
| QoL 3: floors/markers slide vertically, fixed frame, correct direction | **Done — `a96de49`**. Ascending sends old plan downward/new plan in from above; descending reverses it. A scripted Link height change captured the floor update and slide; natural stair/flight traversal and companions remain unplayed. |
| QoL 4: preserve dungeon visibility/size/orientation across foot/swim/boat and Tower re-entry | **Done — `41f6830`, `1ba6c41`**. Shared state and context tests; Tower swimming plan reached. |
| QoL 5: boat dungeon Down/L3; native left/right; only bottom custom shortcut | **Done — `41f6830`**. Headless input/icon checks plus native Tower boat cycle/zoom captures. |
| QoL 6: 250 ms exits, 200 ms compass-return delay, reversals | **Done — `a96de49` plus 0.2.1 SDK integration.** The guest retains ownership through its 250 ms exit and the host delays compass return by 200 ms. |
| Validation paragraph 1: Cemu 2.6 Pro Controller regional campaign | **Not applicable as port validation**. Upstream's campaign is not inherited by this follow-up; this port's checks are listed below. |
| Validation paragraph 2: 1,242 synthetic checks, instruction/hook/buffer counts and ending observer | **Not applicable to the guest renderer**. No claim that those PPC checks test this C mod. This mod's C tests and USA/EU module builds are separate evidence. |
| Retained behavior: discovery, Map/Compass, opened chests, chart/menu synchronization, ending safety, Fortress distant projection | **Done for existing gates and new native inventory/chest flags — `6a7e010`, `27adf89`**. **Done — 0.2.1 SDK integration** for reverse chart sync on capable ports. Cemu pane-attachment ending fixes are **not applicable**: this mod never reparents native panes. Ending gameplay was not replayed; distant Fortress projection is not claimed corrected. |
| Installed-game assets; no saves/controller profiles/executable/Xbox pack shipped; bounded validation claim | **Done — existing runtime art service, this report and notices**. No private game/test artifacts are committed. |

## Validation and limits

The following is the **0.2.0** campaign. For the 0.2.1 module builds and SDK
scene checks, see [port compatibility](SDK-INTEGRATION.md).

- Host tests: `python3 -m unittest discover -s hd-minimap/tests` passes. Its C binary
  checks sectors/coverage, floors, multi-floor room eligibility, context handoffs,
  all dungeon boat cycle states, input/icon gates, 30/60/120 Hz easing/reversals,
  250 ms exits, chart-message ranges, UTF-16 tags/surrogates/truncation and build gates.
- Strict PowerPC compile passes with `-Wall -Wextra -Werror`. Final relocatable ELF
  translates and compiles into native modules for both USA and EU. Release ELF SHA-256:
  `bb477dae8a45fbe22e367994bc4d321cf4a0f0a6c67c1150b998f78561fbe4fd`.
- UndefinedBehaviorSanitizer host tests pass. AddressSanitizer could not start:
  the macOS sanitizer runtime deadlocked during shadow-memory initialization before
  `main`; no ASan pass is claimed.
- Runtime: macOS, Metal, USA, port `fa1edf4`. Fresh empty save folders were used;
  no existing saves were read or altered. Native stage warps
  and test-only inventory/Map/Compass/opened flags are fixture setup, not a normal
  progression/playthrough claim.
- DRC on foot, Tower entrance while swimming, Fortress exterior, and Outset island
  were reached. DRC show/hide/resize/zoom and contextual plans were captured. Command
  Melody, companion flight/glide and the unspecified missing Tower room were not reached.
  Tower boat controls were subsequently checked using an authored native restart fixture.
- English/French chart previews use a test-only build with **only** an extra draw
  call showing the chart on foot. They verify the native message/inventory path,
  artwork and caption layout; they do not verify boat controls or chart handoff.
  The preview build is not part of the repository; no preview condition is shipped.
- Additional audit: `52769d2` fixes Fortress exterior Down handling before its mode
  was changed to dungeon, and clears altitude history when leaving a dungeon or
  starting a stage transition. Captures confirm Down now zooms.
- Native boat fixture: a new Quest Log created for these checks; a portable restart
  is derived with `has_ship/on_ship` metadata and
  event bit 0x0F80, the creation prerequisite checked by `daShip_c::create` at
  0x0247C358. The native restart creates a real boat; logs show its actor position
  and riding status. No live HUD/riding-status flag is overridden by the mod.
  This is authored fixture setup, not natural story progression.
- Resource audit: `b417ade` releases inactive image families, icons and markers within
  the SDK limit of 32 images, retaining outgoing chart/panel resources until off-screen.
  Native two-floor artwork counts peak at 21 in Tower; stale unrelated caches could
  otherwise starve room loads. This mitigation does not establish gap 12 resolved.
  The closing panel also retains its own zoom choice across a context change.
  Two boat handoff runs exercised the cleanup before that final zoom snapshot refinement;
  the final ELF has compile/translation and host checks, not another gameplay run.
- The sea handoff attempts finish cleanly but enter native story
  dialogue at Outset. Their sea chart cycle/exit captures are inconclusive; no
  gameplay pass is claimed for marine memory, treasure exit, or hidden-to-hidden handoff.

## Scenes captured

- DRC on foot: compact, expanded, exiting, fully hidden, entering, resize and camera zoom.
- Fortress exterior: Down overview/zoom regression and the opened-chest check (four
  eligible markers become three).
- Tower of the Gods: native boat compact/expanded/zoom/hidden cycle; boat to swimming
  retaining expanded zoom; entrance plan while swimming.
- Scripted Link height change: floor 128 → 130 → 128. Link is deliberately moved above
  the room ceiling, so the world camera can show black; this is not a natural traversal test.
- Chart caption previews: English “Treasure Chart 1” (native index 3, 46 eligible charts
  without decoded Triforce flags) and French “Carte au trésor 1”; English/French
  Triforce Chart 1 message lookups preceded the final native-inventory refinement.

The captures are screenshots, not a finished video recording. No screenshot, state,
game asset, compiled object or test save is included in the repository.

## SDK decisions

Gaps 9 and 13 use the port's map/HUD services in 0.2.1 (see
[port compatibility](SDK-INTEGRATION.md)); the validation campaign above predates
this integration unless explicitly noted.
[NATIVE-CALLS.md](NATIVE-CALLS.md) documents addresses, callers, message labels,
chart lifecycle and why compass Wait reopens. [SDK-PROPOSAL.md](SDK-PROPOSAL.md)
records the original proposal for the supported calls, ownership, failure behavior
and automatic undo.
