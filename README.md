# Wind Waker HD guest mods

SDK v2 guest mods for [ZeldaWWHDRecomp](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp). Install through **Mods → Browse**, complete the package's setup steps, enable code mods when offered, then enable and restart. Guest panels use the port's HUD API on Metal and Vulkan. The repository is currently private; the default raw catalogue URL returns 404 to players until the lead makes it public or chooses another host.

| Mod | Current status | Behaviour |
| --- | --- | --- |
| [Heart ticker](heart-ticker/) | Existing guest example | Hearts tick down to half a heart and refill. |
| [GameCube sea minimap](gc-minimap/) | Guest port in progress; verification pending | Position and heading on locally prepared Great Sea island maps. |
| [Call of the Sky](dragon/) | Guest implementation; gameplay verification pending | Optional Dragon Roost song quest and Valoo ride. |

The v0.2.2 built-in sources are preserved under `legacy/`. Their environment switches and patches apply only to those prototypes. Current guest packages use the mod manager and take effect at restart. Android guest mods remain unsupported.

No game data, keys, saves or state files belong in this repository, packages or CI artifacts. Map images are generated from each player's own game files into the mod's data folder, outside Git. Original package artwork is generated from reviewed source scripts.

## Testing and publication

`main` is published: players load its `index.json`. `devel` is testing. Testers set:

```sh
WWHD_MOD_CATALOGUE=https://raw.githubusercontent.com/ZeldaWWHDRecomp/ZeldaWWHDMods/devel/index.json
```

`catalogue.json` holds reviewed source metadata; CI generates package hashes and absolute URLs. `sdk.json` pins the exact public SDK commit. The latest pin includes new curated declarations and must be published by the lead before remote CI can fetch it. Do not interpret the base release number as proof that an older binary includes the required SDK services.

Against a clean checkout of that SDK, with PowerPC-capable clang/lld:

```sh
python3 tools/guard.py . --sdk path/to/pinned-port
python3 tools/package.py --sdk path/to/pinned-port --out build/packages \
  --base-url https://github.com/ZeldaWWHDRecomp/ZeldaWWHDMods/releases/download \
  --channel devel
```

The source checkout must also be clean. The hosting base is a single `MOD_PACKAGE_BASE` CI variable (or workflow input). Immutable assets live under `devel-<full-source-sha>/` or `main-<full-source-sha>/`; testing builds cannot overwrite published packages. For local tests, use the generated index and a local HTTP fixture serving the package URLs, or rewrite a copy to local paths. Never commit local paths or hand-edited download hashes.

The workflow builds artifacts only. The lead uploads and verifies immutable published-channel assets, creates the generated-index metadata commit on the tested source, and fast-forwards `main` directly to that commit. It must not briefly expose testing URLs on `main`. See [the promotion checklist](docs/review-checklist.md) for the exact source-versus-index commit procedure.

Outside contributions are source-only PRs into `devel`. Read [CONTRIBUTING.md](CONTRIBUTING.md), including setup-tool restrictions and required review. The lead configures required reviews/checks and no force pushes; nothing publishes automatically.
