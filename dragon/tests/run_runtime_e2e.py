#!/usr/bin/env python3
"""Opt-in dragon runtime scenarios; requires a private, reviewed input route.

No game, normal saves, full states, screenshots or fixture bytes are distributed.
--write-plan creates the coverage checklist and deliberately unready route template.
--validate-only checks a private route without starting a game. Run one case per
USA/EU x Metal/Vulkan x 30/interp60/true60 combination. The harness starts one owned
process and terminates only that process. It never builds the host runtime;
the runtime may compile its installed guest package through the normal mod flow. A full state can only be
created and loaded within this run, with the current binary and mod package.

Legacy walkthrough travel was teleported. Such routes must be classified
'instrumented'; they never count as a normal, uninterrupted quest completion.
Screenshots and ordinary-actor/boat/Leaf behavior require explicit human review;
a completed process and a visible panel alone are not gameplay proof.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
import zipfile

COVERAGE = {
    'normal-quest': 'Normal prerequisite save; invitation, letter, Medli acceptance, all three sword-cut chimes, lesson, six-note song, first ride. No pokes or seeded quest progress.',
    'dialogue': 'Read/close letter; open/cancel/accept Medli dialogue; verify A/B do not also attack, roll or leave the panel.',
    'song-cancel': 'Reach lesson normally; wrong meter/note/order leaves stock songs intact; B cancels complete recognition before learning or summoning.',
    'ride-release': 'Learned save produced by normal flow; native claw/hook/rope, approach, launch, lift, steering, floor/ceiling, obstacle stop, A release.',
    'ordinary-actors': 'Normal story Medli/Valoo/gongs remain stock, with dragon enabled and disabled on the same current base.',
    'boat-leaf': 'Recognize/cancel/complete while aboard the boat; boat control restored; release then activate the assigned Deku Leaf and verify normal magic consumption.',
    'save-slots': 'Load distinct Quest Logs, initialize another slot, reload each; progress stays per-slot and other-slot initialization preserves the active quest.',
    'full-state': 'Create current-base full states during dialogue, song, pickup and flight; restore them in this same run and verify mod state/actors/callbacks/input ownership recover.',
    'failure-recovery': 'Explicit private instrumented fixture only: native request failure/timeout, competing PTMF, actor deletion, stage/event interruption. Never normal-flow proof.',
}
MODES = ('30', 'interp60', 'true60')


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def save_inventory(folder):
    return {str(path.relative_to(folder)): sha256(path) for path in sorted(folder.rglob('*')) if path.is_file() and not path.is_symlink()}


def safe_copy_save(source, target):
    if source.is_symlink() or not source.is_dir():
        raise ValueError('Normal save must be a directory, not a symlink')
    for path in source.rglob('*'):
        if path.is_symlink() or (not path.is_file() and not path.is_dir()):
            raise ValueError('Save contains a symlink or special file')
        if path.is_file() and path.suffix.lower() in ('.bin', '.wwstate'):
            raise ValueError('Provide normal game saves, not old full/portable state files')
    shutil.copytree(source, target)


def unpack_package(source, target):
    with zipfile.ZipFile(source) as archive:
        entries = archive.infolist()
        if len(entries) > 4096 or sum(info.file_size for info in entries) > 64 * 1024**2:
            raise ValueError('Dragon package exceeds test extraction limits')
        names = set()
        for info in entries:
            path = Path(info.filename)
            if (path.is_absolute() or '\\' in info.filename or ':' in info.filename or
                    any(part in ('', '.', '..') for part in info.filename.rstrip('/').split('/')) or
                    info.filename in names or ((info.external_attr >> 16) & 0o170000) == 0o120000):
                raise ValueError('Unsafe/duplicate package entry')
            names.add(info.filename)
            if not info.is_dir():
                destination = target / path
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(archive.read(info))
    manifest = json.loads((target / 'manifest.json').read_text())
    if manifest.get('id') != 'dragon' or manifest.get('kind') != 'guest' or not (target / 'mod.elf').is_file():
        raise ValueError('Expected a built dragon guest package')


def finite(value, low, high):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or not low <= value <= high:
        raise ValueError('Invalid finite scenario number')
    return value


def validate_route(route):
    if route.get('format_version') != 1 or route.get('case') not in COVERAGE:
        raise ValueError('Unknown route format/case')
    if route.get('classification') not in ('normal', 'instrumented'):
        raise ValueError('Explicit normal/instrumented classification required')
    if route.get('ready') is not True:
        raise ValueError('Route is unready: record and review the current-base input route first')
    finite(route.get('duration'), 1, 3600)
    origin = route.get('origin_frame')
    if not isinstance(origin, int) or isinstance(origin, bool) or origin < 700:
        raise ValueError('Allow at least 700 TV frames for normal startup')
    for key, count in (('press', 3), ('stick', 4), ('rstick', 4)):
        for event in route.get(key, []):
            if not isinstance(event, list) or len(event) != count:
                raise ValueError('Malformed timed input')
            start, end = event[:2]
            finite(start, 0, route['duration']); finite(end, start, route['duration'])
            if key == 'press':
                if not isinstance(event[2], int) or isinstance(event[2], bool) or not 0 <= event[2] <= 0xffffffff:
                    raise ValueError('Button bits must be an unsigned integer')
            else:
                finite(event[2], -1, 1); finite(event[3], -1, 1)
    for event in route.get('boot_press', []):
        if len(event) != 3 or any(not isinstance(n, int) or isinstance(n, bool) for n in event) or not 0 <= event[0] < event[1] < origin or not 0 <= event[2] <= 0xffffffff:
            raise ValueError('Malformed boot input')
    fixture = route.get('fixture', {})
    if fixture and route['classification'] != 'instrumented':
        raise ValueError('Pokes/seeded quest data cannot be classified as normal gameplay')
    if route['case'] == 'normal-quest' and (fixture or route['classification'] != 'normal'):
        raise ValueError('Normal quest completion excludes instrumented fixtures')
    if route['case'] == 'failure-recovery' and route['classification'] != 'instrumented':
        raise ValueError('Failure injection is instrumented evidence')
    for slot, progress in fixture.get('progress', {}).items():
        if slot not in ('0', '1', '2') or not valid_progress(progress):
            raise ValueError('Invalid private progress fixture')
    if fixture.get('pokes') and fixture.get('region') not in ('USA', 'EU'):
        raise ValueError('Native poke fixtures require an explicit USA/EU region')
    for poke in fixture.get('pokes', []):
        if not isinstance(poke, str) or not re.fullmatch(r'[0-9]+(?:\.[0-9]+)?:\*?[0-9A-Fa-f]{1,8}(?:\+[0-9A-Fa-f]{1,8})?:[0-9A-Fa-f]+', poke) or len(poke.rsplit(':', 1)[1]) % 2:
            raise ValueError('Invalid private poke; provide native regional addresses explicitly')
    for frame in route.get('capture_frames', []):
        if not isinstance(frame, int) or isinstance(frame, bool) or frame < origin:
            raise ValueError('Capture frames must be TV frames at/after the origin')
    saves = route.get('full_save', [])
    loads = route.get('full_load', [])
    for frame, slot in saves + loads:
        if not isinstance(frame, int) or not isinstance(slot, int) or frame <= origin or not 1 <= slot <= 4:
            raise ValueError('Invalid current-run full-state schedule')
    for frame, slot in loads:
        if not any(prior < frame and number == slot for prior, number in saves):
            raise ValueError('Full-state loads require an earlier save in this same run')
    if route['case'] == 'full-state' and not loads:
        raise ValueError('Full-state case requires current-run save/load actions')
    expected = route.get('expect', {})
    if not expected or not route.get('review_points'):
        raise ValueError('Require automatic observations and explicit gameplay review points')
    for slot, progress in expected.get('progress', {}).items():
        if slot not in ('0', '1', '2') or not valid_progress(progress):
            raise ValueError('Invalid expected slot progress')
    return route


def valid_progress(value):
    return (isinstance(value, list) and len(value) == 2 and
            all(isinstance(n, int) and not isinstance(n, bool) for n in value) and
            0 <= value[0] <= 4 and 0 <= value[1] <= 7 and
            (value[1] == 0 if value[0] < 2 else value[0] == 2 or value[1] == 7))


def timed_input(events, kind):
    if kind == 'press':
        return ','.join(f'{start}-{end}:{bits:X}' for start, end, bits in events)
    return ','.join(f'{start}-{end}:{x}:{y}' for start, end, x, y in events)


def parse_progress(path):
    match = re.fullmatch(r'WWHD_DRAGON_QUEST\s+1\s+([0-4])\s+([0-7])\s*', path.read_text())
    if not match or not valid_progress(list(map(int, match.groups()))):
        raise ValueError('Malformed actual quest progress')
    return list(map(int, match.groups()))


def inspect_outputs(directory, route, mode, enabled=True):
    log = (directory / 'runtime.log').read_text(errors='replace')
    checks = {'finished': (directory / 'test_done').is_file(),
              'guest_activation': ('[guestmods] loaded ' in log) == enabled,
              'no_guest_crash': not any(word in log.lower() for word in ('unimplemented ppc', 'guest mod build failed', 'fatal error', 'assertion failed'))}
    for text in route['expect'].get('log_contains', []):
        checks['log:' + text] = text in log
    for text in route['expect'].get('log_absent', []):
        checks['absent:' + text] = text not in log
    actual_progress = {}
    for slot, expected in route['expect'].get('progress', {}).items():
        path = directory / 'manager/Data/dragon' / f'dragon-quest-slot{slot}.txt'
        try:
            actual_progress[slot] = parse_progress(path)
        except (OSError, ValueError):
            actual_progress[slot] = None
        checks['progress:' + slot] = actual_progress[slot] == expected
    for frame in route.get('capture_frames', []):
        checks['capture:' + str(frame)] = (directory / f'frame_{frame}_present.png').is_file()
    rows = []
    trace = directory / 'link.txt'
    if trace.is_file():
        for line in trace.read_text().splitlines():
            columns = line.split()
            if len(columns) == 18:
                rows.append({'step': int(columns[1]), 'full': int(columns[3]), 'dt': float(columns[4]), 'proc': int(columns[5]), 'position': list(map(float, columns[6:9]))})
    checks['link_trace'] = bool(rows)
    if mode == 'true60':
        checks['true60_half_passes'] = any(row['full'] == 0 for row in rows)
    else:
        checks['no_true60_half_passes'] = not any(row['full'] == 0 for row in rows)
    for proc in route['expect'].get('link_procs', []):
        checks['link_proc:' + str(proc)] = any(row['proc'] == proc for row in rows)
    for _, slot in route.get('full_save', []):
        checks['current_full_state:' + str(slot)] = (directory / 'states' / f'slot{slot}.bin').is_file()
    for slot in {number for _, number in route.get('full_load', [])}:
        count = sum(number == slot for _, number in route['full_load'])
        checks['full_restore:' + str(slot)] = len(re.findall(r'\[savestate\] slot ' + str(slot) + r': restored in ', log)) >= count
    if mode == 'interp60':
        checks['interpolation_enabled'] = '60 fps mode 1' in log
    return checks, actual_progress, {'rows': len(rows), 'half_passes': sum(row['full'] == 0 for row in rows), 'observed_procs': sorted({row['proc'] for row in rows})}


def continue_progress(source, destination, identity):
    result_path = source / 'result.json'
    report = json.loads(result_path.read_text())
    receipt = json.loads((source / 'gameplay-review.json').read_text())
    if (report.get('classification') != 'normal' or report.get('automatic_pass') is not True or
            receipt.get('result_sha256') != sha256(result_path) or receipt.get('all_review_points_passed') is not True):
        raise ValueError('Continuation requires reviewed normal-flow evidence, never an instrumented seed')
    for key in ('binary_sha256', 'package_sha256', 'sdk_commit', 'region'):
        if report.get(key) != identity[key]:
            raise ValueError('Continuation belongs to a different current binary/package/SDK/region')
    copied = {}
    for path in (source / 'manager/Data/dragon').glob('dragon-quest-slot[012].txt'):
        if path.is_symlink() or not path.is_file():
            raise ValueError('Invalid continued progress file')
        parse_progress(path)
        shutil.copy2(path, destination / path.name)
        copied[path.name] = sha256(path)
    if not copied:
        raise ValueError('Continuation has no verified quest progress')
    return {'source_result_sha256': sha256(result_path), 'progress_sha256': copied}


def compare_baseline(source, destination):
    previous = json.loads((source / 'scenario.json').read_text())
    current = json.loads((destination / 'scenario.json').read_text())
    for key in ('binary_sha256', 'package_sha256', 'sdk_commit', 'region', 'renderer', 'mode', 'source_save_sha256', 'route'):
        if previous[key] != current[key]:
            raise ValueError('Ordinary-actor control identity differs: ' + key)
    if previous.get('dragon_enabled') is not False:
        raise ValueError('Comparison requires a dragon-disabled control run')
    return {'control-identical:' + name: (source / name).is_file() and (destination / name).is_file() and sha256(source / name) == sha256(destination / name)
            for name in ('link.txt', 'camera.txt', 'sounds.txt', 'digest.txt', 'saveinfo.bin')}


def plan():
    return {'format_version': 1, 'matrix': {'regions': ['USA', 'EU'], 'renderers': ['metal', 'vulkan'], 'modes': list(MODES)},
            'coverage': COVERAGE,
            'route_template': {'format_version': 1, 'case': 'normal-quest', 'classification': 'normal', 'ready': False,
                'duration': 240, 'origin_frame': 3300, 'boot_press': [[frame, frame + 8, 0x8000] for frame in range(1200, 3200, 60)],
                'press': [], 'stick': [], 'rstick': [], 'capture_frames': [], 'full_save': [], 'full_load': [],
                'expect': {'progress': {'0': [4, 7]}, 'link_procs': []},
                'review_points': ['Uninterrupted travel and all three genuine sword-cut chimes', 'Letter and Medli dialogue consume only their intended controls', 'Six-note lesson unlock, native grapple and first flight']},
            'legacy_reference': 'Legacy dragon walkthrough identifies harbor (197720,94,-199470), cliff (201800,2562,-199900), outer lookout (209430,1900,-202600) and Medli lookout (202100,2562,-199900). Its travel used test teleports; those recordings are instrumented references, not normal-flow proof.',
            'state_policy': 'Never import v0.2.2 or other external full-state headers. Current-run save/load only; normal source save copied byte-for-byte. Retain binary/package/SDK hashes with every report.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write-plan', type=Path)
    parser.add_argument('--validate-only', action='store_true')
    parser.add_argument('--continue-from', type=Path, help='Reviewed normal current-base run; copy its quest files, never full states')
    parser.add_argument('--without-dragon', action='store_true', help='Disabled control for ordinary-actors only')
    parser.add_argument('--compare-baseline', type=Path, help='Exact current-base ordinary-actor disabled control artifacts')
    for key in ('binary', 'game', 'save', 'sdk', 'package', 'out', 'route'):
        parser.add_argument('--' + key, type=Path)
    parser.add_argument('--renderer', choices=('metal', 'vulkan'))
    parser.add_argument('--mode', choices=MODES, default='30')
    parser.add_argument('--timeout', type=int, default=600)
    parser.add_argument('--max-game-sessions', type=int, choices=range(1, 5), default=4)
    args = parser.parse_args()
    if args.write_plan:
        with args.write_plan.open('x') as output:
            json.dump(plan(), output, indent=2); output.write('\n')
        return 0
    if not args.route:
        parser.error('--route is required')
    route = validate_route(json.loads(args.route.read_text()))
    if args.validate_only:
        print(json.dumps({'valid': True, 'case': route['case'], 'classification': route['classification']})); return 0
    if any(getattr(args, key) is None for key in ('binary', 'game', 'save', 'sdk', 'package', 'out', 'renderer')):
        parser.error('Running requires binary/game/save/sdk/package/out/renderer')
    args.out = args.out.resolve(); args.sdk = args.sdk.resolve()
    repo = Path(__file__).resolve().parents[2]
    if args.out.exists() or any(args.out.is_relative_to(checkout) for checkout in (repo, args.sdk)):
        parser.error('Use a new private output directory outside both checkouts')
    sys.path.insert(0, str(args.sdk / 'tools/recomp'))
    import builds
    build = builds.identify(str(args.game.resolve() / 'code/cking.rpx'))
    if build is None:
        raise ValueError('Unsupported regional game; no address guessing')
    if route.get('fixture', {}).get('pokes') and route['fixture']['region'] != build.name:
        raise ValueError('Private poke fixture belongs to another region')
    sys.path.insert(0, str(args.sdk / 'tools/bench'))
    from run_bench import other_games, other_benchmarks
    if len(other_games()) >= args.max_game_sessions or other_benchmarks():
        raise ValueError('Functional game-session limit reached or benchmark running')
    if args.without_dragon and route['case'] != 'ordinary-actors':
        parser.error('Disabled control is limited to ordinary-actors')
    args.out.mkdir(parents=True)
    before = save_inventory(args.save.resolve())
    safe_copy_save(args.save.resolve(), args.out / 'save')
    if save_inventory(args.out / 'save') != before:
        raise ValueError('Normal save copy differs')
    manager = args.out / 'manager'; package_dir = manager / 'Mods/dragon'
    package_dir.mkdir(parents=True); unpack_package(args.package, package_dir)
    data = manager / 'Data/dragon'; data.mkdir(parents=True)
    sdk_commit = subprocess.check_output(['git', '-C', str(args.sdk), 'rev-parse', 'HEAD'], text=True).strip()
    base_identity = {'binary_sha256': sha256(args.binary), 'package_sha256': sha256(args.package), 'sdk_commit': sdk_commit, 'region': build.name}
    continued = continue_progress(args.continue_from.resolve(), data, base_identity) if args.continue_from else None
    if continued and route.get('fixture', {}).get('progress'):
        raise ValueError('Do not overwrite reviewed normal progress with a fixture')
    for slot, (phase, mask) in route.get('fixture', {}).get('progress', {}).items():
        (data / f'dragon-quest-slot{slot}.txt').write_text(f'WWHD_DRAGON_QUEST 1\n{phase} {mask}\n')
    (manager / 'profiles.json').write_text(json.dumps({'format_version': 1, 'active': 'Default', 'profiles': {'Default': {'enabled': {'dragon': not args.without_dragon}}}}))
    config = args.out / 'guest-sdk.json'
    config.write_text(json.dumps({'format_version': 1, 'python': [sys.executable], 'compiler': ['clang'], 'builder': str(args.sdk / 'tools/guestmod/build_guest_mod.py'), 'include': str(args.sdk / 'runtime/include')}))
    env = {key: value for key, value in os.environ.items() if not key.startswith('WWHD_')}
    env.update(WWHD_CODE_MODS='1', WWHD_NO_AUDIO='1', WWHD_NO_HOST_INPUT='1', WWHD_NO_GAMEPAD='1', WWHD_HIDDEN_WINDOWS='1', WWHD_UNCAPPED='1',
        WWHD_RENDERER_RUNTIME=args.renderer, WWHD_MOD_MANAGER_DIR=str(manager), WWHD_TEST_TRUST_NATIVE_MODS='dragon', WWHD_GUEST_BUILD_CONFIG=str(config),
        WWHD_SETTINGS=str(args.out / 'settings.ini'), WWHD_DISPLAY_SETTINGS=str(args.out / 'display.plist'), WWHD_CONTROLS=str(args.out / 'controls.json'),
        WWHD_SHADER_CACHE=str(args.out / 'shaders.bin'), WWHD_VK_SHADER_CACHE=str(args.out / 'vkshaders'), WWHD_VK_PIPELINE_CACHE=str(args.out / 'vkpipelines.bin'),
        WWHD_STATE_DIR=str(args.out / 'states'), WWHD_TEST_ORIGIN=str(route['origin_frame']), WWHD_TEST_END=str(route['duration']),
        WWHD_PRESS=timed_input(route.get('boot_press', []), 'press'), WWHD_TEST_PRESS=timed_input(route.get('press', []), 'press'),
        WWHD_TEST_STICK=timed_input(route.get('stick', []), 'stick'), WWHD_TEST_RSTICK=timed_input(route.get('rstick', []), 'stick'),
        WWHD_DUMP_FRAMES=','.join(map(str, route.get('capture_frames', []))), WWHD_DUMP_PRESENT='1', WWHD_SIM_SCREEN='1280x720',
        WWHD_LINK_TRACE='link.txt', WWHD_CAM_TRACE='camera.txt', WWHD_SE_TRACE='sounds.txt', WWHD_PROC_DIGEST='digest.txt', WWHD_SAVEINFO_DUMP='saveinfo.bin',
        WWHD_TRUE60='1' if args.mode == 'true60' else '0', WWHD_INTERP='0', WWHD_INTERP_FPS='60', WWHD_INTERP_PACED='0', XDG_CONFIG_HOME=str(args.out / 'config'))
    if args.mode == 'interp60':
        env['WWHD_INTERP_AT_STEP'] = str(route['origin_frame'] - 190)
    for key in ('full_save', 'full_load'):
        if route.get(key):
            env['WWHD_STATE_' + ('SAVE' if key == 'full_save' else 'LOAD') + '_AT'] = ','.join(f'{frame}:{slot}' for frame, slot in route[key])
    pokes = route.get('fixture', {}).get('pokes', [])
    if pokes:
        env['WWHD_TEST_POKE'] = ','.join(pokes)
    identity = {**base_identity, 'dragon_enabled': not args.without_dragon, 'continued_progress': continued,
                'region': build.name, 'renderer': args.renderer, 'mode': args.mode, 'source_save_sha256': before,
                'route': route, 'classification': route['classification'], 'full_state_policy': 'Current run only; no external states imported'}
    (args.out / 'scenario.json').write_text(json.dumps(identity, indent=2) + '\n')
    with (args.out / 'runtime.log').open('w') as log:
        process = subprocess.Popen([str(args.binary.resolve()), '--game', str(args.game.resolve()), '--save', str(args.out / 'save')], cwd=args.out, env=env, stdout=log, stderr=subprocess.STDOUT)
        print('Owned game PID:', process.pid, flush=True)
        try:
            deadline = time.monotonic() + args.timeout
            while process.poll() is None and not (args.out / 'test_done').exists():
                if time.monotonic() >= deadline:
                    raise RuntimeError('Runtime scenario timed out')
                if len(other_games(process.pid)) >= args.max_game_sessions or other_benchmarks():
                    raise RuntimeError('Functional session limit reached or benchmark started')
                time.sleep(1)
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill(); process.wait()
    checks, progress, trace = inspect_outputs(args.out, route, args.mode, not args.without_dragon)
    if args.compare_baseline:
        if route['case'] != 'ordinary-actors' or args.without_dragon:
            raise ValueError('Control comparison is only for enabled ordinary-actors runs')
        checks.update(compare_baseline(args.compare_baseline.resolve(), args.out))
    checks['source_save_unchanged'] = save_inventory(args.save.resolve()) == before
    report = {**identity, 'automatic_checks': checks, 'automatic_pass': all(checks.values()), 'actual_progress': progress, 'trace': trace,
              'status': 'NEEDS_GAMEPLAY_REVIEW' if all(checks.values()) else 'FAIL', 'pending_review': route['review_points'],
              'limitations': 'Automatic checks observe progress/trace/capture delivery, not semantic gameplay or visual parity. Instrumented runs never count as normal full flow. Audio melody is intentionally absent.'}
    (args.out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))
    return 2 if all(checks.values()) else 1


if __name__ == '__main__':
    raise SystemExit(main())
