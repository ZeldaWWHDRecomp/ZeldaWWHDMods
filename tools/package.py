#!/usr/bin/env python3
"""Build platform-independent guest packages against the pinned public SDK."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import subprocess
import zipfile
from guard import scan, setup_files
from validate import validate, https

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


def package_root(base_url, channel, revision):
    if channel not in {'main','devel'}: raise ValueError('Unknown package channel')
    if not base_url or not https(base_url): raise ValueError('Absolute HTTPS hosting base required')
    if not re.fullmatch('[0-9a-f]{40}',revision): raise ValueError('Full source commit required')
    return base_url.rstrip('/')+'/'+channel+'-'+revision


def build(sdk, out, clang, lld, base_url, channel='devel', revision=None):
    if subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']):
        raise ValueError('Source checkout must be clean: immutable package tags identify committed source')
    actual_revision=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
    if revision is not None and revision!=actual_revision: raise ValueError('Source revision must match checkout')
    revision=actual_revision
    base_url = package_root(base_url,channel,revision)
    source = json.loads((ROOT/'catalogue.json').read_text())
    if any('downloads' in entry for entry in source['mods']): raise ValueError('Source metadata must not supply downloads/hashes')
    declared={entry['id']:entry for entry in source['mods']}
    if len(declared)!=len(source['mods']) or any(not re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,63}',name) for name in declared): raise ValueError('Invalid or duplicate source mod ID')
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
    for name in sorted(declared):
        folder = ROOT / name
        manifest = json.loads((folder / 'manifest.json').read_text())
        if manifest.get('kind')!='guest' or manifest.get('id')!=name: raise ValueError('Invalid declared guest folder')
        source_file=folder/'src/mod.c' if (folder/'src/mod.c').exists() else folder/'mod.c'
        obj, elf = out / (name + '.o'), out / (name + '.elf')
        subprocess.run([clang, '--target=powerpc-unknown-eabi', '-mcpu=750', '-O2', '-ffreestanding',
                        '-fno-builtin', '-nostdlib', '-fno-jump-tables', '-ffunction-sections', '-fdata-sections',
                        '-I' + str(sdk / 'runtime/guest/include'), '-I'+str(source_file.parent), '-c', str(source_file), '-o', str(obj)], check=True)
        subprocess.run([lld, '-m', 'elf32ppc', '-r', str(obj), '-o', str(elf)], check=True)
        archive = out / (name + '.zip')
        payload = {file.name: file.read_bytes() for file in [folder / 'manifest.json', folder / 'README.md', folder / 'LICENSE']}
        payload['src/mod.c'] = source_file.read_bytes()
        for header in sorted(source_file.parent.glob('*.h')):
            if header.is_symlink(): raise ValueError('Source header symlink refused')
            payload['src/'+header.name]=header.read_bytes()
        for tool in setup_files(folder,manifest): payload[tool]=(folder/tool).read_bytes()
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
        entry=dict(declared[name])
        for key in ('id','kind','version','setup'):
            if entry.get(key)!=manifest.get(key): raise ValueError('Source metadata/manifest mismatch: '+key)
        entry['downloads']={'all':dict(url=base_url+'/'+archive.name,size=len(data),sha256=hashlib.sha256(data).hexdigest())}
        entries.append(entry)
    (out / 'index.json').write_text(json.dumps(dict(format_version=1, mods=entries), indent=2) + '\n')
    validate(out, ROOT / 'catalogue.json')
    problems = scan(out, sdk, packages=True)
    if problems: raise ValueError('\n'.join(problems))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/packages')
    parser.add_argument('--clang', default='clang')
    parser.add_argument('--lld', default='ld.lld')
    parser.add_argument('--base-url', required=True, help='HTTPS release/download root; channel-commit path appended')
    parser.add_argument('--channel',choices=['main','devel'],default='devel')
    parser.add_argument('--revision')
    args = parser.parse_args()
    build(args.sdk.resolve(), args.out.resolve(), args.clang, args.lld, args.base_url,args.channel,args.revision)
