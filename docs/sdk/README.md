# Mod SDK guide

Everything you need to write a code mod for the Wind Waker HD port and submit it to this catalogue.
Code mods are written in C, compiled for the game's own CPU (32-bit big-endian PowerPC) and
translated to native code on the player's machine when they install the mod. One package works on
macOS, Windows and Linux, for the USA and European game.

| Page | What it covers |
| --- | --- |
| [Setup and toolchain](setup.md) | enabling code mods in the port, installing clang/lld, a first build |
| [Packages and the manifest](packages.md) | the package layout in this repository, every manifest field, the catalogue entry |
| [The C API](c-api.md) | hooks and replacements, game functions and data, host services, HUD drawing, audio, limits |
| [Story mods](story.md) | letters, treasure charts, items, quests, the story notebook, talking to residents, the NPC catalogue |
| [Art, layouts and licences](art.md) | image and model formats, sizes, the Story Notebook's look, the licence rules for assets |
| [Testing and submitting](testing.md) | building and installing locally, the repository's checks, the pull request |
| [Worked examples](examples.md) | the Old map, Call of the Sky, the Story Notebook and QoL extras, explained |

The port's own reference, with the newest services first, is the
[SDK v2 guide in the port repository](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp/blob/devel/docs/mod-sdk-v2.md).
The repository's rules are in [CONTRIBUTING.md](../../CONTRIBUTING.md) and
[Creating a mod](../creating-a-mod.md).

## Port versions

| Feature | Port version |
| --- | --- |
| Code mods, hooks, host services, HUD drawing | 0.2.11 |
| Region-neutral `game_id` `"wwhd"` (mod manager 1.5) | 0.2.11 |
| Story system: letters, charts, quests, the story notebook with its open icon and page layout | 0.3.0 |
| Letters declared in the manifest (`"letters"`), with no quest and no code | 0.3.0 |
| Mod items (`"items"`, `wwhd_item_*`): icons in the game's item pictures, the Delivery Bag's extra pages, use on X, Y or R | 0.3.0 |
| Treasure charts like the game's: the light ring, the crane, the chest with a game item or a mod item, the X at its spot in the chart view | 0.3.0 |
| Quests shown on the chart before they start (step 1's place) | 0.3.0 |
| Live options, input filters, touches and pinches, points on the picture, the microphone (mod manager 1.6) | 0.5.0 |
| The game's own HUD art (`WWHD_HUD_GAME`, `wwhd_game_pane_get`) | 0.5.0 |

Port 0.3.0 is the first release with story mods: everything in [Story mods](story.md) needs it. Set
your catalogue entry's `port_versions.minimum` to the version that has every feature your mod uses.

## Ground rules

- Never use or ship game material: no extracted, traced or converted textures, models, fonts, text,
  sounds or screenshots, no saves and no keys. Mods use the game the player owns at run time.
- Use SDK declarations to hook or call game functions; don't copy decompiled implementations.
- Name game characters in manifests by the neutral ids of the NPC catalogue (`windfall-um1`), not by
  their names.
- Write your own texts.
