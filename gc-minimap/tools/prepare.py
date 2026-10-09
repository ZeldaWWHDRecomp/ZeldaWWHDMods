"""Prepare local-only GC sea maps; no game-derived content is distributed."""
import json
import struct
import formats
import vector
import rvz
import safe_io


def room_archive(context,files,room):
    relative=f'res/Stage/sea/Room{room}.arc'
    if files is None:
        try: return context.read_input(relative)
        except FileNotFoundError: return context.read_input('files/'+relative)
    if relative not in files and 'files/'+relative in files: relative='files/'+relative
    if relative not in files: raise ValueError('Required sea room is missing from GameCube dump')
    offset,size=files[relative]
    return context.read_game(offset,size)


def prepare(context):
    # File inputs are ISO; directory inputs expose only selected relative files.
    # Ask the capability helper to classify the source without exposing its path.
    # Clear readiness/bounds before reading the new input, including malformed ISO.
    # Existing textures cannot revive a sector whose bounds were not reverified.
    context.write_data('maps-ready.json',b'{"ready":false}\n')
    for room in range(1,50): context.write_data(f'room-{room:02}.bounds',bytes(16))
    disc=rvz.disc_reader(context) if context.is_disc() else context
    files=formats.iso_files(disc) if context.is_disc() else None
    prepared=[];hidden=[]
    for room in range(1,50):
        archive=formats.rarc_files(room_archive(disc,files,room))
        candidates=[name for name in archive if name=='dat/room.dzr' or name=='dzr/room.dzr' or name=='room.dzr']
        if len(candidates)!=1 or 'dat/s128.bti' not in archive:
            hidden.append(room);continue
        try: bounds=formats.bounds_from_dzr(archive[candidates[0]])
        except ValueError:
            hidden.append(room);continue
        width,height,rgba=formats.decode_bti(archive['dat/s128.bti'])
        raster,svg,receipt=vector.trace(width,height,rgba)
        context.write_data(f'room-{room:02}-original.png',formats.png_rgba(width,height,rgba))
        context.write_data(f'room-{room:02}-vector.png',formats.png_rgba(1024,1024,raster))
        context.write_data(f'room-{room:02}-vector.svg',svg)
        context.write_data(f'room-{room:02}-vector.json',receipt)
        context.write_data(f'room-{room:02}.bounds',struct.pack('>4f',*bounds))
        prepared.append(room)
    if not prepared: raise ValueError('No sectors with verified map bounds; selected dump is not ready')
    result={'format_version':1,'ready':True,'rooms':prepared,'hidden_without_verified_bounds':hidden,'derived_assets_local_only':True}
    context.write_data('maps-ready.json',json.dumps(result,sort_keys=True,indent=2).encode()+b'\n')
    return result


if __name__=='__main__':
    print(json.dumps(prepare(safe_io.arguments()),sort_keys=True))
