#!/usr/bin/env python3
"""Opt-in real-map minimap+dragon coexistence check; private captures need visual review.

This checks normal invitation/letter interaction, not the complete quest or flight.
Use --allow-concurrent-benchmark only for explicitly authorized functional checks.
"""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import zipfile

spec = importlib.util.spec_from_file_location('identity', Path(__file__).with_name('run_disabled_identity.py'))
identity = importlib.util.module_from_spec(spec)
spec.loader.exec_module(identity)
CAPTURES = {'before_letter': 2850, 'letter_open': 3000, 'after_close': 3200}


def unpack(path, target, expected):
    with zipfile.ZipFile(path) as archive:
        entries = archive.infolist()
        if len(entries) > 4096 or sum(e.file_size for e in entries) > 512 * 1024**2:
            raise ValueError('Oversized package')
        seen = set()
        for entry in entries:
            name = Path(entry.filename)
            if (name.is_absolute() or '..' in name.parts or '\\' in entry.filename or ':' in entry.filename
                    or entry.filename in seen or (entry.external_attr >> 16) & 0o170000 == 0o120000
                    or entry.file_size > 128 * 1024**2):
                raise ValueError('Unsafe package entry')
            seen.add(entry.filename)
            if entry.is_dir():
                continue
            destination = target / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(archive.read(entry))
    manifest = json.loads((target / 'manifest.json').read_text())
    if manifest.get('id') != expected or manifest.get('kind') != 'guest':
        raise ValueError('Unexpected guest package identity')


def loaded_modules(text):
    result = {}
    for line in text.splitlines():
        if '[guestmods] loaded ' not in line:
            continue
        for name in ('dragon', 'gc-minimap'):
            if '/' + name + '.' in line or '\\' + name + '.' in line:
                result[name] = line
    return result


def runtime_evidence(text, completed, captures):
    loaded = loaded_modules(text)
    failures = [line for line in text.splitlines() if any(token in line.lower() for token in
                ('hook conflict', 'multiple replacements', '[guestmods] failed', '[guestmods] error'))]
    return {'scenario_completed': completed, 'loaded_modules': loaded, 'conflict_errors': failures,
            'all_phase_captures_present': all(captures.values()),
            'runtime_smoke_passed': completed and set(loaded) == {'dragon', 'gc-minimap'}
                                    and not failures and all(captures.values()),
            'visual_review_required': True, 'complete_quest_or_flight_proven': False}


def rectangles_overlap(a, b):
    """Positive-area overlap; touching borders are safe."""
    return max(a[0], b[0]) < min(a[2], b[2]) and max(a[1], b[1]) < min(a[3], b[3])


def seed_cache(sdk, manager, source, game):
    """Copy only modules whose SDK/ELF/compiler/region/base cache key matches exactly."""
    sys.path.insert(0, str(sdk / 'tools/guestmod'))
    import build_guest_mod as builder
    sys.path.insert(0, str(sdk / 'tools/recomp'))
    import builds
    region = builds.identify(str(game / 'code/cking.rpx'))
    if region is None:
        raise ValueError('Unsupported game region')
    base = 0x7F000000; copied = {}; requests = []
    cc = ['clang']; include = str(sdk / 'runtime/include'); version = builder.compiler_version(cc)
    for name in ('dragon', 'gc-minimap'):
        package = manager / 'Mods' / name
        manifest, elf = builder.package_elf(package)
        key = builder.cache_key(elf, name, base, cc, include, region, version)
        origin = source / key / (name + builder.module_ext())
        if not origin.is_file() or origin.is_symlink():
            raise ValueError('Missing exact verified cache module: ' + str(origin))
        destination = manager / 'GuestBuild' / key / origin.name
        destination.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(origin, destination)
        digest = identity.sha(origin.read_bytes())
        if digest != identity.sha(destination.read_bytes()):
            raise ValueError('Cache module copy changed')
        copied[name] = {'key': key, 'base': base, 'module_sha256': digest, 'source': str(origin)}
        requests.append({'package': str(package), 'id': name, 'base': base, 'module': str(destination)})
        allocation = builder.inspect_package(package, base, region)['allocation_size']
        heap = (manifest.get('guest', {}).get('heap_size', 256 * 1024) + 15) & ~15
        base += (allocation + heap + 0xffff) & ~0xffff
    checked = builder.check_cached(requests, manager / 'GuestBuild', cc, include, region)
    if set(checked['valid']) != {'dragon', 'gc-minimap'}:
        raise ValueError('SDK rejected seeded native cache')
    return copied


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'game', 'save', 'sdk', 'minimap', 'dragon', 'maps', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--renderer', choices=('metal', 'vulkan'), required=True)
    parser.add_argument('--cache-from', type=Path, help='Reviewed exact current SDK native cache; refuses any key mismatch')
    parser.add_argument('--dragon-panel-x', type=int, default=24, help='Reviewed package panel x; record actual source geometry, then visually verify')
    parser.add_argument('--timeout', type=int, default=600)
    parser.add_argument('--max-game-sessions', type=int, choices=range(1, 5), default=4)
    parser.add_argument('--min-free-gib', type=int, default=15)
    parser.add_argument('--allow-concurrent-benchmark', action='store_true')
    args = parser.parse_args()
    args.out = args.out.resolve(); args.sdk = args.sdk.resolve()
    if args.min_free_gib < 0 or args.timeout < 1:
        parser.error('Invalid disk floor or timeout')
    if any((parent / '.git').exists() for parent in (args.out, *args.out.parents)):
        parser.error('Derived captures/maps must stay outside all Git checkouts')
    sys.path.insert(0, str(args.sdk / 'tools/bench'))
    from run_bench import other_games, other_benchmarks
    def gate(own_pid=None):
        if len(other_games(own_pid)) >= args.max_game_sessions:
            raise RuntimeError('Global game-session limit reached')
        if other_benchmarks() and not args.allow_concurrent_benchmark:
            raise RuntimeError('Benchmark running')
        if shutil.disk_usage(args.out.parent).free < args.min_free_gib * 1024**3:
            raise RuntimeError('Configured disk floor reached')
    gate()
    args.out.mkdir(exist_ok=False); manager = args.out / 'manager'
    for name in ('dragon', 'gc-minimap'):
        unpack(args.dragon if name == 'dragon' else args.minimap, manager / 'Mods' / name, name)
    data = manager / 'Data/gc-minimap'; data.mkdir(parents=True)
    maps = {}
    for source in sorted(args.maps.iterdir()):
        if source.is_file() and not source.is_symlink() and source.name.startswith('room-') and source.suffix in ('.png', '.bounds'):
            if source.stat().st_size > 1024**2:
                raise ValueError('Oversized prepared map')
            shutil.copy2(source, data / source.name); maps[source.name] = identity.sha(source.read_bytes())
    if not maps:
        raise ValueError('No prepared real map data')
    cached = seed_cache(args.sdk, manager, args.cache_from, args.game.resolve()) if args.cache_from else {}
    before = identity.save_snapshot(args.save); shutil.copytree(args.save, args.out / 'save')
    (manager / 'profiles.json').write_text(json.dumps({'format_version': 1, 'active': 'Default',
        'profiles': {'Default': {'enabled': {'dragon': True, 'gc-minimap': True}}}}))
    config = args.out / 'guest-sdk.json'
    config.write_text(json.dumps({'format_version': 1, 'python': [sys.executable], 'compiler': ['clang'],
        'builder': str(args.sdk / 'tools/guestmod/build_guest_mod.py'), 'include': str(args.sdk / 'runtime/include')}))
    env = identity.case_environment(args.out, args.renderer, '30', 3000, list(CAPTURES.values()))
    env.update(WWHD_NO_GAMEPAD='1', WWHD_TEST_TRUST_NATIVE_MODS='dragon,gc-minimap',
        WWHD_GUEST_BUILD_CONFIG=str(config), WWHD_CRASH_RECOVERY='0', WWHD_TEST_ORIGIN='2800', WWHD_TEST_END='20')
    env['WWHD_PRESS'] = ','.join('%d-%d:8000' % (f, f + 8) for f in range(150, 2650, 30)) + ',2900-2908:0800,3100-3108:8000'
    (args.out / 'environment.json').write_text(json.dumps({k: v for k, v in env.items() if k.startswith('WWHD_')}, indent=2))
    report = {'binary_sha256': identity.sha(args.binary.read_bytes()), 'renderer': args.renderer,
        'packages': {name: identity.sha(path.read_bytes()) for name, path in [('dragon', args.dragon), ('gc-minimap', args.minimap)]},
        'sdk_commit': subprocess.check_output(['git', '-C', str(args.sdk), 'rev-parse', 'HEAD'], text=True).strip(),
        'source_save': before, 'map_hashes': maps, 'normal_save_no_pokes_no_seeded_progress': True,
        'capture_phases': CAPTURES, 'seeded_native_cache': cached,
        'reviewed_expected_panel_rectangles': {'minimap': [6, 477, 231, 702], 'dragon': [args.dragon_panel_x, 476, args.dragon_panel_x + 560, 696]},
        'expected_panel_regions_nonoverlap': not rectangles_overlap([6,477,231,702], [args.dragon_panel_x,476,args.dragon_panel_x+560,696]),
        'coexistence_passed': False, 'actual_legibility_requires_visual_review': True, 'allow_concurrent_benchmark': args.allow_concurrent_benchmark,
        'min_free_gib': args.min_free_gib, 'status': 'incomplete'}
    with (args.out / 'runtime.log').open('w') as log:
        gate()
        process = subprocess.Popen([str(args.binary.resolve()), '--game', str(args.game.resolve()), '--save', str(args.out / 'save')],
            cwd=args.out, env=env, stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
        print('Owned combined functional PID:', process.pid, flush=True)
        try:
            deadline = time.monotonic() + args.timeout
            while process.poll() is None and not (args.out / 'test_done').exists():
                gate(process.pid)
                if time.monotonic() > deadline:
                    raise RuntimeError('Functional scenario timed out')
                time.sleep(1)
        except Exception as error:
            report['error'] = str(error)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait()
    captures = {phase: all((args.out / ('frame_%d%s.png' % (frame, suffix))).is_file()
                          for suffix in ('', '_present', '_drc')) for phase, frame in CAPTURES.items()}
    report.update(runtime_evidence((args.out / 'runtime.log').read_text(errors='replace'),
        (args.out / 'test_done').exists(), captures))
    report['original_save_unchanged'] = before == identity.save_snapshot(args.save)
    report['binary_unchanged'] = report['binary_sha256'] == identity.sha(args.binary.read_bytes())
    report['status'] = 'runtime_smoke_visual_review_pending' if report['runtime_smoke_passed'] and report['original_save_unchanged'] and report['binary_unchanged'] else 'incomplete'
    (args.out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(report['status'], flush=True)
    return 0 if report['status'] == 'runtime_smoke_visual_review_pending' else 2


if __name__ == '__main__':
    raise SystemExit(main())
