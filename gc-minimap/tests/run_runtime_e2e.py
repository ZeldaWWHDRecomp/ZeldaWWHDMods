#!/usr/bin/env python3
"""Opt-in real-game HUD smoke test. All saves/captures remain in a private output folder.

Synthetic maps exercise guest capture and HUD delivery; they do not prove real-map
registration, setup, catalogue lifecycle, or legacy visual parity.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time
import zipfile
import zlib


def synthetic_maps(data, edge=None):
    def chunk(kind, value):
        return struct.pack('>I', len(value)) + kind + value + struct.pack('>I', zlib.crc32(kind + value) & 0xffffffff)
    pixels = b''.join(b'\0' + bytes((31, 201, 109, 255)) * 32 for _ in range(32))
    image = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 32, 32, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b'')
    for room in range(1, 50):
        gx, gz = (room - 1) % 7 - 3, (room - 1) // 7 - 3
        bounds = [gx * 100000 - 50000, gz * 100000 - 50000, gx * 100000 + 50000, gz * 100000 + 50000]
        # Authored bounds deliberately put every real position in this sector
        # beyond one chart edge; no game memory or derived map data is changed.
        if edge in ('left', 'right'):
            low = gx * 100000 + (60000 if edge == 'left' else -70000)
            bounds[0], bounds[2] = low, low + 10000
        if edge in ('top', 'bottom'):
            low = gz * 100000 + (60000 if edge == 'top' else -70000)
            bounds[1], bounds[3] = low, low + 10000
        (data / ('room-%02d.bounds' % room)).write_bytes(struct.pack('>4f', *bounds))
        (data / ('room-%02d-vector.png' % room)).write_bytes(image)


def sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def hud_observation(path):
    from PIL import Image
    with Image.open(path) as image:
        pixels = image.convert('RGB').crop((28, 499, 208, 679)).getdata()
        green = marker = 0
        for r, g, b in pixels:
            green += g > r + 40 and g > b + 40
            marker += r > 200 and b < 110 and r > g + 10
    return {'green_pixels': green, 'marker_color_pixels': marker}


def state_cycle_result(out, synthetic, first_frame):
    log = (out / 'runtime.log').read_text(errors='replace')
    saved = bool(re.search(r'\[savestate\] slot 1: written \(', log))
    restored = bool(re.search(r'\[savestate\] slot 1: restored in ', log))
    state = out / 'states/slot1.bin'
    # Native STATE_DUMP is the raw GX2 scan surface, before the guest HUD.
    # Require its successful restore-triggered dump before scheduled presentation
    # captures, then inspect those captures which include the host HUD overlay.
    dump = out / 'state_load1_3.png'
    restore_at = log.find('[savestate] slot 1: restored in ')
    dump_at = log.find('[gfx] wrote state_load1_3.png')
    captures = []
    for frame in (first_frame, first_frame + 2, first_frame + 4):
        path = out / ('frame_%d_present.png' % frame)
        row = hud_observation(path) if path.is_file() and synthetic else {}
        row['capture_exists'] = path.is_file()
        row.update(frame=frame, after_restore_dump=restore_at >= 0 and dump_at > restore_at and
                   any(log.find(prefix + path.name) > dump_at for prefix in
                       ('[gfx] wrote ', '[display] present dump ')))
        captures.append(row)
    ordered = all(row['capture_exists'] and row['after_restore_dump'] for row in captures)
    visible = all(row.get('marker_color_pixels', 0) >= 10 and
                  row.get('green_pixels', 0) > 5000 for row in captures) if synthetic else None
    passed = saved and restored and state.is_file() and dump.is_file() and ordered and visible is not False
    return {'passed': passed, 'synthetic_hud_recovery_pass': visible,
            'real_map_visual_review_required': not synthetic, 'save_written_log': saved, 'restore_success_log': restored,
            'state_sha256': sha256_file(state) if state.is_file() else None,
            'post_restore_native_dump': dump.name, 'post_restore_present_hud': captures,
            'scope': 'Same-process compatible full-state restore with gc-minimap active. '
                     'Synthetic captures check chart/marker recovery; real-map captures require separate visual review. '
                     'No real-map registration or cross-version claim from automated results.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'game', 'save', 'sdk', 'package', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--chart-data', type=Path, help='Previously prepared private map data')
    source.add_argument('--synthetic-maps', action='store_true', help='Authored HUD smoke fixture only')
    parser.add_argument('--synthetic-marker-edge', choices=('left', 'right', 'top', 'bottom'), help='Authored boundary-clipping fixture; requires --synthetic-maps')
    parser.add_argument('--renderer', choices=('metal', 'vulkan'), required=True)
    parser.add_argument('--mode', choices=('30', 'interp60', 'true60'), default='30')
    parser.add_argument('--first-frame', type=int, default=3000)
    parser.add_argument('--state-cycle', action='store_true', help='Save and restore a same-run native full state; verify post-restore HUD')
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--max-game-sessions', type=int, choices=range(1, 5), default=4)
    parser.add_argument('--min-free-gib', type=int, default=15, help='Explicit local disk floor; 0 disables the gate when authorized')
    parser.add_argument('--allow-concurrent-benchmark', action='store_true', help='Authorized functional checks only; results are not performance evidence')
    args = parser.parse_args()
    if args.min_free_gib < 0:
        parser.error('Disk floor must be nonnegative')
    if args.synthetic_marker_edge and not args.synthetic_maps:
        parser.error('--synthetic-marker-edge requires --synthetic-maps')
    args.out = args.out.resolve()
    repo = Path(__file__).resolve().parents[2]
    for checkout in (repo, args.sdk.resolve()):
        if args.out.is_relative_to(checkout):
            parser.error('Private outputs must stay outside source checkouts')
    if args.first_frame < 700:
        parser.error('Allow at least 700 frames for startup')
    if shutil.disk_usage(args.out.parent).free < args.min_free_gib * 1024**3:
        parser.error('Free disk is below the configured floor')
    sys.path.insert(0, str(args.sdk.resolve() / 'tools/bench'))
    from run_bench import other_games, other_benchmarks
    if len(other_games()) >= args.max_game_sessions or (other_benchmarks() and not args.allow_concurrent_benchmark):
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
        synthetic_maps(data, args.synthetic_marker_edge)
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
    if args.state_cycle:
        env.update(WWHD_STATE_SAVE_AT='%d:1' % (args.first_frame - 150),
                   WWHD_STATE_LOAD_AT='%d:1' % (args.first_frame - 80), WWHD_STATE_DUMP='3')
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
                if shutil.disk_usage(args.out).free < args.min_free_gib * 1024**3:
                    raise RuntimeError('Free disk fell below the configured floor')
                if len(other_games(process.pid)) >= args.max_game_sessions or (other_benchmarks() and not args.allow_concurrent_benchmark):
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
    report = {'allow_concurrent_benchmark': args.allow_concurrent_benchmark, 'min_free_gib': args.min_free_gib, 'renderer': args.renderer, 'mode': args.mode, 'synthetic_maps': args.synthetic_maps, 'synthetic_marker_edge': args.synthetic_marker_edge, 'synthetic_hud_presence_pass': passed, 'frames': observations, 'binary_sha256': sha256_file(args.binary), 'package_sha256': sha256_file(args.package), 'sdk_head': subprocess.check_output(['git', '-C', str(args.sdk.resolve()), 'rev-parse', 'HEAD'], text=True).strip(), 'limitations': 'Smoke only. Does not prove setup/catalogue lifecycle, real map registration or legacy visual parity.'}
    if args.state_cycle:
        report['state_cycle'] = state_cycle_result(args.out, args.synthetic_maps, args.first_frame)
    (args.out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))
    if passed is False or (args.state_cycle and not report['state_cycle']['passed']):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
