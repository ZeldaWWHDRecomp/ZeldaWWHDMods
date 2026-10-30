# Native calls and build guards

Addresses below are USA v0 canonical addresses. `WWHD_GAME_FUNC` declarations go
through the port's build translator, not direct absolute function-pointer casts.
The translator maps EU v0 identities and rejects unrecognized/non-equivalent call
sites at installation. Runtime configuration also reads the supported SDK key
`game.build` and enables the mod only for `USA` or `EU`; missing or unknown identity
means no native calls and no drawing. Pointer/range/count checks precede object access.
No native HUD fields are written by the guest.

## Chart names

The implementation follows `qol/src/picto_subjects.c` on the mods repository's
`devel` branch, rather than requiring a new SDK function:

| Canonical address | Call | Identification |
| --- | --- | --- |
| `0x02687A64` | Coordinator `findChart(index)` | Same inventory lookup used by the GamePad and Minimap HD's `_boatChartScan`; native initialization rewrites raw chart numbers/types before lookup |
| `0x025B8308` | `dSv_player_map_c::isTriforce` | Coordinator decoding predicate at `0x0268A6B4`; only native type 0 requires the deciphered flag |
| `0x025F65F4` | Construct private `HdMsgText` | QoL `picto_msg_construct`, used before the numbered-message loader |
| `0x025F50E8` | `MsgResMgr::getMessage(manager, out, set, label)` | QoL `picto_msg_load`; native `hdTextUnit::putMessage` numbered-message path |
| `0x0273A2E8` | Read UTF-16 message text by label index | QoL `picto_msg_text`; checks the loaded set and its entry count first |
| `0x02699094` | Caption-selection observation (return hook only) | `hdMenuMap_c::setChartCaption` in `wwhd_src/d/hd_screen_menu_map_states.cpp`; generated USA instructions at `026990A8`, `026990C0`, `026990D8`, `026990F0` confirm the four message ranges |

Native chart indices 0–2 use `index + 0x31D8`; 3–35 use `index + 0x31DD`;
36–43 use `index + 0x36F1`; 44–48 use `index + 0x36FD`. These are native indices,
not displayed chart numbers, sectors or raw archive positions. The native coordinator
remaps the original Triforce chart numbers into three Triforce charts, forty-one
treasure charts and five additional treasure charts. The mod now reads its live
remapped entries, with native type filtering, opened-chart and deciphering gates. The label is five decimal digits (`%05d`).
An empty set selector delegates to the game's `message*` set search in the player's
installed language, exactly as QoL does. No guessed English set or names are shipped.
The manager is at `0x101F4AE8`, SafeString vtable at `0x100E0F90` (both declared
with `WWHD_GAME_DATA` so the translator maps them). The private message object is
0x924 bytes; its loaded set is at +8 and label index at +0x118.

Captions are read during the logic hook, not while drawing. The copied caption
belongs to the map snapshot, including a finishing treasure-panel exit. Bounded
UTF-16 conversion skips sized native formatting tags and handles surrogate pairs;
missing resources keep the numeric fallback. State-load epoch invalidates the cache.

## Controlled character

`0x02656970`, the native companion lookup, is identified by Minimap HD 1.3's
`_sageFocusControlled` call and checked in the corresponding USA game dispatcher.
It is called only in exact `M_Dai`/`kaze` stages and only after a known-build check.
Its result must be a guest heap pointer identical to the current controlled actor
(play +0x5B2C) and different from stable Link (play +0x5B34). No actor pointer is
retained across steps. Existing floor calls `0x0258DC48`/`0x0258DBF4` directly query
height-based stage/room floor data; `dMap_GetFloorNo_WithRoom` calls the raw table
lookup at `0x0258DB6C`, so the guest does not inherit the menu's landing-held floor.

## Fortress exterior chests

The native exterior/fixed map path at `0x026581C8` calls `initFixedTreasures`
(`0x026580E0`), using four records at `0x101F5A70`, initialized from the installed
image by the dungeon map's static initializer. Their X/Y are map coordinates,
not world positions or the sea room's TRES list. Marker conversion at `0x02666620`
uses unit scale and negates Y for this native map context. The mod reads that live
table with `WWHD_GAME_DATA`, validates coordinates/kind, and uses the existing
`stage_chest_open(2, kind)` (`0x02520864`) exactly as the coordinator's exterior
opened predicate (`0x02689DDC`) does. The Compass remains required. No coordinate
or game artwork is packaged in the mod.

## Chart synchronization: native-only investigation

Reviewed the actual GamePad path in `hd_screen_menu_map_states.cpp` and the USA
instruction-derived `build/gen/code_057.c`:

- `0x0269B238` (`hdMenuMapHandleEvent`) reads a **global input queue**, copies its
  event, and handles previous/next types `0x5D`/`0x5E`. It does not take a chart
  index or an event argument. The writes at `0269B2C4` and `0269B310` update +AC,
  with +B0 outgoing and +B8 direction owned by the native carousel.
- `0x02698998` (`findChart`) computes an owned-chart candidate; it does not select it.
- `0x026ECDB0` (`hdTreMapScreen_c::selectChart`) selects artwork on a **child** screen,
  not the canonical MenuMap choice. Calling it would let title/art/carousel disagree.
- `0x02699118` resets the current selection and carousel; it takes no requested index.
- Other stores to the canonical +AC field in this MenuMap unit are construction/setup
  (`0x0269735C`, `0x0269C6BC`), not callable selection setters.
- The reference pack's comment naming `0x0269B9F4` as a next-chart action is not a
  callable entry in this USA image: it is an interior vtable-load instruction in
  chart-detail updating. It is not declared as a function.
- `0x0269BBA4`, `0x0269BDE0` manage the carousel from its existing fields; they are
  not setters. No synthetic global input-queue mutation is made.

There is no verified explicit-index selector in the reviewed path. Version 0.2.1
uses the supported SDK bridge instead; one-way caption following remains only
on older ports. None of these candidate mutators is called by the guest.

## Boat compass visibility: native-only investigation

Reviewed `hd_screen_hud_main.cpp`, `hd_screen_compass_clock.cpp`, `hd_parts.cpp`,
`hd_parts_no_control.cpp` and `hd_parts_layout.cpp`:

- HUD factory strings `L_CompassClock_00` / `CompassClock_00` identify HUD child 4.
- `0x0264CE34` (`hdCompassClock_c::toWait`) changes to Wait; it is the HUD's own reset
  path. It is **not** a persistent hide API: `0x0264C544` (`waitExecute`) reopens via
  the In state while `sCompassActive` is set by sailing.
- `0x0264C528` is Wait's animation-frame initializer; `0x0264C850` is Display's update.
  Repeated resets can restart native transitions/sounds and do not provide a scoped,
  reversible visibility claim.
- Compass Out/Clock Out methods are state callbacks, not public hide/show ownership
  functions; calling an enter callback alone does not change the state machine.
- Base parts draw (`0x026F8B54`) and child draw (`0x026F9020`) draw the hierarchy;
  no verified paired compass-specific visibility setter exists in these units.

No candidate compass mutator is called. Version 0.2.1 uses the SDK compass lease,
rectangle query and automatic undo. Map-above-compasses remains the older-port
fallback; see [SDK-INTEGRATION.md](SDK-INTEGRATION.md).

Sources: [QoL message pattern](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDMods/blob/devel/qol/src/picto_subjects.c),
[MenuMap](https://github.com/ZeldaWWHDDecomp/wwhd/blob/main/wwhd_src/d/hd_screen_menu_map_states.cpp),
[compass state machine](https://github.com/ZeldaWWHDDecomp/wwhd/blob/main/wwhd_src/d/hd_screen_compass_clock.cpp),
[HUD factory](https://github.com/ZeldaWWHDDecomp/wwhd/blob/main/wwhd_src/d/hd_screen_hud_main.cpp).
