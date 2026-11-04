# TWWHD Randomizer core mod — experimental

This is a source port of **TWWHD Randomizer 1.2.0** through the guest mod SDK.
It runs on the **unmodified USA game**, using seed data and changed content from
the player's official randomized output. The randomized RPX is validated and
read as input; its executable code is not installed or executed by this route.

The core has source implementations, data preparation, private package building,
SDK integration and private package generation. It remains experimental: **no full
randomized playthrough has been verified**. Windows and Android end-to-end use,
real boss/NPC/dungeon encounters, and the gameplay effects of every supported
option still need validation. USA is the only supported game build.

For normal play, use the port's [randomized-game setup](https://github.com/ZeldaWWHDRecomp/ZeldaWWHDRecomp/blob/devel/docs/randomizer-experiment.md).
That route recompiles the randomized game directly. This mod is a separate
experimental source project, is not in the catalogue, and is not ready for
ordinary installation. **Saves do not transfer between the two routes**, even
when they use the same seed. Neither route converts the other's saves.

Use only a trusted official port checkout for development: `WWHD_PORT_SOURCE` executes Python tools from that checkout. Seed packages contain player-owned content and must stay outside this source project.

The checked-in `manifest.template.json` is deliberately not installable. A private build fills its validated seed identity and emits the actual manifest. See [development instructions, supported policies and testing limits](docs/DEVELOPMENT.md).

The source is covered by [MPL-2.0](LICENSE); upstream randomizer contributions retain their [MIT licence and attribution](LICENSE-TWWHD-Randomizer.txt). No original game code, game assets or seed packages are distributed here.
