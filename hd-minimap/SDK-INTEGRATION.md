# Native map/HUD SDK integration — 0.2.1

Local branch `hd-minimap-sdk-integration` continues parity head `1450999`, already
rebased onto merged devel `e32095b7`. Implementation commit: `a1ab9ba`.
Nothing was pushed. The matching port SDK is local branch `codex-sdk-map-hud`,
commit `d034b49370eb7f2ccfba5bb5c9a9203dcf6a386e`.

## Behavior and compatibility

Gaps 9 (two-way chart selection) and 13 (native boat compass replacement) are
implemented using the supported SDK. The numbered native chart-message loader,
USA/EU build guard and localized names from 0.2.0 remain in place. Native addresses,
callers and the original lifecycle investigation remain in [NATIVE-CALLS.md](NATIVE-CALLS.md).
The guest adds no raw native mutator or game-memory write.

- Query unsigned `hud.boat-compasses.version` and `map.chart-selection.version`;
  absent capabilities leave both zero. Probe again when the HUD context is reset.
  Default SDK helpers use the existing optional `wwhd_setting_get` transport;
  the ELF introduces no mandatory service imports that an older installer lacks.
- Renew `wwhd_hud_boat_compasses(1)` in Link logic while a visible boat panel or
  chart tail remains. Use `wwhd_hud_boat_compasses_rect` only after success; its
  TV rectangle supplies the anchor, with configured x/y offsets. Renew throughout
  the 250 ms exit, release once with `(0)` when both payloads finish, then let the
  host provide its 200 ms return delay. Reversal retargets current geometry.
- Visibility loss skips renewal. Host revocation restores native drawing; a failed
  owned-rectangle recheck drops private presentation safely before restarting.
  No pane is reparented and no stale transform is restored. `toWait()` is not called.
- Poll `wwhd_map_chart_selected` in logic. L3 queues one
  `wwhd_map_chart_select(native_index)` per input edge. Keep local intent while the
  native screen has not committed; acknowledge its selected index or cancel an
  uncommitted request when an unrelated native choice wins. Exiting chart mode,
  invalid context and HUD reset cancel pending intent through `wwhd_map_chart_cancel`.
  Never restore an old native selection after commit. The host owns validation and
  the native outgoing/selected/processed carousel lifecycle.
- Older ports retain the above-compasses map and one-way eligible-caption following.
  Missing/unsupported/busy APIs never trigger a private memory workaround.
- Presentation advances in logic rather than draw. Covered undiscovered-sector
  hints now share panel visibility, exit and lease rules, including hidden mode.

## Checks and evidence

Host C tests via `python3 -m unittest discover -s hd-minimap/tests` and UBSan pass.
New cases cover pending chart acknowledgment/conflict/cancellation, visibility,
coverage, independent chart tails and one release after 250 ms at 30/60/120 Hz.
Strict PowerPC compile passes with `-Wall -Wextra -Werror`; native guest-module
translation/compilation succeeds for USA and EU. Final relocatable ELF SHA-256:
`c2456555ee355930eacd90023e3ce45418915632158ead437b0bf0d9af71952b`.

Private evidence root (not shipped):
`/Users/lschaupp/projects/zelda_wwhd-work/hd-minimap-sdk-proof/`.
Runs retain environment, logs, package, own saves/states and screenshots. Fixtures
derive only from this task's own blank Quest Log, with authored native restart,
progression/inventory and discovery flags. They are not normal story progression.
Runs were muted, load-guarded at <=90, with at most two game processes at a time.

| Evidence | Result |
| --- | --- |
| `new-port-07/frame_9400_present.png` | Boat map/chart occupy the native compass corner, with compasses suppressed. |
| `new-port-07/frame_9900_present.png` | L3 selects Treasure Chart 2; native GamePad also shows Treasure Chart 2. |
| `new-port-07/frame_10200_present.png` | Subsequent native GamePad arrow selects Treasure Chart 3; minimap follows it. |
| `new-port-07/frame_10307_present.png` | Exit reversal retains a partial map without a native compass flash. |
| `new-port-07/frame_10814_present.png`, `frame_10820_present.png` | Map is gone while return delay remains, then native compasses return. |
| `old-port-02/frame_9900_present.png`, `log-retry` | Final ELF loads on older port `fa1edf4`/installer 0.2; capabilities zero, above-compass fallback and no reverse setter. Clean exit. |

`new-port-03` used the preceding candidate with step-stamp gating at 30 Hz. The
final source removes that gate so each real Link invocation renews ownership;
final-code runtime checks are recorded below. The earlier candidate's scenes are
supporting evidence, not a substitute for the final module builds.

Final-code run `new-port-05` exits cleanly after a native sea-to-Tower portable
restart. `frame_9400_present.png` shows Treasure Chart 1 on both displays;
`frame_10200_present.png` follows later native Treasure Chart 3. Tower
`frame_12400_present.png` shows the expanded plan, `frame_12500_present.png` its
L3 zoom, and `frame_12700_present.png` the hidden plan. Native dungeon compass visibility remains the game’s decision.
The runtime switches to true60 mode at simulation t=300 s, but logs explicitly
keep boat Link at 30 Hz (zero Link 60 Hz steps). This proves compatibility with
that runtime mode, not a 60 Hz boat actor. Some single-frame scripted inputs are
missed in that mode, so its marine exit timing is not claimed as a pass.

`new-port-06` omitted its private builder configuration, so no mod loaded; it
is excluded from mod evidence. The repeat `new-port-07` restores that configuration and verifies final-code
30 Hz entry, local/native chart changes, reversal, 250 ms exit and delayed compass
return (the final-code captures are listed in the table).

Initial `new-port-01` was outside island-art coverage and does not prove replacement.
`new-port-04` enabled true60 before boot scripting and remained in the intro;
it is explicitly excluded from gameplay evidence. All artifacts are retained.

## Limits and retained artifacts

EU gameplay, actual Command Melody/companion flight and the unspecified missing
Tower room are not established by these checks. Gap 12 still needs a reproducible
room/floor case; companion gameplay validation remains independent of these SDK
features. Host lifecycle tests in the port cover arbitration, unload/state/HUD
replacement and cancellation; those scenarios are not all replayed with this mod.
See [PARITY.md](PARITY.md) for the retained broader 0.2.0 campaign.

No private assets, saves, screenshots, objects or compiled modules are committed.
`retained-paths.json` under the private root inventories sizes without deleting
anything. `sdk.json`, `index.json` and the port README/CHANGELOG are untouched.
