# Creating a mod for The Wind Waker HD port

This guide is for modders who want to write a mod for the native Wind Waker HD port
([ZeldaWWHDRecomp](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp)) and publish it through
this repository's mod catalogue.

## What kind of mod?

| Kind | What it is | Needs code mods turned on |
| --- | --- | --- |
| `settings` | A preset of the port's built-in options | no |
| `content` | Replacement game resources (textures, text, models) made by you | no |
| `cemu` | A Cemu graphics pack (rules.txt, texture/shader replacements) | no |
| `guest` | **Code mod**: C code compiled for the game's PowerPC CPU, hooking game functions | yes |

Content, settings and Cemu packs are described in the port's
[mod manager guide](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp/blob/devel/docs/mod-manager.md).
The rest of this guide is about **code mods**, the kind most mods in this repository are. The
full API reference is the port's
[Mod SDK v2 guide](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp/blob/devel/docs/mod-sdk-v2.md).

## The rules (read first)

Pull requests that break these are closed without review:

- **No game files or anything taken from them**: no textures, models, maps, sounds, text, code,
  disc images, saves or keys, not even small excerpts or converted copies. If your mod needs game
  data, a setup tool builds it on the player's machine from their own copy of the game (see
  [Mods that need game data](#mods-that-need-game-data)).
- **Source only.** No compiled files (`.elf`, `.o`, `.dll`, `.so`, `.dylib`, `.exe`), archives,
  packages or encoded blobs. The catalogue build compiles your mod from source; you never upload
  the package or its checksum.
- **Your own art.** Images must be your own work. Prefer a small script that generates them.
- **No decompiled game code copied into your mod.** Use the SDK's headers to call and hook game
  functions.
- **A licence** (`LICENSE` in your mod folder) that allows us to build and distribute the mod.
- **Setup tools** (scripts that run on players' machines) may only read the files the player
  picked and write into the mod's own data folder: no network access, no starting other programs.

## 1. Set up the tools

You need clang with the PowerPC backend, and lld:

- **macOS:** `brew install llvm lld` (Apple's own clang can't build for PowerPC).
- **Windows:** MSYS2, CLANG64 shell: `pacman -S mingw-w64-clang-x86_64-clang mingw-w64-clang-x86_64-lld make`.
- **Linux (Debian/Ubuntu):** `sudo apt install clang lld make`; check that `clang --print-targets` lists PowerPC.

You also need the port's SDK headers: `sdk/guest/include` in a release folder of the port, or
`runtime/guest/include` in a checkout of the port. To test your mod you need the port itself,
installed with your own game, with **Settings → Mods → Enable code mods** turned on.

## 2. Create the mod folder

One folder per mod, named like its ID (lowercase letters, digits, `-`, `_`, `.`):

```
my-mod/
  manifest.json     what the mod is, its options and setup steps
  mod.c             your code (more .c files are fine)
  README.md         what it does, how to use it, known limitations
  LICENSE           your licence
  assets/           optional: your own images (PNG) for on-screen drawing
  tools/            optional: setup tools (Python, standard library only)
```

`manifest.json`:

```json
{
  "format_version": 1,
  "id": "my-mod",
  "name": "My mod",
  "version": "0.1.0",
  "game_id": "wwhd-usa",
  "kind": "guest",
  "guest": {"api_version": 1, "elf": "mod.elf"},
  "author": "Your name",
  "description": "One sentence about what the mod does.",
  "options": [
    {"id": "speed", "type": "number", "name": "Speed", "default": 1, "min": 0.5, "max": 2}
  ],
  "setup": [
    {"id": "build", "type": "build_guest_mod", "title": "Build My mod for this installation"}
  ]
}
```

- `version` is `major.minor.patch`; raise it with every change you want players to get.
- `options` appear in the Mods panel; read them in code with `wwhd_config_*`.
- The `build_guest_mod` setup step builds your mod for the player's game version (USA or EU).
- `"game_id": "wwhd-usa"` is right for EU players too: the port translates USA addresses for EU.

## 3. Write the code

Hook a game function and read your option:

```c
#include "wwhd_guest.h"
#include "wwhd/functions.h"

WWHD_HOOK(WWHD_ADDR_daPy_Execute, on_link_step, (void* link)) {
    static u32 steps;
    if (++steps == 1)
        wwhd_log_float("my-mod: speed option", wwhd_config_float("speed", 1.0));
}
```

What you can use (details in the [Mod SDK v2 guide](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp/blob/devel/docs/mod-sdk-v2.md)):

- `WWHD_HOOK` / `WWHD_HOOK_RETURN` run before / after a game function; `WWHD_REPLACE` replaces
  it, and `WWHD_GAME_ORIGINAL` calls the game's own version from your replacement.
- `wwhd/functions.h` names every known game function (`WWHD_ADDR_<name>`), `wwhd/bindings.h`
  lets you call them (`wwhd_<name>(...)`), and `actor.h`, `link.h`, `camera.h`, `items.h`,
  `messages.h`, `save.h` give access to parts of game objects.
- Services: logging, your options, a heap, controller input, files in your mod's data folder,
  the logic step length, read-only port settings, and drawing on screen (text, shapes, your
  images; HUD API v1).

Good habits:

- Use `wwhd_logic_dt()` for anything time-based. The port can run at 30, 60, 120 or 240 fps, and
  a hook on a logic function runs once per game step, not once per frame.
- Keep hooks small; they run every step or every frame.
- Only one mod can replace a given function. Prefer hooks where you can.
- Don't cast numbers to pointers: that only works on the USA version. Use the SDK's names and
  `WWHD_GAME_DATA`.

The port's [`examples/guest-mods`](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp/tree/devel/examples/guest-mods)
are complete small mods to start from (`heart-ticker`, `addcalc-replace`, `hud-demo`,
`button-icons`), and [`heart-ticker/`](../heart-ticker/) in this repository is the packaged form.

## 4. Build and test locally

```sh
SDK=path/to/port/runtime/guest/include      # or <release folder>/sdk/guest/include
clang --target=powerpc-unknown-eabi -mcpu=750 -O2 -ffreestanding -fno-builtin -nostdlib \
  -fno-jump-tables -ffunction-sections -fdata-sections -I"$SDK" -c my-mod/mod.c -o build/mod.o
ld.lld -m elf32ppc -r build/mod.o -o my-mod/mod.elf
```

(macOS: use `$(brew --prefix llvm)/bin/clang` and `$(brew --prefix lld)/bin/ld.lld`.) Then in the
game: **Settings (F1) → Mods → Installed packages → Choose folder** → pick `my-mod/` →
**Install package**, enable it, confirm, run its setup step and restart. Your `wwhd_log` lines go
to the game's log (turn on **Settings → Graphics → Write a log file** to get `captures/wwhd.log`).

Don't commit `mod.elf` or `build/`: the catalogue build compiles the mod from your source.

To check the whole catalogue path, build the packages as CI does and point the port at your local
index:

```sh
python3 tools/test_guard.py path/to/pinned-port      # the port checkout pinned in sdk.json
python3 tools/package.py --sdk path/to/pinned-port --out build/packages
WWHD_MOD_CATALOGUE=build/packages/index.json  <start the port>
```

Test at least: 30 fps and 60 fps, Metal and Vulkan if you can, loading a save state, and the mod
turned off (the game must behave exactly as without it).

## Mods that need game data

A mod may need data from the game, for example map images. It must never ship that data.
Instead, its manifest declares setup steps that run on the player's machine:

```json
"setup": [
  {"id": "game", "type": "game_path", "game": "gc_usa", "title": "Choose your GameCube game"},
  {"id": "prepare", "type": "run_tool", "tool": "tools/prepare.py",
   "arguments": ["{game:gc_usa}", "{data}"], "outputs": ["map.png"],
   "title": "Prepare the map images"},
  {"id": "build", "type": "build_guest_mod", "title": "Build the mod"}
]
```

The player picks their own game file or folder; your tool reads it and writes its results into
the mod's data folder (`{data}`); your code loads them from there (`wwhd_file_read`,
`wwhd_hud_texture(WWHD_HUD_DATA, ...)`). The tool runs in the mod's data folder without a shell;
the player confirms it once, like code, and again whenever the package changes. Setup tools are
reviewed most strictly: standard-library Python only, read only what the player picked, write only
into `{data}`. Game sources: `gc_usa`, `gc_eur`, `gc_jpn`, `wiiu_eur`, `wiiu_jpn`.

## 5. Submit it

1. Fork this repository and add your mod folder on a branch.
2. Open a pull request into **`devel`** (not `main`). Describe what the mod does, what you
   tested (platforms, renderer, frame rates) and any known problems.
3. CI checks the rules above and builds your mod from source. A maintainer reviews the code,
   especially hooks, replacements, file access and setup tools.
4. After merging, the mod appears in the **development catalogue** for testing:
   `WWHD_MOD_CATALOGUE=https://raw.githubusercontent.com/ZeldaWWHDRecomp/ZeldaWWHDMods/devel/index.json`.
5. When it has been tested, maintainers publish it on **`main`**, the catalogue every player's
   port loads by default.

Updates work the same way: change the source, raise `version`, open a pull request into `devel`.
