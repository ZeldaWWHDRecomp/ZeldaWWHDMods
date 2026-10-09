#!/usr/bin/env python3
"""Opt-in disabled-package versus absent-package capture comparison, never a performance test.

Runs sequentially with independent copies of one normal save and identical starting
settings. Outputs (including game-derived captures/traces) must remain private.
Host clocks and asynchronous loading are not deterministic in the current port:
nonmatching images are reported as inconclusive for attributing a mod regression.
Even matching captures prove only equality of those sampled images, not global identity.
Requires Pillow for reading actual runtime captures; offline helper tests use no Pillow.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import zipfile

FLOOR = 15 * 1024**3


def sha(data):
    return hashlib.sha256(data).hexdigest()


def compare_rgba(left_size, left, right_size, right):
    if len(left) != left_size[0] * left_size[1] * 4 or len(right) != right_size[0] * right_size[1] * 4:
        raise ValueError('Invalid RGBA capture size')
    result = {'left_size': list(left_size), 'right_size': list(right_size),
              'left_rgba_sha256': sha(left), 'right_rgba_sha256': sha(right)}
    if left_size != right_size:
        return dict(result, pixels_identical=False, different_pixels=None,
                    different_channels=None, maximum_channel_difference=None, reason='dimensions differ')
    channels = sum(a != b for a, b in zip(left, right))
    pixels = sum(left[i:i+4] != right[i:i+4] for i in range(0, len(left), 4))
    return dict(result, pixels_identical=left == right, different_pixels=pixels,
                different_channels=channels,
                maximum_channel_difference=max((abs(a-b) for a, b in zip(left, right)), default=0))


def save_snapshot(folder):
    records = {}
    total = 0
    for path in sorted(folder.rglob('*')):
        if path.is_symlink():
            raise ValueError('Save fixture may not contain symlinks')
        if path.is_dir():
            continue
        if not path.is_file() or path.stat().st_size > 64 * 1024**2:
            raise ValueError('Unexpected or oversized normal-save file')
        total += path.stat().st_size
        if total > 128 * 1024**2:
            raise ValueError('Normal-save fixture exceeds 128 MiB')
        records[path.relative_to(folder).as_posix()] = sha(path.read_bytes())
    if not records:
        raise ValueError('Normal-save fixture is empty')
    return records


def unpack_package(archive_path, folder):
    with zipfile.ZipFile(archive_path) as archive:
        entries = archive.infolist()
        if len(entries) > 4096 or sum(info.file_size for info in entries) > 512 * 1024**2:
            raise ValueError('Oversized package inventory')
        seen = set()
        for info in entries:
            path = Path(info.filename)
            if (not info.filename or path.is_absolute() or '..' in path.parts or '\\' in info.filename
                    or ':' in info.filename or info.filename in seen
                    or (info.external_attr >> 16) & 0o170000 == 0o120000
                    or info.file_size > 128 * 1024**2):
                raise ValueError('Unsafe package entry')
            seen.add(info.filename)
            if info.is_dir():
                continue
            target = folder / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(archive.read(info))
    manifest = json.loads((folder / 'manifest.json').read_text())
    if manifest.get('id') != 'gc-minimap' or manifest.get('kind') != 'guest':
        raise ValueError('Expected a gc-minimap guest package')


def case_environment(case, renderer, mode, first_frame, frames):
    env = {key: value for key, value in os.environ.items() if not key.startswith('WWHD_')}
    env.update(WWHD_CODE_MODS='1', WWHD_NO_AUDIO='1', WWHD_NO_HOST_INPUT='1',
               WWHD_HIDDEN_WINDOWS='1', WWHD_UNCAPPED='1', WWHD_VK_UNCAPPED='1',
               WWHD_RENDERER_RUNTIME=renderer, WWHD_MOD_MANAGER_DIR=str(case / 'manager'),
               WWHD_SETTINGS=str(case / 'settings.ini'), WWHD_DISPLAY_SETTINGS=str(case / 'display.plist'),
               WWHD_CONTROLS=str(case / 'controls.json'), WWHD_SHADER_CACHE=str(case / 'shaders.bin'),
               WWHD_VK_SHADER_CACHE=str(case / 'vkshaders'), WWHD_VK_PIPELINE_CACHE=str(case / 'vkpipelines.bin'),
               WWHD_STATE_DIR=str(case / 'states'), WWHD_TEST_ORIGIN=str(first_frame - 50),
               WWHD_TEST_END='10', WWHD_DUMP_FRAMES=','.join(map(str, frames)), WWHD_DUMP_PRESENT='1',
               WWHD_SIM_SCREEN='1280x720',
               WWHD_PRESS=','.join('%d-%d:8000' % (frame, frame + 8) for frame in range(150, first_frame - 200, 30)),
               WWHD_TRUE60='1' if mode == 'true60' else '0', WWHD_INTERP='0',
               WWHD_RNG_TRACE=str(case / 'rng-trace.txt'), WWHD_LOGIC_TIMELINE=str(case / 'logic-timeline.txt'),
               WWHD_SAVEINFO_DUMP=str(case / 'save-info.bin'), XDG_CONFIG_HOME=str(case / 'config'))
    if mode == 'interp60':
        env['WWHD_INTERP_AT_STEP'] = str(first_frame - 190)
    return env


def run_case(args, case, installed, frames, other_games, other_benchmarks, expected_save, binary_hash):
    if len(other_games()) >= args.max_game_sessions or other_benchmarks():
        raise RuntimeError('Functional game limit reached or a benchmark is running')
    if shutil.disk_usage(args.out).free < FLOOR:
        raise RuntimeError('Free disk is below 15 GiB')
    case.mkdir()
    manager = case / 'manager'
    (manager / 'Mods').mkdir(parents=True)
    if installed:
        unpack_package(args.package, manager / 'Mods/gc-minimap')
    # Keep profile/options bytes identical even when the referenced disabled package is absent.
    (manager / 'profiles.json').write_text(json.dumps({'format_version': 1, 'active': 'Default',
        'profiles': {'Default': {'enabled': {'gc-minimap': False}, 'config': {'gc-minimap': args.options}}}}))
    shutil.copytree(args.save, case / 'save')
    if save_snapshot(case / 'save') != expected_save or sha(args.binary.read_bytes()) != binary_hash:
        raise RuntimeError('Initial save or binary changed during comparison')
    for argument, name in ((args.settings, 'settings.ini'), (args.controls, 'controls.json'), (args.display, 'display.plist')):
        if argument:
            shutil.copy2(argument, case / name)
    env = case_environment(case, args.renderer, args.mode, args.first_frame, frames)
    (case / 'initial-environment.json').write_text(json.dumps({key: value for key, value in env.items() if key.startswith('WWHD_') or key == 'XDG_CONFIG_HOME'}, indent=2, sort_keys=True) + '\n')
    started = time.monotonic()
    with (case / 'runtime.log').open('w') as log:
        process = subprocess.Popen([str(args.binary), '--game', str(args.game), '--save', str(case / 'save')],
                                   cwd=case, env=env, stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
        print('Owned functional game PID:', process.pid, case.name, flush=True)
        try:
            while process.poll() is None and not (case / 'test_done').exists():
                if time.monotonic() - started >= args.timeout:
                    raise RuntimeError('Functional comparison case timed out')
                if shutil.disk_usage(args.out).free < FLOOR:
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
    if not (case / 'test_done').exists():
        raise RuntimeError('Game exited before scenario completion')
    runtime_log = (case / 'runtime.log').read_text(errors='replace')
    if '[guestmods] loaded ' in runtime_log or '[guestmod:gc-minimap]' in runtime_log:
        raise RuntimeError('A guest module loaded during a disabled/absent case')
    profile = json.loads((manager / 'profiles.json').read_text())
    if profile['profiles'][profile['active']]['enabled'].get('gc-minimap') is not False:
        raise RuntimeError('Minimap was not explicitly disabled')
    trace = case / 'rng-trace.txt'
    lines = trace.read_text().splitlines() if trace.exists() else []
    return {'case': case.name, 'package_installed': installed, 'package_enabled': False,
            'scenario_completed': True, 'link_trace_records': len(lines),
            'rng_trace_sha256': sha(trace.read_bytes()) if trace.exists() else None,
            'initial_save_sha256_by_file': expected_save, 'no_guest_module_loaded': True,
            'normal_save_after': save_snapshot(case / 'save')}


def compare_cases(disabled, absent, frames):
    from PIL import Image
    observations = []
    valid_scene = True
    for frame in frames:
        for screen, suffix in (('tv_scan', '.png'), ('tv_present', '_present.png'), ('drc_scan', '_drc.png')):
            left_path = disabled / ('frame_%d%s' % (frame, suffix))
            right_path = absent / ('frame_%d%s' % (frame, suffix))
            if not left_path.is_file() or not right_path.is_file():
                raise RuntimeError('Missing required %s capture at frame %d' % (screen, frame))
            with Image.open(left_path) as image:
                left_size, left = image.size, image.convert('RGBA').tobytes()
            with Image.open(right_path) as image:
                right_size, right = image.size, image.convert('RGBA').tobytes()
            result = compare_rgba(left_size, left, right_size, right)
            # Black/flat TV frames are not useful scene evidence even when byte-identical.
            colors = [len(set(pixels[i:i+4] for i in range(0, len(pixels), 4))) for pixels in (left, right)]
            if screen.startswith('tv_') and min(colors) < 32:
                valid_scene = False
            result.update(frame=frame, screen=screen, distinct_colors=colors,
                          png_files_identical=left_path.read_bytes() == right_path.read_bytes(),
                          disabled_png_sha256=sha(left_path.read_bytes()), absent_png_sha256=sha(right_path.read_bytes()))
            observations.append(result)
    return observations, valid_scene



def capture_status(rows, scene):
    if not rows or not scene:
        return 'inconclusive'
    return 'captured_frames_identical' if all(row['pixels_identical'] and row['png_files_identical'] for row in rows) else 'inconclusive'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'game', 'save', 'sdk', 'package', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--renderer', choices=('metal', 'vulkan'), required=True)
    parser.add_argument('--mode', choices=('30', 'interp60', 'true60'), default='30')
    parser.add_argument('--first-frame', type=int, default=3000)
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--pairs', type=int, choices=range(1, 5), default=2)
    parser.add_argument('--max-game-sessions', type=int, choices=range(1, 5), default=4)
    parser.add_argument('--options', type=json.loads, default={}, help='Same minimap option object in both profiles')
    for name in ('settings', 'controls', 'display'):
        parser.add_argument('--' + name, type=Path, help='Optional identical starting configuration file')
    args = parser.parse_args()
    for name in ('binary', 'game', 'save', 'sdk', 'package', 'out'):
        setattr(args, name, getattr(args, name).resolve())
    if not isinstance(args.options, dict) or args.first_frame < 700 or args.timeout < 1:
        parser.error('Expected an option object, startup frame >=700, and positive timeout')
    if any((parent / '.git').exists() for parent in (args.out, *args.out.parents)):
        parser.error('Derived captures/traces must stay outside every Git checkout')
    if not args.binary.is_file() or not args.package.is_file() or not args.game.is_dir() or not args.save.is_dir():
        parser.error('Binary, package, game directory or normal save is unavailable')
    if not args.out.parent.is_dir() or shutil.disk_usage(args.out.parent).free < FLOOR:
        parser.error('Output parent must exist with at least 15 GiB free')
    sys.path.insert(0, str(args.sdk / 'tools/bench'))
    from run_bench import other_games, other_benchmarks
    expected_save = save_snapshot(args.save)
    binary_hash = sha(args.binary.read_bytes())
    args.out.mkdir(exist_ok=False)
    # Freeze optional initial configuration once, so external edits cannot change the second case.
    fixtures = args.out / 'configuration-fixtures'
    fixtures.mkdir()
    for name in ('settings', 'controls', 'display'):
        original = getattr(args, name)
        if original:
            target = fixtures / name
            shutil.copy2(original, target)
            setattr(args, name, target)
    frames = [args.first_frame + offset for offset in (0, 2, 4)]
    report = {'binary_sha256': binary_hash, 'package_sha256': sha(args.package.read_bytes()),
              'renderer': args.renderer, 'mode': args.mode, 'frames': frames, 'pairs': [],
              'sdk_source_commit': subprocess.check_output(['git', '-C', str(args.sdk), 'rev-parse', 'HEAD'], text=True).strip(),
              'capture_coverage': ['TV scan texture', 'composed TV presentation', 'DRC scan texture'],
              'deterministic_timebase_supported': False, 'global_disabled_identity_proven': False,
              'limitations': ['Current guest timebase follows the host steady clock; there is no supported fixed-clock flag.',
                              'Scripted boot, shader compilation and asynchronous loading can diverge between runs.',
                              'Exact samples establish only captured image equality, not all game behavior or scene equivalence.',
                              'DRC capture is the scan texture; the port does not provide an independent composed DRC-window dump here.',
                              'Differences do not by themselves attribute a regression to the disabled mod.',
                              'This optional check supplements the full region/renderer/frame-rate and lifecycle verification.']}
    try:
        for pair in range(args.pairs):
            order = ('disabled', 'absent') if pair % 2 == 0 else ('absent', 'disabled')
            cases = {}
            for label in order:
                case = args.out / ('pair-%02d-%s' % (pair + 1, label))
                cases[label] = run_case(args, case, label == 'disabled', frames, other_games, other_benchmarks, expected_save, binary_hash)
            rows, scene = compare_cases(args.out / cases['disabled']['case'], args.out / cases['absent']['case'], frames)
            scene = scene and all(case['link_trace_records'] > 0 for case in cases.values())
            identical = all(row['pixels_identical'] for row in rows)
            result = {'order': list(order), 'cases': cases, 'captures': rows,
                      'nonflat_tv_and_link_execution_observed': scene,
                      'all_sampled_pixels_identical': identical,
                      'all_sampled_png_files_identical': all(row['png_files_identical'] for row in rows),
                      'status': capture_status(rows, scene),
                      'rng_traces_identical': cases['disabled']['rng_trace_sha256'] == cases['absent']['rng_trace_sha256']}
            report['pairs'].append(result)
            (args.out / 'result.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
        if save_snapshot(args.save) != expected_save or sha(args.binary.read_bytes()) != binary_hash:
            raise RuntimeError('Original save or runtime binary changed during comparison')
        report['status'] = 'captured_frames_identical' if all(pair['status'] == 'captured_frames_identical' for pair in report['pairs']) else 'inconclusive'
    except Exception as error:
        report['status'] = 'incomplete'
        report['error'] = str(error)
    (args.out / 'result.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'status': report['status'], 'pairs_completed': len(report['pairs']), 'private_report': str(args.out / 'result.json')}))
    return 0 if report['status'] == 'captured_frames_identical' else 2


if __name__ == '__main__':
    raise SystemExit(main())
