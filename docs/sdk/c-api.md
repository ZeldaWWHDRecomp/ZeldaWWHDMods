# The C API

Include `wwhd_guest.h` and the generated `wwhd/` headers from the pinned SDK. Everything below is
declared there; the header comments are the exact contract.

## Hooks and replacements

| Macro | Runs |
| --- | --- |
| `WWHD_HOOK(target, name, (args))` | before the game function, with its arguments |
| `WWHD_HOOK_RETURN(target, name, (args))` | after it, with the same arguments; the game's result is kept |
| `WWHD_REPLACE(target, ret, name, (args))` | instead of it |
| `WWHD_GAME_ORIGINAL(...)` | inside a replacement: the game's own code, below all mod hooks |

- Every game function can be hooked, including calls through function pointers.
- Hooks can't change the arguments or the result. To change them, replace the function and call the
  original yourself.
- Only one mod may replace a function; a second one is an error naming both mods. Any number of
  mods may hook one. Entry hooks run in load order, return hooks in reverse order.
- A hook on a logic function runs once per logic step, at any frame rate; a hook on a drawing
  function runs once per displayed frame. Use `wwhd_logic_dt()` for anything time-based, and don't
  count displayed frames as game steps. Keep frequent hooks small.
- The calling convention is the game's: arguments in r3–r10 / f1–f8, results in r3/r4/f1.

A typical gameplay hook, and the check that a save file is loaded:

```c
#include "wwhd_guest.h"
#include "wwhd/functions.h"
#include "wwhd/save.h"

WWHD_HOOK(WWHD_ADDR_daPy_Execute, my_step, (void* link)) {
    (void)link;
    u16 max_life = dComIfGs_player_status_a.max_life;
    if (max_life < 12 || max_life > 80) return;  /* no save file loaded */
    /* ... */
}
```

## Game functions, data and objects

The headers come from the public [HD decompilation](https://github.com/ZeldaWWHDDecomp/wwhd) and use
the USA game's addresses; the port maps them for European installs.

- `wwhd/functions.h`: every public verified function as a hook target, `WWHD_ADDR_<name>`.
  Ambiguous names have an address suffix.
- `wwhd/bindings.h`: callable declarations, `wwhd_<name>(...)`. Object pointers are `void*`.
  Functions returning a register pair use `wwhd_gpr_pair` with `WWHD_RESULT_R3` / `WWHD_RESULT_R4`.
- `wwhd/data.h`: named save/resource pointers, matrix stack and item tables.
- `wwhd/actor.h`, `link.h`, `camera.h`, `items.h`, `messages.h`, `save.h`: partial views of game
  objects with checked field offsets; unknown parts are reachable as bytes.
- Functions not in the public decomp can still be hooked by address with
  `WWHD_GAME_FUNC(address, ret, name, (args))`; game variables with `WWHD_GAME_DATA(address, type)`.

**European installs.** Write your mod against USA addresses; when it is built on an EU install, the
port translates hook targets, game calls, function pointers and `WWHD_GAME_DATA` references. Functions
that differ or don't exist in EU are refused with an error naming them. Don't cast integers to
pointers (that hard-codes a USA address), and only use object fields that are the same in both
versions.

## Host services

| Service | What it does |
| --- | --- |
| `wwhd_log`, `_int`, `_hex`, `_float` | log lines tagged with your mod's ID |
| `wwhd_config_int`, `_bool`, `_float`, `_string` | your mod's options (values from startup, or as they are now for `"live"` options; the fallback on a missing option or wrong type) |
| `wwhd_config_changed` | a live option's revision (port 0.5.0), below |
| `wwhd_malloc`, `wwhd_free` | per-mod heap, 16-byte aligned; size from `guest.heap_size`; null when full |
| `wwhd_input_read` | buttons, sticks and touch (read-only) |
| `wwhd_input_filter_register` | change the controller state the game reads (port 0.5.0), below |
| `wwhd_touch_register`, `wwhd_touch_claim` | touches and mouse clicks on parts of the TV picture your mod claims, and pinches there (port 0.5.0), below |
| `wwhd_screen_point` | where the drawn camera shows a point of the world, in touch units (port 0.5.0), below |
| `wwhd_mic_open`, `_close`, `_state`, `_pitch` | the microphone's pitch, while an option the player turned on allows it (port 0.5.0), below |
| `wwhd_file_read`, `wwhd_file_write` | flat file names in your mod's own data folder, at most 1 MiB per call; write replaces the file; return bytes or -1 |
| `wwhd_logic_dt`, `wwhd_logic_step` | the length of the current logic step in seconds (60 fps modes included) and the step counter |
| `wwhd_setting_get`, `wwhd_setting_changed` | read-only port settings, below |
| `wwhd_hud_*` | drawing on screen, below |
| `wwhd_audio_*` | bounded PCM audio streams, below |
| `wwhd_mail_*`, `wwhd_chart_*`, `wwhd_quest_*`, `wwhd_item_*` | letters, charts, quests and items (port 0.3.0): [Story mods](story.md) |
| `memcpy`, `memmove`, `memset` | as usual |

Your mod can also call any game function through `wwhd/bindings.h`.

## Live options, input filters and touches

These need port 0.5.0: put `"minimum_manager_version": "1.6.0"` in the manifest, so older ports refuse
the package with "needs a newer version of the game". [QoL extras](examples.md#qol-extras) uses all of
them.

**Live options.** Mark an option `"live": true` in the manifest and the player can change it while
playing (in Settings or on the item menu's Settings page). `wwhd_config_*` then return the value as it is
now, and `wwhd_config_changed(id)` returns the option's revision: 1 at start, one more per change (0 for
an option your mod doesn't have). Check it once per logic step and read the value again when it changed.
Keep your hooks installed and return early while a feature is off.

```c
static unsigned long long seen;
static int clothes_on;
static void options(void) {
    unsigned long long r = wwhd_config_changed("clothes");
    if (r != seen) { seen = r; clothes_on = wwhd_config_bool("clothes", 0); }
}
```

**Input filters.** `wwhd_input_filter_register(fn)` calls `fn(state)` on each of the game's controller
reads (once per logic step, also at 60 fps), after the port's own touch rules and before the game sees
it. Change `buttons` and the sticks; the touch fields are read-only. One filter per mod (0 removes it);
filters of several mods run in load order.

```c
static void filter(wwhd_input_state* s) {
    if (fire > 0) { fire--; s->buttons |= 0x8000u; }  /* hold A */
}
/* once, from a hook: */ wwhd_input_filter_register(filter);
```

**Touches.** Register one callback with `wwhd_touch_register(fn)`, then claim rectangles with
`wwhd_touch_claim(slot, x0, y0, x1, y1)`, slots 0–7. Units are layout units: 1280 × 720 around the
picture's centre, +y up (a wider picture extends x). A finger on a touch screen (with the on-screen
controls shown or hidden) or a mouse click on the TV picture that lands in a claim goes only to your
callback, as a `wwhd_touch_event` with `kind` `WWHD_TOUCH_DOWN`, `_MOVE`, `_UP` or `_CANCEL` and its `x`,
`y`. It stays yours until it lifts, even outside the rectangle. The newest claim wins where claims
overlap. Claim only while your feature shows something to touch, and remove the claim (an empty
rectangle, `x1 <= x0`) when it doesn't, so the player's thumbs work the game again. Events arrive at the
next controller read, before the input filters, so a tap can become a button in the same read.

```c
static void touch(const wwhd_touch_event* e) {
    if (e->kind == WWHD_TOUCH_UP) fire = 4;
}
/* once, from a hook: */ wwhd_touch_register(touch);
/* while the board is shown: */ wwhd_touch_claim(0, -185, -215, 190, 155);
/* when it's gone: */            wwhd_touch_claim(0, 0, 0, 0, 0);
```

**Pinches.** A second finger next to your mod's finger, or the mouse wheel over one of your claims on a
computer, comes as `WWHD_TOUCH_PINCH` with `x` = the spread now divided by the spread when the pinch
began (above 1: fingers apart, or the wheel turned away from the player; each notch is 15 %), then
`WWHD_TOUCH_PINCH_END`. The finger keeps its own events; its moves pause during the pinch.

```c
if (e->kind == WWHD_TOUCH_PINCH) zoom = start_zoom * e->x;
```

**Points on the picture.** `wwhd_screen_point(world, out)` takes a point of the current room (`x, y, z`)
and writes where the drawn camera shows it to `out[0]`, `out[1]`, in the same layout units as touches,
with the picture's shape and field of view as drawn. It returns 1, or 0 when the point is behind the
camera or no camera is drawn. The camera moves: compare on the step you project.

```c
f32 w[3] = {x, y, z}, at[2];
if (wwhd_screen_point(w, at) && (e->x - at[0]) * (e->x - at[0]) + (e->y - at[1]) * (e->y - at[1]) < 40 * 40) {
    /* the finger is on that point */
}
```

**The microphone.** A mod gets the pitch of what the player sings, hums or whistles, never the sound.

- Only through an option the player turns on: a bool option marked `"permission": "microphone"`.
  `wwhd_mic_open()` works only while that option is on; otherwise the state is `WWHD_MIC_UNAVAILABLE`.
- The game never asks for the microphone at install or start. The first `wwhd_mic_open()` asks the
  system then (Android's permission, the macOS prompt); the state is `WWHD_MIC_ASKING` until the answer.
- If the player declines, the state becomes `WWHD_MIC_DENIED` and the port turns the option off again
  with a short notice. The player can allow the microphone in the system settings and turn it on again.
- The port analyses the sound for its pitch and drops it: nothing is stored, written, logged or sent,
  and your mod never gets samples.
- `wwhd_mic_pitch(out)` writes the newest window's (about 21 ms) pitch in Hz to `out[0]` (0: none),
  its loudness (0–1) to `out[1]` and its clarity (0–1) to `out[2]`, and returns a window count: a new
  value means a new window. `wwhd_mic_state()` is `WWHD_MIC_CLOSED`, `_ASKING`, `_OPEN`, `_DENIED` or
  `_UNAVAILABLE`.
- Call `wwhd_mic_close()` when you stop listening (the port also closes it when the option goes off).
  Keep it closed when nothing listens: QoL extras opens it once when the player turns the option on
  (for the question), then only while Link conducts.

```json
{"id": "sing", "type": "bool", "name": "Sing the songs", "default": false, "live": true, "permission": "microphone"}
```

```c
f32 p[3];
static unsigned long long last;
if (wwhd_mic_state() == WWHD_MIC_OPEN) {
    unsigned long long n = wwhd_mic_pitch(p);
    if (n != last && p[0] > 0 && p[1] >= 0.02f && p[2] >= 0.6f) { /* a clear note at p[0] Hz */ }
    last = n;
}
```

For scripted tests, `WWHD_TEST_MIC=from-to:hz,...` (TV frames) feeds tones without a device or a
question, and `WWHD_TEST_MIC_DENY=1` answers the question with "declined".

## Port settings

`wwhd_setting_get(key, type, buffer, capacity)` returns the bytes written, or 0 for an unknown key,
wrong type or too small buffer (the buffer stays unchanged). Pass a null buffer and 0 to get the
size. Strings include the NUL; numbers are big-endian. Mods never change settings.
`wwhd_setting_changed(key)` returns a revision that changes when the value changes; read settings
once per logic step.

| Key | Type | Values |
| --- | --- | --- |
| `input.face_layout` | STRING | `position`, `labels`, `custom` |
| `input.controller_mode` | STRING | `gamepad`, `pro` |
| `game.language` | U32 | Wii U language code 0–11 in use (set at startup) |
| `game.build` | STRING | `USA`, `EU` |
| `display.drc_mode` | STRING | `window`, `pip`, `auto`, `off`, `gamepad` |
| `display.aspect` | F64 | aspect ratio of the current frame |
| `render.interp_fps` | U32 | frame rate shown: 30, the interpolation rate, or 60 in true 60 mode |
| `render.true60` | BOOL | 0 or 1 |

```c
static char layout[16];
if (wwhd_setting_get("input.face_layout", WWHD_SETTING_STRING, layout, sizeof layout)) {
    /* layout holds the current preset name */
}
```

## Drawing on screen (HUD API v2)

Register one draw callback with `wwhd_hud_register(draw, screen)` from a game hook (a null callback
unregisters it). The port calls `draw(list)` after each logic step; record elements with
`wwhd_hud_emit(list, &element)`. The list is shown when the callback returns and stays on screen
until the next step. Only read game state in the callback.

```c
static wwhd_hud_element box;
static void draw(u32 list) {
    box = (wwhd_hud_element){
        .kind = WWHD_HUD_RECT, .anchor = WWHD_HUD_TOP_LEFT,
        .x = 20, .y = 96, .w = 160, .h = 40,
        .thickness = 1, .u1 = 1, .v1 = 1, .rgba = 0x204060C0
    };
    wwhd_hud_emit(list, &box);
}
WWHD_HOOK(WWHD_ADDR_daPy_Execute, register_box, (void* link)) {
    (void)link;
    wwhd_hud_register(draw, WWHD_HUD_BOTH);
}
```

- **Elements** (`wwhd_hud_element`): filled or outlined rectangles and circles, lines, text and
  images. Set `thickness`, `u1` and `v1` to 1 unless you need other values. Circle `size` is the
  radius, text `size` the height. Images support UV subrects, tint, rotation and alpha or additive
  blending. Keep elements and text in static storage or your heap.
- **Coordinates:** TV 1280 × 720, GamePad 854 × 480 (`WWHD_HUD_BOTH` uses TV coordinates, scaled).
  Anchors keep elements at the screen edges on wide screens.
- **Images:** `wwhd_hud_texture(WWHD_HUD_PACKAGE, "assets/x.png")` loads your package's own art;
  `WWHD_HUD_DATA` loads from your mod's data folder (for images your setup tool prepared). Keep the
  handle and free it with `wwhd_hud_release`. After a state load the handles are gone: compare
  `wwhd_hud_epoch()` in each callback and reload when it changes.
- **The game's own HUD art:** `wwhd_hud_texture(WWHD_HUD_GAME, "<Layout>/<texture>")` loads a texture
  of the game's 2D layouts (map frames, icons, markers) from the player's own game files, for example
  `"Map_00/MapShip_00^t"`. Add `#<material>` to colour it the way the game does, and `@2`, `@3` or `@4`
  for a sharper enlarged copy. `wwhd_game_pane_get("layout-pane:<Layout>/<pane>", &pane)` reads a pane's
  place, size, rotation and alpha; the key `layout-texture:<Layout>/<texture>` checks that a texture
  exists without loading it. Missing names return 0, with the reason in the Mods tab. Ship only the
  names, never the pictures. Example: [`hd-minimap/`](../../hd-minimap/).
- **Clipping:** `wwhd_hud_clip(list, &element)` records `WWHD_HUD_CLIP_PUSH` (a screen-aligned
  rectangle from `x/y/w/h` and `anchor`, intersected with the parent clip) and `WWHD_HUD_CLIP_POP`.
  Up to 16 nested clips; an unbalanced callback drops the list.
- **Limits:** per list 1024 elements and 64 KiB of text; per mod 32 images and 16 MiB of decoded
  pixels. An invalid element (bad UTF-8, bad handle, non-finite numbers, over the limits) drops the
  whole list, with a message in the Mods tab.

It draws on Metal and Vulkan, on the TV picture and the GamePad screen; the settings overlay stays
on top. Positions are not interpolated between logic steps.

## PCM audio streams

Mods may play original synthesized audio: `wwhd_audio_open(48000, channels)` (one or two channels),
`wwhd_audio_submit(handle, samples, frames, channels)`, `wwhd_audio_available(handle)` and
`wwhd_audio_close(handle)`. Samples are signed 16-bit, interleaved and big-endian (the game CPU's
order); the host copies them before returning. Only 48000 Hz is supported.

- Each stream holds 8192 frames; each submission is at most 2048 frames; four streams per mod.
- Submit returns the accepted frames (zero when full). Errors: `WWHD_AUDIO_INVALID` (-1),
  `WWHD_AUDIO_BUSY` (-2: retry later without advancing your synthesis), `WWHD_AUDIO_QUOTA` (-3).
- Compare `wwhd_audio_epoch()` before using handles: loading a full save state or an output device
  change clears all streams. Recreate them and resume from your own musical state.

## Trust, save states and limits

- **Trust.** A code mod becomes native code inside the game process. Its memory accesses stay inside
  the game's memory and it reaches the system only through the services above, but that is not a
  sandbox: it can crash the game or damage saves. Enabling a code mod asks once; a changed package
  asks again.
- **Save states.** Full save states include the mods' memory and heaps. Loading a state made with
  different mods shows a warning and continues. Files a mod writes itself are not part of save
  states.
- **Limits.** Changing enabled mods or their options needs a restart, except options marked
  `"live": true`. Mods live in a 16 MiB region of
  game memory; each mod gets its own part. No events or exports between mods yet. Instructions the
  translator doesn't support are reported when the mod is built.

Next: [Story mods](story.md).
