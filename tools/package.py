#!/usr/bin/env python3
"""Build platform-independent guest packages against the pinned public SDK."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import zipfile
from guard import scan
from validate import validate

ROOT = Path(__file__).resolve().parents[1]


def owned_payload(folder, manifest):
    """Preserve combined-package paths; source and ZIP guards still scan every byte."""
    directories = ['assets', 'textures', manifest.get('content_dir', 'content')]
    payload = {}
    for name in directories:
        if not isinstance(name, str): raise ValueError('Unsafe combined-package directory')
        relative = Path(name)
        if not name or '\\' in name or ':' in name or relative.is_absolute() or any(
                part in ('', '.', '..') for part in name.split('/')):
            raise ValueError('Unsafe combined-package directory')
        root = folder / relative
        checked = folder
        for part in relative.parts:
            checked /= part
            if checked.is_symlink(): raise ValueError('Combined-package symlink refused')
        if not root.exists():
            if 'content_dir' in manifest and name == manifest['content_dir']:
                raise ValueError('Declared content directory is missing')
            continue
        if not root.is_dir(): raise ValueError('Combined-package path must be a directory')
        for path in sorted(root.rglob('*')):
            if path.is_symlink(): raise ValueError('Combined-package symlink refused')
            if path.is_dir(): continue
            if not path.is_file(): raise ValueError('Unsupported combined-package file')
            if path.stat().st_size > 128 * 1024 * 1024:
                raise ValueError('Combined-package file exceeds 128 MiB')
            payload[path.relative_to(folder).as_posix()] = path.read_bytes()
            if len(payload) > 4096 or sum(map(len, payload.values())) > 512 * 1024 * 1024:
                raise ValueError('Combined-package inventory exceeds limits')
    return payload


def build(sdk, out, clang, lld, base_url=None):
    pin = json.loads((ROOT / 'sdk.json').read_text())
    actual = subprocess.check_output(['git', '-C', str(sdk), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != pin['commit']:
        raise ValueError('SDK checkout does not match sdk.json')
    # Tracked modifications could alter declarations or the inherited guard.
    if subprocess.check_output(['git', '-C', str(sdk), 'status', '--porcelain', '--untracked-files=no']):
        raise ValueError('SDK checkout has tracked modifications')
    problems = scan(ROOT, sdk)
    if problems: raise ValueError('\n'.join(problems))
    out.mkdir(parents=True, exist_ok=True)
    entries = []
    for name in ['heart-ticker']:
        folder = ROOT / name
        manifest = json.loads((folder / 'manifest.json').read_text())
        obj, elf = out / (name + '.o'), out / (name + '.elf')
        subprocess.run([clang, '--target=powerpc-unknown-eabi', '-mcpu=750', '-O2', '-ffreestanding',
                        '-fno-builtin', '-nostdlib', '-fno-jump-tables', '-ffunction-sections', '-fdata-sections',
                        '-I' + str(sdk / 'runtime/guest/include'), '-c', str(folder / 'mod.c'), '-o', str(obj)], check=True)
        subprocess.run([lld, '-m', 'elf32ppc', '-r', str(obj), '-o', str(elf)], check=True)
        archive = out / (name + '.zip')
        payload = {file.name: file.read_bytes() for file in [folder / 'manifest.json', folder / 'README.md', folder / 'LICENSE']}
        payload['src/mod.c'] = (folder / 'mod.c').read_bytes()
        payload['mod.elf'] = elf.read_bytes()
        payload.update(owned_payload(folder, manifest))
        with zipfile.ZipFile(archive, 'w') as bundle:
            for path, data in sorted(payload.items()):
                info = zipfile.ZipInfo(path, date_time=(2026, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                bundle.writestr(info, data)
        obj.unlink();elf.unlink()
        data = archive.read_bytes()
        url = base_url.rstrip('/') + '/' + archive.name if base_url else archive.name
        if base_url and not base_url.startswith('https://'): raise ValueError('Package base URL must use HTTPS')
        entries.append(dict(id=name, name=manifest['name'], description=manifest['description'],
                            authors=[manifest['author']], version=manifest['version'], kind='guest',
                            licences=['MPL-2.0'], builds=['USA', 'EU'], requires=[], setup=manifest['setup'],
                            port_versions={'minimum': pin['minimum_port']},
                            downloads={'all': dict(url=url, size=len(data), sha256=hashlib.sha256(data).hexdigest())}))
    (out / 'index.json').write_text(json.dumps(dict(format_version=1, mods=entries), indent=2) + '\n')
    validate(out, ROOT / 'index.json')
    problems = scan(out, sdk, packages=True)
    if problems: raise ValueError('\n'.join(problems))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/packages')
    parser.add_argument('--clang', default='clang')
    parser.add_argument('--lld', default='ld.lld')
    parser.add_argument('--base-url')
    args = parser.parse_args()
    build(args.sdk.resolve(), args.out.resolve(), args.clang, args.lld, args.base_url)
