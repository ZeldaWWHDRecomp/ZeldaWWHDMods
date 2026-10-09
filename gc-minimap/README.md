# GameCube sea minimap — SDK v2 guest port

This is the current guest-mod implementation. The v0.2.2 built-in prototype is preserved in `../legacy/gc-minimap/` for comparison.

Install through the port's Mods catalogue, enable code mods when offered, select your own GameCube Wind Waker RVZ, ISO or full extracted game folder, run **Prepare local island maps**, build, then enable and restart. Preparation accepts USA, European and Japanese GameCube data; the running HD installation may be USA or European. RVZ is read directly in bounded chunks; no ISO conversion or other programs are used. The decomp’s executable/REL-only extraction is insufficient because it contains no maps.

Preparation uses Python's standard library. Zstandard-compressed RVZ files require Python 3.14 or newer; older bundled port interpreters still need an update or a suitable Python configured for setup. Other supported RVZ codecs use the older standard-library decoders. It reads your selected source and writes flat files into this mod's data folder. Derived island maps are never part of the distributed package. Package frame, heading marker and arrows are original artwork generated reproducibly by `generate_art.py`.

The panel shows on the TV during Great Sea gameplay, hides during events and transitions, and hides sectors without verified map bounds. Position, size, opacity and vector/original map style are configurable. Arrows are decorative. There are no interior or dungeon maps and no separate boat marker. Map preparation uses verified GameCube map extents; the old HD recoloured fallback is not used. Exact vector raster and panel parity with the legacy version remains under verification.

This implementation requires the HUD-capable SDK v2 port and the curated declarations pinned in `../sdk.json`; the numeric minimum identifies the base release and does not imply those declarations ship in an older binary. The lead must publish the port SDK commit before CI can fetch the pin. The port handles USA-to-European address translation during guest preparation. Enabling and disabling take effect on restart.

Synthetic tests: `python3 -m unittest discover -s gc-minimap/tests`. Asset format fixtures are authored in tests; no game data is included.
