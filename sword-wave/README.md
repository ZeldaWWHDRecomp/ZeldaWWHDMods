# Sword Wave

At **full hearts**, a sword Spin Attack fires one forward Sword Wave. Quick spins,
charged spins and the start of Hurricane Spin use the same native attack gate.
Charging alone, ordinary slashes and spinning with a carried enemy weapon do not
fire a wave. Healing after the first damaging spin frame does not fire it late.

The wave uses WWHD's three real Spin Attack particle resources, native callback,
sword colors and width scaling. It has its own attack-only sword capsule and
particle storage; Link's attack, models, inventory and save bytes are unchanged.
No textures, models, game bytes or host runtime changes are included.

## Open arc correction (0.1.1)

The native stripe renderer joins every particle in an emitter's list. Version
0.1.0 appended a fresh curve every frame, plus an automatic anchor particle;
those end-to-start links produced the triangular attachment behind the front.

Each wave now seeds one open arc per resource, disables automatic emission and
moves those same particles along the original front curve. The native cut-turn
callback still creates the particles/materials. Owned-particle hooks bracket
native particle calculation: renew their short lifetime before aging and anchor
their positions after physics, before rendering. No old arcs or endpoint joins
accumulate. The original front curve, colors, width, damage and motion remain.

Native linked lists, allocation and deferred GPU teardown are untouched. The
particles expire naturally after detachment; regular Link/other emitters pass
through both observer hooks unchanged. Partial allocation remains bounded.
Translated tests model native prepend/callback ordering and aging, check ordered
open geometry over repeated 30/60-Hz steps, unchanged front coordinates, pause,
partial allocation, constant particle count and unrelated-emitter isolation.
Visual acceptance on the actual game is still required.

## Behavior

Based on Dawnlight's **normal Great Spin projectile**, not Survivors' upgrades:

- One active wave; a new eligible spin replaces the previous wave.
- 40 units per native 30-Hz tick (1200 units/second), 2400-unit range.
- A transverse capsule with half-width 150 and radius 55. Damage starts beyond
  the native spin radius plus 55, avoiding a second melee hit at launch.
- Native sword damage/defenses/reactions, using the current spin's attack power
  and material (including Hero's Sword / Master Sword / soup differences).
- Native continuous-hit suppression; unshielded enemies do not consume the wave.
- Stops at background walls or a registered native shield block. Center/edge
  line sweeps prevent the wide wave from simply passing through narrow gaps.
- Lock-on aims toward the target's eye. Without lock-on, shoots in Link's heading
  at spin start. This intentionally uses the requested **forward** direction;
  Dawnlight's free-camera fallback is not imported.
- Full hearts are required **at release**. Losing hearts afterward does not
  recall an already launched wave. No magic or other resource is consumed.
- Pauses with gameplay. Clears on death, events, stage/room/owner changes,
  save loading and the live Enable Sword Wave option being turned off.

Install `sword-wave-0.1.1.wwhdmod` through **Settings → Mods**, enable it and restart.
The game needs existing code-mod support, as with the other standalone mods.
No rebuild of the app is needed. Source: `sword-wave/src/`.

## Build and catalogue validation

Use a clean SDK checkout at the commit in the repository's `sdk.json`.
The catalogue builds the single entrypoint `src/mod.c`; source files, licence
and original artwork are included by the existing repository packager.

```sh
export WWHD_SDK=/absolute/path/to/pinned/ZeldaWWHDRecomp
python3 sword-wave/tests/build_and_test.py
```

The local regression runner requires Python 3, PowerPC-enabled Clang/LLD and a
native Clang compiler. Generated files stay under the repository's ignored
`build/` directory. It does not require game files. For catalogue publication,
commit the sources first and run the repository's documented `tools/guard.py`,
`tools/package.py` and `tools/validate.py` against the pinned SDK. The local
runner's package is a test artifact, not the published catalogue download.

The maintained catalogue generates the download URL, size and checksum.
Contributors edit `catalogue.json`; do not hand-edit `index.json` or commit ELF/ZIP
files. Publication is separate from opening or merging a source PR.

## Source / ABI evidence

Reviewed `ZeldaWWHDDecomp/wwhd` at `a874e1a9243f573af44c2649a2f2107dbebb50be`
(CC0) and `BeZide93/dawnlight` at `b5e9046cd98bdd8593ab2cf219d72a0f61ef7213`,
`src/great_spin_projectile.cpp`. This mod is a new C guest adapter.

| Native source | Contract used |
| --- | --- |
| `d_a_player_main_02.cpp`, `main_08.cpp` | Spin init `023E6380`, Hurricane init `02441A44`; proc `55/56`, item `103` |
| `d_a_player_main_04.cpp`, `_04_collision.h` | Spin resources `25/26/28`, callbacks `6730/6740/6750`, attack cylinder `78BC`; sample its active flag/power/material |
| `JPA_particle.cpp`, `JPADrawVisitor.cpp` | Particle calculation `0281FBD8`; native list/stripe order, position and lifetime fields; retained native deferred deletion |
| `d_particle_callbacks.cpp`, `d_particle_water.cpp` | `025A847C` particle set; 16-byte cut-turn callback, native end `025A9D64` detaches it synchronously |
| `d_cc_d.h`, `c_m3d_g_cps.h` | Capsule `138` bytes; geometry start `118`, end `124`, vtable `130`, radius `134`; status `3C` bytes |
| `d_cc_s.cpp`, `c_cc_s.cpp` | `0200E240` native registration, attack-only mask, `AtNoConHit`, shield response bit at `54` |
| `d_bg_s.h`, `d_bg_s_lin_chk.cpp` | Owned `6C`-byte query, base constructor, native vtables from Link's arrow query; relocated embedded pointers |
| `d_com_inf_game.h`, SDK `save.h` | Play/accessor, manager pointers, events/transitions, current/max quarter-hearts |

Only observer hooks are used. All game function/data references pass through SDK
relocation for USA/EU; no replacements or direct enemy HP edits are installed.
Native effect colors are observed at creation, never copied from a game asset.
