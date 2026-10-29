# Wind Waker HD guest mods

## Story quests (port 0.3.0)

With port 0.3.0, mods can tell new stories in the game's own way: letters by Rito mail, treasure
charts and quests that show up in a notebook of their own.

- **The Story Notebook:** a storyteller's letter brings Link a notebook that lists every mod quest:
  what to do next, where, and how far you got. **Show on chart** marks a quest on the sea chart. It
  comes with a first small quest on Windfall Island and installs with the port's setup.
- **Call of the Sky:** an optional song quest on Dragon Roost Island, ending with a ride across the
  Great Sea (experimental).
- **The Old map:** a sailor's letter brings a treasure chart. Open it in the MAP screen, follow the
  ring of light, salvage the chest and find the red circle on your sea chart (experimental).

Modders: the [Mod SDK guide](docs/sdk/README.md) explains letters, charts, items and quests, with
these packages as [worked examples](docs/sdk/examples.md).

## Mods

**Writing a mod?** The [Mod SDK guide](docs/sdk/README.md) has everything you need: setup and the toolchain, the package layout, manifest fields (letters, charts, items, quests, the notebook), the C API, art and licence rules, testing and submitting, and worked examples. [Creating a mod](docs/creating-a-mod.md) describes this repository's build and submission workflow.

SDK v2 guest mods for [ZeldaWWHDRecomp](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp). Install through **Mods → Browse**, complete the package's setup steps, enable code mods when offered, then enable and restart. Guest panels use the port's HUD API on Metal and Vulkan.

| Mod | Current status | Behaviour |
| --- | --- | --- |
| [Heart ticker](heart-ticker/) | Existing guest example | Hearts tick down to half a heart and refill. |
| [GameCube minimap](gc-minimap/) | Guest implementation; 30 fps gameplay verified | Position and heading on locally prepared sea and current-room dungeon maps. |
| [Call of the Sky](dragon/) | Experimental guest implementation; gameplay verification pending | Optional Dragon Roost song quest and Valoo ride, with its quest in the story notebook. |
| [The Story Notebook](story-notebook/) | The story system's required package, installed with the port's setup (port 0.3.0) | The storyteller's letter brings the story notebook, with a first small quest. |
| [Old map](story-chart/) | Experimental story example | A treasure chart sent in a letter, with no quest: salvage its chest like the game's treasures. |
| [Letter from a storyteller](rito-letter/) | Experimental story example | A letter sent from code by the Rito postbox. |

Experimental mods (`"experimental": true`) are untested and may break the game or a save. The port lists them only with **Show experimental mods** on and backs up the player's saves before one is enabled.

The v0.2.2 built-in sources are preserved under `legacy/`. Their environment switches and patches apply only to those prototypes. Current guest packages use the mod manager and take effect at restart; options a package marks live follow at once. Guest mods run on Android too (port 0.4.0 and later): the setup app's **Code mods** switch builds the game with code-mod support, and each mod is translated and built on the device when the game starts.

No game data, keys, saves or state files belong in this repository, packages or CI artifacts. Map images are generated from each player's own game files into the mod's data folder, outside Git. Original package artwork is either generated from reviewed source scripts or committed as licensed original artwork listed in the mod's `assets/ASSETS.md`.

## Testing and publication

`main` is published: players load its `index.json`. `devel` is testing. Testers set:

```sh
WWHD_MOD_CATALOGUE=https://raw.githubusercontent.com/ZeldaWWHDRecomp/ZeldaWWHDMods/devel/index.json
```

`catalogue.json` holds reviewed source metadata; CI generates package hashes and absolute URLs. `sdk.json` pins the exact public SDK commit. Do not interpret the base release number as proof that an older binary includes the required SDK services.

Against a clean checkout of that SDK, with PowerPC-capable clang/lld:

```sh
python3 tools/guard.py . --sdk path/to/pinned-port
python3 tools/package.py --sdk path/to/pinned-port --out build/packages \
  --base-url https://github.com/ZeldaWWHDRecomp/ZeldaWWHDMods/releases/download \
  --channel devel
```

The source checkout must also be clean. The hosting base is a single `MOD_PACKAGE_BASE` CI variable (or workflow input). Immutable assets live under `devel-<full-source-sha>/` or `main-<full-source-sha>/`; testing builds cannot overwrite published packages. For local tests, use the generated index and a local HTTP fixture serving the package URLs, or rewrite a copy to local paths. Never commit local paths or hand-edited download hashes.

The workflow builds artifacts only. The maintainers upload and verify immutable published-channel assets, create the generated-index metadata commit on the tested source, and fast-forward `main` directly to that commit. It must not briefly expose testing URLs on `main`. See [the promotion checklist](docs/review-checklist.md) for the exact source-versus-index commit procedure.

Outside contributions are source-only PRs into `devel`. Read [CONTRIBUTING.md](CONTRIBUTING.md), including setup-tool restrictions and required review. The maintainers require reviews and checks and allow no force pushes; nothing publishes automatically.
