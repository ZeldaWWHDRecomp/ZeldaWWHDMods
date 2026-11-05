#!/usr/bin/env python3
"""Compile, translate and execute both regional guest modules, then package."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import zipfile
MOD = Path(__file__).resolve().parents[1]
HERE = MOD / "src"
REPO = Path(os.environ["WWHD_SDK"]).resolve()
def run(args):
    subprocess.run([str(x) for x in args], cwd=REPO, check=True)
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', type=Path, default=MOD.parent/'build/sword-wave')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    clang = os.environ.get('WWHD_PPC_CLANG', 'clang')
    lld = os.environ.get('WWHD_PPC_LLD', 'ld.lld')
    host = shlex.split(os.environ.get('WWHD_HOST_CC', clang))
    run([clang, '--target=powerpc-unknown-eabi', '-mcpu=750', '-std=c11', '-O2',
         '-ffreestanding', '-fno-builtin', '-nostdlib', '-fno-jump-tables',
         '-ffunction-sections', '-fdata-sections', '-Wall', '-Wextra', '-Werror',
         '-I', REPO/'runtime/guest/include', '-c', HERE/'mod.c', '-o', out/'mod.o'])
    run([lld, '-m', 'elf32ppc', '-r', out/'mod.o', '-o', out/'mod.elf'])
    manifest = json.loads((MOD/'manifest.json').read_text())
    sys.path.insert(0, str(REPO/'tools/guestmod'))
    import build_guest_mod as builder
    from builds import by_name
    addresses = {'DIRECTION':0x028304F0, 'PARTICLES':0x0281FBD8, 'PLAY':0x025200D4, 'LINK':0x0240EBB0, 'DELETE':0x0240EE28,
        'SPIN':0x023E6380, 'ROLL':0x02441A44, 'PARTICLE':0x025A847C,
        'END':0x025A9D64, 'CAPSULE':0x024445B0, 'STATUS':0x02375AB8,
        'STATUS_INIT':0x02515F14, 'STATUS_MOVE':0x02515E50, 'RESET_HIT':0x02516138,
        'COLLISION':0x0200E240, 'LINE_CTOR':0x02008FEC, 'LINE_SET':0x024F1AFC,
        'LINE_CROSS':0x02008860, 'PAUSE':0x025AF2A4, 'MENU':0x025986BC, 'LOAD':0x02721908}
    for region in ('USA','EU'):
        build = by_name(region)
        translated = builder.translator_for((out/'mod.elf').read_bytes(), 0x7F000000, build)
        assert len(translated.hooks)==10 and all(h[0]!=1 for h in translated.hooks)
        source = out/('translated-'+region+'.c')
        source.write_text(translated.emit(manifest['id']))
        module = out/('translated-'+region+'.so')
        run(host+['-O1','-fPIC','-shared','-ffp-contract=off','-fno-strict-aliasing','-w',
                  '-I',REPO/'runtime/include',source,'-o',module])
        header=''.join(f'#define GAME_{name} 0x{build.code(a):08X}u\n' for name,a in addresses.items())
        header+=f'#define SAVE_PTR 0x{build.data(0x101F84DC):08X}u\n'
        header+=f'#define SINCOS 0x{build.data(0x104A44F8):08X}u\n'
        (out/'test_addresses.h').write_text(header)
        run(host+['-std=c11','-O1','-Wall','-Wextra','-Werror','-I',REPO/'runtime/include','-I',out,
                  MOD/'tests/adapter_test.c','-ldl','-lm','-o',out/'adapter-test'])
        run([out/'adapter-test',module])
        print(region, 'translation and execution passed', flush=True)
    archive=out/('sword-wave-'+manifest['version']+'.wwhdmod')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        z.write(out/'mod.elf','mod.elf')
        for name in ('manifest.json','README.md'):
            z.write(MOD/name,name)
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert z.read('mod.elf')==(out/'mod.elf').read_bytes()
        assert json.loads(z.read('manifest.json'))==manifest
    print(archive)
if __name__=='__main__':
    main()
