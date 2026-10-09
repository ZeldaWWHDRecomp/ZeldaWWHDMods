# Creating a mod for The Wind Waker HD port

This guide describes source contributions to the
[Wind Waker HD mod catalogue](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDMods).
Read [CONTRIBUTING.md](../CONTRIBUTING.md) first. The port's
[SDK v2 guide](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp/blob/devel/docs/mod-sdk-v2.md)
contains the API reference; this page explains this repository's build and review flow.

## Scope and contribution rules

The port supports settings presets, content replacements, Cemu packs and guest code mods.
This repository's current catalogue packager discovers declared **guest** mods and builds
one PowerPC ELF per mod. Other package kinds need a separate supported packaging path;
adding a folder alone does not register a package.

Contributions contain readable sources, a manifest, README and distribution licence.
Do not commit compiled objects or libraries (`.o`, `.elf`, `.dll`, `.so`, `.dylib`, `.exe`),
ZIPs or other archives, encoded blobs, generated image files, game files, game-derived
assets, saves or keys. Even original PNGs must be generated from reviewed source during
packaging. CI builds the ELF, packages and checksums; contributors supply none of those.
Use SDK declarations to hook or call game functions; do not copy decompiled implementations.

Setup tools receive the strictest review because they execute on the player's machine.
They use permitted Python standard-library parsers, read selected inputs through reviewed
capabilities and write only to their mod's data directory. Networking, subprocesses,
dynamic execution and direct filesystem access are refused. Static checks support human
review; they are not an operating-system sandbox.

## 1. Set up the tools

Use clang with a PowerPC backend and lld, without a submitted Makefile or other build system:

- macOS: install LLVM and lld with Homebrew. Apple's clang lacks the PowerPC backend.
- Windows: use an LLVM toolchain with the PowerPC backend, for example MSYS2 CLANG64 clang/lld.
- Linux: install clang and lld; check `clang --print-targets` includes PowerPC.

The committed `sdk.json` pins the public port checkout used by catalogue CI. Use a clean
SDK checkout at that exact commit for the whole catalogue build. Guest headers live in
`runtime/guest/include` in a port checkout, or `sdk/guest/include` in a port release.
They are distinct from the runtime translation headers in `runtime/include` / `sdk/include`.
Do not update `sdk.json` in an ordinary mod contribution; maintainers review SDK changes separately.

For local setup/parser tests use Python 3.14 with `compression.zstd` available. The minimap
reader accepts ISO/extracted GameCube sources and reads RVZ directly, without conversion
programs. Zstandard-compressed RVZ needs that Python standard-library codec. Release setup
selects or provisions a capable interpreter through the trusted installer; setup scripts
cannot download their own dependencies. Existing installations may need the release's
`--repair-guest-python` setup operation. Native Windows/Linux release verification is a
separate requirement from package CI passing on Python 3.14.

To test a code mod, install the port with your own game and turn on
**Settings → Mods → Enable code mods**. Guest mods are currently unsupported on Android.

## 2. Create and register the mod

Use a lowercase ID beginning with a letter or digit, followed by letters, digits, `_` or `-`,
with at most 64 characters. The folder name and manifest ID must match.

```text
my-mod/
  manifest.json
  README.md
  LICENSE
  src/
    mod.c                 the single compiled translation unit
    helper.h              optional local declarations
    feature_impl.h        optional implementation included by mod.c
    feature.c             optional C helper included by mod.c
  generate_art.py          optional pure original-art generator
  tools/                  optional reviewed Python setup modules
```

The packager prefers `src/mod.c` and also accepts the older root-level `mod.c` layout.
It compiles **only that one translation unit**. Additional `.c` files are not compiled or
linked automatically: include needed implementation units from `mod.c`, or use implementation
headers, as [dragon](../dragon/src/mod.c) does. Keep these sources flat in the entrypoint's
directory. The package retains its `.c` and `.h` files under `src/`; arbitrary nested source
folders and independent build systems are not supported by this packager.

Start with [heart-ticker](../heart-ticker/) or the port's
[guest examples](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp/tree/devel/examples/guest-mods).
A minimal manifest is:

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
  "description": "One sentence describing the mod.",
  "options": [
    {"id": "speed", "type": "number", "name": "Speed", "default": 1, "min": 0.5, "max": 2}
  ],
  "setup": [
    {"id": "build", "type": "build_guest_mod", "title": "Build My mod for this installation"}
  ]
}
```

Add a matching entry to the root `catalogue.json` `mods` array. It is the authored discovery
list; the packager builds exactly those IDs, not every directory with a manifest. Copy an
existing entry's metadata shape and set your ID, name, description, authors, licences,
version, supported builds, requirements, minimum port version and setup list. The `id`,
`kind`, `version` and `setup` fields must match the manifest. Do not add `downloads`, URLs,
sizes or hashes, and do not edit generated `index.json`.

For original artwork, declare `"art_generator": "generate_art.py"` in that catalogue entry.
The reviewed pure Python module exports `generate()`, returning a dictionary of flat PNG
filenames to PNG bytes. The packager audits it, executes it during a trusted build and
places the results under `assets/`. See [the minimap generator](../gc-minimap/generate_art.py).
Commit the readable generator, not its PNG output. It cannot read files, use setup I/O,
import arbitrary local code, access the network or start other programs.

The `build_guest_mod` step translates the packaged PowerPC ELF into native code for the
player's installation. Catalogue CI performs the earlier guest C → PowerPC ELF compilation.
Modders use canonical USA SDK addresses for both USA and European installs; the port maps
supported symbols for Europe and refuses missing mappings. Keep `game_id` as `wwhd-usa`.
Increase `version` for each update intended for players.

## 3. Write the code

```c
#include "wwhd_guest.h"
#include "wwhd/functions.h"

WWHD_HOOK(WWHD_ADDR_daPy_Execute, on_link_step, (void* link)) {
    static u32 steps;
    if (++steps == 1)
        wwhd_log_float("my-mod: speed option", wwhd_config_float("speed", 1.0));
}
```

Use `WWHD_HOOK` / `WWHD_HOOK_RETURN` for entry/return hooks and `WWHD_REPLACE` when replacement
is needed. `WWHD_GAME_ORIGINAL` delegates from a replacement. Only one mod can replace a given
function. The SDK supplies named function/data declarations, actor and game-object headers,
logging, options, heaps, input, per-mod files, timing and HUD services.

Use `wwhd_logic_dt()` for elapsed time, since render and logic rates can differ. Keep frequent
hooks small. Use SDK names and `WWHD_GAME_DATA` for region-aware data references instead of
casting USA numeric addresses to pointers. See the SDK guide for the actual declaration/API
contracts and supported services.

## 4. Build and test locally

For a quick guest compile, keep all generated files in ignored build output:

```sh
SDK=path/to/port/runtime/guest/include
mkdir -p build/local
cp -R my-mod build/local/my-mod
clang --target=powerpc-unknown-eabi -mcpu=750 -O2 -ffreestanding -fno-builtin -nostdlib \
  -fno-jump-tables -ffunction-sections -fdata-sections -I"$SDK" -Imy-mod/src \
  -c my-mod/src/mod.c -o build/local/mod.o
ld.lld -m elf32ppc -r build/local/mod.o -o build/local/my-mod/mod.elf
```

On macOS, use `$(brew --prefix llvm)/bin/clang` and your installed lld's `bin/ld.lld`.
In the port choose **Settings (F1) → Mods → Installed packages → Choose folder**, select the
staged `build/local/my-mod` folder, install it, confirm trust, complete setup, enable it and
restart. Enabling code mods takes effect at restart. For generated art and complete metadata
validation, use the catalogue build below instead of the minimal staging example.

The full package build requires a **clean committed source checkout**, including no unrelated
untracked files, and a clean tracked SDK checkout at the exact `sdk.json` commit. Commit your
source first. CI runs trusted baseline guards before executing submitted code; ordinary PRs
cannot change SDK pins, delivery metadata, workflow/policy entrypoints or protected setup I/O.
Use [CONTRIBUTING.md](../CONTRIBUTING.md) and the
[maintainer checklist](review-checklist.md) for that review boundary.

```sh
python3 tools/guard.py . --sdk path/to/pinned-port
python3 tools/test_guard.py path/to/pinned-port
python3 tools/package.py --sdk path/to/pinned-port --out build/packages \
  --clang clang --lld ld.lld --channel devel \
  --base-url https://github.com/ZeldaWWHDRecomp/ZeldaWWHDMods/releases/download
python3 tools/validate.py build/packages --root-index catalogue.json
```

The source guard refuses `.elf` and `.o` inputs. Package validation permits the ELF produced
by the trusted build; that does not make compiled files valid contributions. Output ZIPs
contain compiled code and reviewed sources, plus original generated art where declared.

The generated local index still records **absolute HTTPS download URLs**, not local ZIP
paths. Setting `WWHD_MOD_CATALOGUE` to a local index changes where metadata is read, not where
its packages download from. For an offline install, choose the locally generated ZIP through
Installed packages; for Browse testing, use a maintainer-hosted development index whose
immutable assets have been uploaded and verified. Do not weaken URL validation for testing.

Test install/setup/enable/restart, disable/remove, USA/EU, 30/60 fps, Metal/Vulkan, save states
and recovery from setup errors. Confirm the disabled game behaves as without the mod. Record
unverified platforms/regions honestly; do not commit game-derived screenshots or fixtures.

## Mods that need game data

Declare selection and preparation before the installation build step. For the reviewed
minimap preparation service, the manifest shape is:

```json
"setup": [
  {"id": "game", "type": "game_path", "game": "gc_wind_waker", "title": "Choose your GameCube game"},
  {"id": "prepare", "type": "run_tool", "tool": "tools/prepare.py",
   "arguments": ["--source", "{game:gc_wind_waker}", "--data", "{data}"],
   "outputs": ["maps-ready.json"], "title": "Prepare local map images"},
  {"id": "build", "type": "build_guest_mod", "title": "Build the mod"}
]
```

`gc_wind_waker` accepts supported USA, European and Japanese GameCube sources, including
ISO/RVZ and extracted folders. Actual regional/game-data verification is separate from
header-format acceptance. Other selector IDs include `gc_usa`, `gc_eur`, `gc_jpn`,
`wiiu_eur` and `wiiu_jpn`; choose the selector supported by your pinned manager.

The audited minimap code imports `safe_io` and calls only zero-argument `safe_io.arguments()`
to obtain the original manager-provided context. Its public operations are bounded
`read_game(offset, size)`, selected-root `read_input(relative_path)`, `is_disc()` and
`write_data(flat_filename, bytes)`. Do not replace the context, access private fields or
import helper filesystem exports. The helper itself is a protected maintainer-reviewed
exception to the pure-parser policy. Adding another mod's I/O helper needs separate trusted
baseline integration; copying an unrestricted helper into a new contribution is not permitted.

The manager runs the reviewed command without a shell, and the player confirms the package's
trust dialog again when it changes. Derived data remains only in that mod's private data
folder. Guest code reads it through per-mod file services or
`wwhd_hud_texture(WWHD_HUD_DATA, ...)`. Include all imported local setup modules, bound malformed
input handling and output sizes, and never ship generated game assets in the package or CI.

## 5. Submit and publish

1. Fork the repository, add readable sources and the matching `catalogue.json` entry, and
   open a pull request into **devel**. Include licence, test evidence and known limitations.
2. Source-only CI checks the contribution with trusted baseline policy and builds packages
   using the pinned SDK and direct clang/lld commands. PR runs have read-only permissions,
   no secrets and no publishing. Maintainers review hooks, service access and the complete
   setup/generator source closure.
3. Maintainers publish verified testing artifacts and the generated devel index. Testers use
   `WWHD_MOD_CATALOGUE=https://raw.githubusercontent.com/ZeldaWWHDRecomp/ZeldaWWHDMods/devel/index.json`.
   A successful CI artifact alone is not a published catalogue update.
4. Only the maintainer promotes reviewed, tested source to **main**, which players load by default.

Package URLs identify the channel and full clean source commit, for example
`.../devel-<40-character-source-commit>/my-mod.zip`; main uses a separate `main-<source-commit>`
location. Existing published assets are immutable. The package build requires an absolute
HTTPS hosting base and generates the URL, size and hash from its own output.

Promotion is prepared before main moves: build main-channel packages from tested source S,
upload and verify them, then create metadata-only publish commit P descended from S containing
the generated main index. Fast-forward main directly to P in one ref move; never first expose
a devel index on main. Fast-forward devel to P before subsequent work. The source commit in
package URLs is S, while P records their delivery metadata. See the maintainer checklist for
ancestry checks and lead-controlled GitHub settings. Contributors never upload packages or
supply hashes, and CI does not perform publication.
