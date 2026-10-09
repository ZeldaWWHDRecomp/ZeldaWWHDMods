#!/usr/bin/env python3
"""Opt-in real-game HUD smoke test. All saves/captures remain in a private output folder.

Synthetic maps exercise guest capture and HUD delivery; they do not prove real-map
registration, setup, catalogue lifecycle, or legacy visual parity.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time
import zipfile
import zlib


def synthetic_maps(data):
    def chunk(kind, value):
        return struct.pack('>I', len(value)) + kind + value + struct.pack('>I', zlib.crc32(kind + value) & 0xffffffff)
    pixels = b''.join(b'\0' + bytes((31, 201, 109, 255)) * 32 for _ in range(32))
    image = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 32, 32, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b'')
    for room in range(1, 50):
        gx, gz = (room - 1) % 7 - 3, (room - 1) // 7 - 3
        (data / ('room-%02d.bounds' % room)).write_bytes(struct.pack('>4f', gx * 100000 - 50000, gz * 100000 - 50000, gx * 100000 + 50000, gz * 100000 + 50000))
        (data / ('room-%02d-vector.png' % room)).write_bytes(image)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'game', 'save', 'sdk', 'package', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--chart-data', type=Path, help='Previously prepared private map data')
    source.add_argument('--synthetic-maps', action='store_true', help='Authored HUD smoke fixture only')
    parser.add_argument('--renderer', choices=('metal', 'vulkan'), required=True)
    parser.add_argument('--mode', choices=('30', 'interp60', 'true60'), default='30')
    parser.add_argument('--first-frame', type=int, default=3000)
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--max-game-sessions', type=int, choices=range(1, 5), default=4)
    args = parser.parse_args()
    args.out = args.out.resolve()
    repo = Path(__file__).resolve().parents[2]
    for checkout in (repo, args.sdk.resolve()):
        if args.out.is_relative_to(checkout):
            parser.error('Private outputs must stay outside source checkouts')
    if args.first_frame < 700:
        parser.error('Allow at least 700 frames for startup')
    if shutil.disk_usage(args.out.parent).free < 15 * 1024**3:
        parser.error('Free disk is below 15 GiB')
    sys.path.insert(0, str(args.sdk.resolve() / 'tools/bench'))
    from run_bench import other_games, other_benchmarks
    if len(other_games()) >= args.max_game_sessions or other_benchmarks():
        parser.error('Functional game limit reached or a benchmark is running')
    args.out.mkdir(exist_ok=False)
    manager = args.out / 'manager'
    package = manager / 'Mods/gc-minimap'
    package.mkdir(parents=True)
    with zipfile.ZipFile(args.package) as archive:
        for info in archive.infolist():
            path = Path(info.filename)
            if path.is_absolute() or '..' in path.parts or '\\' in info.filename or ':' in info.filename or (info.external_attr >> 16) & 0o170000 == 0o120000:
                raise RuntimeError('Unsafe package entry')
            if info.is_dir():
                continue
            target = package / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(archive.read(info))
    data = manager / 'Data/gc-minimap'
    data.mkdir(parents=True)
    if args.synthetic_maps:
        synthetic_maps(data)
    else:
        for path in args.chart_data.iterdir():
            if path.is_file() and not path.is_symlink() and path.suffix in ('.png', '.bounds') and path.name.startswith('room-'):
                shutil.copy2(path, data / path.name)
    shutil.copytree(args.save, args.out / 'save')
    (manager / 'profiles.json').write_text(json.dumps({'format_version': 1, 'active': 'Default', 'profiles': {'Default': {'enabled': {'gc-minimap': True}}}}))
    config = args.out / 'guest-sdk.json'
    config.write_text(json.dumps({'format_version': 1, 'python': [sys.executable], 'compiler': ['clang'], 'builder': str(args.sdk.resolve() / 'tools/guestmod/build_guest_mod.py'), 'include': str(args.sdk.resolve() / 'runtime/include')}))
    env = {key: value for key, value in os.environ.items() if not key.startswith('WWHD_')}
    env.update(WWHD_CODE_MODS='1', WWHD_NO_AUDIO='1', WWHD_NO_HOST_INPUT='1', WWHD_HIDDEN_WINDOWS='1', WWHD_UNCAPPED='1', WWHD_RENDERER_RUNTIME=args.renderer, WWHD_MOD_MANAGER_DIR=str(manager), WWHD_TEST_TRUST_NATIVE_MODS='gc-minimap', WWHD_GUEST_BUILD_CONFIG=str(config), WWHD_SETTINGS=str(args.out / 'settings.ini'), WWHD_DISPLAY_SETTINGS=str(args.out / 'display.plist'), WWHD_CONTROLS=str(args.out / 'controls.json'), WWHD_SHADER_CACHE=str(args.out / 'shaders.bin'), WWHD_VK_SHADER_CACHE=str(args.out / 'vkshaders'), WWHD_VK_PIPELINE_CACHE=str(args.out / 'vkpipelines.bin'), WWHD_STATE_DIR=str(args.out / 'states'), WWHD_TEST_ORIGIN=str(args.first_frame - 50), WWHD_TEST_END='10', WWHD_DUMP_FRAMES=','.join(str(args.first_frame + n) for n in (0, 2, 4)), WWHD_DUMP_PRESENT='1', WWHD_SIM_SCREEN='1280x720', WWHD_PRESS=','.join('%d-%d:8000' % (frame, frame + 8) for frame in range(150, args.first_frame - 200, 30)), WWHD_TRUE60='1' if args.mode == 'true60' else '0', WWHD_INTERP='0', XDG_CONFIG_HOME=str(args.out / 'config'))
    if args.mode == 'interp60':
        env['WWHD_INTERP_AT_STEP'] = str(args.first_frame - 190)
    with (args.out / 'runtime.log').open('w') as log:
        process = subprocess.Popen([str(args.binary.resolve()), '--game', str(args.game.resolve()), '--save', str(args.out / 'save')], cwd=args.out, env=env, stdout=log, stderr=subprocess.STDOUT)
        print('Owned game PID:', process.pid, flush=True)
        try:
            deadline = time.monotonic() + args.timeout
            while process.poll() is None and not (args.out / 'test_done').exists():
                if time.monotonic() >= deadline:
                    raise RuntimeError('Functional case timed out')
                if shutil.disk_usage(args.out).free < 15 * 1024**3:
                    raise RuntimeError('Free disk fell below 15 GiB')
                if len(other_games(process.pid)) >= args.max_game_sessions or other_benchmarks():
                    raise RuntimeError('Functional game limit reached or a benchmark started')
                time.sleep(1)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    if not (args.out / 'test_done').exists():
        raise RuntimeError('Game exited before test completion')
    from PIL import Image
    observations = []
    for frame in (args.first_frame, args.first_frame + 2, args.first_frame + 4):
        path = args.out / ('frame_%d_present.png' % frame)
        with Image.open(path) as image:
            crop = image.convert('RGB').crop((6, 477, 231, 702))
            green = sum(g > r + 40 and g > b + 40 for r, g, b in crop.getdata())
        observations.append({'frame': frame, 'green_pixels': green})
    passed = all(row['green_pixels'] > 5000 for row in observations) if args.synthetic_maps else None
    report = {'renderer': args.renderer, 'mode': args.mode, 'synthetic_maps': args.synthetic_maps, 'synthetic_hud_presence_pass': passed, 'frames': observations, 'binary_sha256': hashlib.sha256(args.binary.read_bytes()).hexdigest(), 'limitations': 'Smoke only. Does not prove setup/catalogue lifecycle, real map registration or legacy visual parity.'}
    (args.out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))
    if passed is False:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
