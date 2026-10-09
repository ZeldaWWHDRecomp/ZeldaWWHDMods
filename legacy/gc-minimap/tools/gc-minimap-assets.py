#!/usr/bin/env python3
"""Prepare the GC minimap's local chart cache from the player's own game files.

The cache holds data derived from the game (HD chart textures, room bounds and,
with --gc, the original GameCube sea maps and their vector traces). It is for the
player's own machine only: the output folder must be outside any Git checkout,
and nothing from it may be committed or shared.

usage:
  gc-minimap-assets.py --port PORT --game GAME --output CACHE [--gc DISC --dtk DTK]

  --port    a checkout of the WWHD port (its tools/shaderprep.py reads the game's archives)
  --game    the dumped Wii U game folder (the one with code/ and content/)
  --output  the cache folder; the mod reads ~/Library/Application Support/wwhd/gc-minimap
            by default, or the folder named by WWHD_GC_MINIMAP_CACHE
  --gc      optional: the player's own GameCube disc image (ISO/RVZ) for the original maps
  --dtk     decomp-toolkit (dtk) executable, required with --gc to read the disc
"""
import argparse, pathlib, sys, re, json, hashlib, struct, subprocess


def inside_git(path):
    return any((parent / '.git').exists() for parent in (path, *path.parents))


p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
p.add_argument('--port', required=True, help='WWHD port checkout (for tools/shaderprep.py)')
p.add_argument('--game', required=True, help='dumped Wii U game folder')
p.add_argument('--output', required=True, help='cache folder outside any Git checkout')
p.add_argument('--gc', help="optional: the player's own GameCube disc image (ISO/RVZ)")
p.add_argument('--dtk', help='decomp-toolkit (dtk) executable, required with --gc')
a = p.parse_args()
if a.gc and not a.dtk:
    p.error('--gc needs --dtk (decomp-toolkit) to read the disc image')
here = pathlib.Path(__file__).resolve().parent
out = pathlib.Path(a.output).expanduser().resolve()
game = pathlib.Path(a.game).expanduser().resolve()
if inside_git(out) or out.is_relative_to(game):
    raise SystemExit('The asset cache must be outside any Git checkout and outside the game folder')
sys.path.insert(0, str(pathlib.Path(a.port).expanduser().resolve() / 'tools'))
from shaderprep import walk

archive = game / 'content/Common/Pack/permanent_2d_UsEnglish.pack'
out.mkdir(parents=True, exist_ok=True)
found = {}
for name, data in walk(archive.read_bytes(), str(archive)):
    m = re.search(r'/TreMap_00.szs/timg/r([0-9a-f]{2})_col_([0-3])\^t.bflim$', name)
    if not m:
        continue
    room = int(m[1], 16); layer = int(m[2])
    (out / f'room-{room:02}-{layer}.bflim').write_bytes(data)
    found.setdefault(room, []).append(layer)
if any(sorted(found.get(room, [])) != [0, 1, 2, 3] for room in range(1, 50)):
    raise SystemExit('Incomplete sea chart set; cache is not ready')
(out / 'manifest.json').write_text(json.dumps({'source_sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
                                               'rooms': 49, 'layers_per_room': 4}, indent=2) + '\n')
print('Prepared 49 charts, 196 local texture layers:', out)

# Original room metadata supplies bounds; do not invent chart coordinates.
bounds = {}
paths = list((game / 'content/Common/Stage').glob('sea_Room*.szs'))
paths += list((game / 'content/Common/Pack').glob('szs_permanent*.pack'))
for archive in paths:
    for name, data in walk(archive.read_bytes(), str(archive)):
        m = re.search(r'/sea_Room(\d+)\.szs/Room\d+\.bfres$', name)
        if not m or data[:4] != b'FRES':
            continue
        room = int(m[1]); rel = lambda o: o + struct.unpack_from('>i', data, o)[0]
        if not struct.unpack_from('>I', data, 0x4c)[0]:
            continue
        table = rel(0x4c); count = struct.unpack_from('>I', data, table + 4)[0]
        for i in range(count):
            node = table + 8 + (i + 1) * 16; start = rel(node + 8)
            if data[start:data.index(b'\0', start)] != b'room.dzr':
                continue
            file = rel(node + 12); start = rel(file); size = struct.unpack_from('>I', data, file + 4)[0]
            dzr = data[start:start + size]
            for j in range(struct.unpack_from('>I', dzr, 0)[0]):
                tag, count2, offset = struct.unpack_from('>4sII', dzr, 4 + j * 12)
                if tag == b'2DMA' and count2:
                    values = struct.unpack_from('>4f', dzr, offset)
                    bounds[room] = values
                    (out / f'room-{room:02}.bounds').write_text(' '.join(map(str, values)) + '\n')
print('Original room bounds found:', len(bounds))

if a.gc:
    from gc_minimap_bti import decode_bti
    from PIL import Image
    disc = pathlib.Path(a.gc).expanduser().resolve()
    imported = []
    for room in sorted(bounds):
        target = out / f'room-{room:02}.bti'
        source = str(disc) + f':files/res/Stage/sea/Room{room}.arc:dat/s128.bti'
        proc = subprocess.run([a.dtk, 'vfs', 'cp', '-q', source, str(target)],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if proc.returncode:
            continue
        width, height, rgba = decode_bti(target.read_bytes())
        (out / f'room-{room:02}.gc-rgba').write_bytes(b'GCM1' + struct.pack('>II', width, height) + rgba)
        # Deterministic vector-derived cache; the tracer validates the original
        # geography before it writes any vector output.
        png = out / f'room-{room:02}-gc-original.png'
        Image.frombytes('RGBA', (width, height), rgba).save(png)
        subprocess.run([sys.executable, str(here / 'gc-minimap-vector.py'), str(png), str(out)],
                       check=True, stdout=subprocess.DEVNULL)
        imported.append(room)
    print('Original GC maps imported and vector-traced locally:', imported)
