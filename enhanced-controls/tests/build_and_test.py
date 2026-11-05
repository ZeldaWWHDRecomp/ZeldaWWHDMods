#!/usr/bin/env python3
"""Build an guest .wwhdmod with the upstream guest SDK; no game required."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

MOD = Path(__file__).resolve().parents[1]
HERE = MOD / "src"
REPO = Path(os.environ["WWHD_SDK"]).resolve()
sys.path.insert(0, str(REPO / 'tools/guestmod'))
import build_guest_mod as builder
from builds import by_name


def run(*cmd):
    subprocess.run([str(x) for x in cmd], check=True)


def build(out, clang, lld):
    out.mkdir(parents=True, exist_ok=True)
    staging = out / 'package'
    staging.mkdir(exist_ok=True)
    manifest = json.loads((MOD / 'manifest.json').read_text())
    version = manifest['version']
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('Version must be a three-part number')
    run(clang, '--target=powerpc-unknown-eabi', '-mcpu=750', '-std=c11', '-O2',
        '-ffreestanding', '-fno-builtin', '-nostdlib', '-fno-jump-tables',
        '-ffunction-sections', '-fdata-sections', '-Wall', '-Wextra', '-Werror',
        '-I', REPO / 'runtime/guest/include', '-c', HERE / 'mod.c', '-o', out / 'mod.o')
    run(lld, '-m', 'elf32ppc', '-r', out / 'mod.o', '-o', staging / 'mod.elf')
    shutil.copyfile(MOD / 'manifest.json', staging / 'manifest.json')
    reports = []
    # The real install-time reader/translator checks both supported game regions.
    for region in ('USA', 'EU'):
        man, elf = builder.package_elf(staging)
        t = builder.translator_for(elf, 0x7f000000, by_name(region))
        assert all(target != by_name(region).code(0x0267598C) for _,target,_,_ in t.hooks)
        source = t.emit(man['id'])  # fails on unsupported PPC instructions
        (out / f'translated-{region}.c').write_text(source)
        report = builder.inspect_package(staging, 0x7f000000, by_name(region))
        report.update(region=region, services=t.services,
                      hooks=[{'kind': k, 'target': f'{target:08X}'} for k,target,_,_ in t.hooks])
        reports.append(report)
    (out / 'validation.json').write_text(json.dumps(reports, indent=2)+'\n')
    archive = out / f'enhanced-controls-{version}.wwhdmod'
    # Explicit allowlist: no files from game directories or generated native modules.
    files = {'manifest.json': staging/'manifest.json', 'mod.elf': staging/'mod.elf',
             'README.md': MOD/'README.md', 'LICENSE': MOD/'LICENSE',
             'src/mod.c': HERE/'mod.c', 'src/core.h': HERE/'core.h',
             'src/editor.h': HERE/'editor.h',
             'assets/NOTICE.md': MOD/'assets/NOTICE.md',
             'assets/ASSETS.md': MOD/'assets/ASSETS.md'}
    for header in ("editor_geometry.h","editor_view.h"):
        files[f"src/{header}"]=HERE/header
    for name in ('combat_jump_attack','combat_vertical_slash','combat_spin_attack','combat_dodge'):
        files[f'assets/{name}.png']=MOD/'assets'/f'{name}.png'
        import struct
        assert struct.unpack_from('>II',files[f'assets/{name}.png'].read_bytes(),16)==(256,256)
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        for name, path in sorted(files.items()):
            info = zipfile.ZipInfo(name, (2026,1,1,0,0,0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            z.writestr(info, path.read_bytes())
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert set(z.namelist()) == set(files)
        assert z.read('mod.elf')[:6] == b'\x7fELF\x01\x02'
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (out/'SHA256SUMS.txt').write_text(f'{digest}  {archive.name}\n')
    print(archive)
    return archive


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out',type=Path,default=MOD.parent/'build/enhanced-controls')
    p.add_argument('--clang',default=os.environ.get('WWHD_PPC_CLANG','clang'))
    p.add_argument('--lld',default=os.environ.get('WWHD_PPC_LLD','ld.lld'))
    args=p.parse_args()
    build(args.out.resolve(),args.clang,args.lld)
