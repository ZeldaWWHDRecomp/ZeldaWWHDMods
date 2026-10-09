#!/usr/bin/env python3
"""Extend the pinned port release guard for mod sources and package payloads."""
import argparse
import importlib.util
from pathlib import Path
import re
import subprocess
import zipfile

EXTENSIONS = {'.rpx', '.rpl', '.wud', '.wux', '.wua', '.szs', '.pack', '.bfres', '.sarc',
              '.arc', '.bfsar', '.bfstm', '.msbt', '.bmg', '.bti', '.bdl', '.bmd', '.gcm', '.iso', '.sav'}
MAGIC = (b'Yaz' + b'0', b'SA' + b'RC', b'RA' + b'RC', b'FRE' + b'S', b'FS' + b'AR', b'FS' + b'TM')
FORBIDDEN = (b'NOT FOR ' + b'PUBLICATION', b'gabi::' + b'call<')
PATTERNS = [re.compile(rb'WWHD_' + rb'FUNC\s*\(\s*0x'), re.compile(rb'VER' + rb'IFY\s*\(\s*0x')]


def load_release_guard(sdk):
    spec = importlib.util.spec_from_file_location('port_guard', sdk / 'tools/release/guard.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def check(name, data, release):
    problems = []
    release.check_entry(name, data, problems)
    if Path(name).suffix.lower() in EXTENSIONS:
        problems.append(name + ': game payload extension')
    if data.startswith(MAGIC):
        problems.append(name + ': game archive/resource magic')
    if any(marker in data for marker in FORBIDDEN) or any(pattern.search(data) for pattern in PATTERNS):
        problems.append(name + ': non-public or recompiled material marker')
    return problems


def scan(root, sdk, packages=False):
    release = load_release_guard(sdk)
    problems = []
    if packages:
        paths = sorted(root.glob('*.zip')) + [root / 'index.json']
    else:
        names = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z', '--cached', '--others', '--exclude-standard'])
        paths = [root / name.decode() for name in names.split(b'\0') if name]
    for path in paths:
        name = path.relative_to(root).as_posix()
        if path.is_symlink():
            problems.append(name + ': symlink refused');continue
        data = path.read_bytes()
        problems.extend(check(name, data, release))
        if path.suffix.lower() == '.zip':
            with zipfile.ZipFile(path) as archive:
                for info in archive.infolist():
                    if info.is_dir(): continue
                    if info.file_size > 128 * 1024 * 1024:
                        problems.append(info.filename + ': oversized package file');continue
                    if (info.external_attr >> 16) & 0o170000 == 0o120000:
                        problems.append(info.filename + ': package symlink refused')
                    problems.extend(check(name + '/' + info.filename, archive.read(info), release))
    return problems


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--packages', action='store_true')
    args = parser.parse_args()
    problems = scan(args.root.resolve(), args.sdk.resolve(), args.packages)
    for problem in problems: print(problem)
    raise SystemExit(bool(problems))
