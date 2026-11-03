# Setup and toolchain

## Enabling code mods in the port

Code-mod support is **off** by default. Content replacements, settings presets and Cemu graphics
packs don't need it.

The simplest way: switch the code mod on in **Settings (F1) → Mods**. Its review says that code-mod
support is added; after **Install** (or **Switch on**) the game code is rebuilt once (a few minutes;
the progress view shows it), the game restarts by itself and the setup continues until the mod is
on. A failed step shows one sentence, **Show details** and **Try again**; Cancel keeps the current
build.

To switch support on or off by hand: **Mods → More → Code-mod support → Use code mods** (on macOS
the Gameplay menu has the same option), confirm the rebuild, then **Restart now** (or later). Both
variants are cached, so switching back to one you built before is quick. Keep the complete release
folder and the compiler setup downloaded: building code mods uses those local tools.

Switching code mods on or off and changing their options take effect after a restart, except options
the package marks live. Code mods run on macOS, Windows, Linux and Android (port 0.4.0 and later). On
Android the setup app's **Code mods** switch (or **Use code mods** in the game, which starts the same
rebuild) builds the game code with code-mod support on the device, about as long as the first setup;
close and reopen the app afterwards. Each mod is then translated and built on the device. Mod
preparation tools written in Python run there too; other programs in a package don't.

To see untested mods of this catalogue, turn on **Show experimental mods**. The port labels them,
warns before enabling one and backs up your saves first.

### GameCube files used by mods

A mod that needs them asks for your GameCube Wind Waker disc image (`.iso`, `.gcm` or `.rvz`) or
extracted game folder; **Mods → More → GameCube game for mods** changes it later. These files stay on
your computer, and a mod's setup writes what it derives from them only to that mod's data folder.

## The toolchain

You need clang with the PowerPC backend, and lld. You don't need devkitPPC.

- **macOS:** `brew install llvm lld`, then use `$(brew --prefix llvm)/bin/clang` and the
  `ld.lld` of your lld installation. Apple's own clang has no PowerPC target.
- **Windows:** install MSYS2, open its CLANG64 shell and run
  `pacman -S mingw-w64-clang-x86_64-clang mingw-w64-clang-x86_64-lld`. The official Windows LLVM
  installer and llvm-mingw may lack the PowerPC backend.
- **Linux (Debian/Ubuntu):** `sudo apt install clang lld`. Check that `clang --print-targets` lists
  PowerPC.

For the repository's checks you also need Python 3.14 (its `compression.zstd` module is used by the
minimap tests) and git.

## The SDK headers

The guest headers are `runtime/guest/include` in a checkout of the
[port repository](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp), or `sdk/guest/include` in a
port release folder. This repository pins the port commit its catalogue is built with in
[`sdk.json`](../../sdk.json); use a clean checkout of exactly that commit for the full package build:

```sh
git clone https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp.git ../port
git -C ../port checkout "$(python3 -c 'import json;print(json.load(open("sdk.json"))["commit"])')"
```

## A first build

```c
#include "wwhd_guest.h"
#include "wwhd/functions.h"

WWHD_HOOK(WWHD_ADDR_daPy_Execute, on_link_step, (void* link)) {
    static u32 steps;
    (void)link;
    if (++steps == 1) wwhd_log("Link's first logic step");
}
```

```sh
SDK=../port/runtime/guest/include
mkdir -p build/local/my-mod
clang --target=powerpc-unknown-eabi -mcpu=750 -O2 -ffreestanding -fno-builtin -nostdlib \
  -fno-jump-tables -ffunction-sections -fdata-sections -I"$SDK" -Imy-mod/src \
  -c my-mod/src/mod.c -o build/local/mod.o
ld.lld -m elf32ppc -r build/local/mod.o -o build/local/my-mod/mod.elf
```

- `-mcpu=750` keeps to the game CPU's instructions.
- Link with `-r` only: the ELF stays relocatable and the port picks its address.
- `-m elf32ppc` is needed on Windows, where MSYS2's lld defaults to PE.
- Keep build output in `build/` (ignored): compiled files are never committed.

Copy `manifest.json`, `README.md`, `LICENSE` and any `assets/` next to the ELF in
`build/local/my-mod`, then install that folder in the port: **Settings (F1) → Mods → Installed
packages → Choose folder**, confirm trust, complete the setup, enable the mod and restart. Build or
load errors appear in the package's details in the Mods tab.

Next: [Packages and the manifest](packages.md).
