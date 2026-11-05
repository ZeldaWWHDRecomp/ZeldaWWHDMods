#!/usr/bin/env python3
"""Test the policy and execute the actual translated PPC module for USA and EU."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import zipfile

HERE=Path(__file__).resolve().parent
REPO=Path(os.environ["WWHD_SDK"]).resolve()
OUT=HERE.parents[1]/'build/enhanced-controls'
CC=shlex.split(os.environ.get('CC','clang'))
CXX=shlex.split(os.environ.get('CXX','clang++'))
sys.path.insert(0,str(REPO/'tools/recomp'))
from builds import by_name


def run(args):
    subprocess.run([str(a) for a in args],check=True)


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    manifest=json.loads((HERE.parent/'manifest.json').read_text())
    archive=OUT/f"enhanced-controls-{manifest['version']}.wwhdmod"
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert json.loads(z.read('manifest.json'))==manifest
        assert z.read('src/mod.c')==(HERE.parent/'src/mod.c').read_bytes()
        assert z.read('src/core.h')==(HERE.parent/'src/core.h').read_bytes()
        assert z.read('src/editor.h')==(HERE.parent/'src/editor.h').read_bytes()
        assert {o['id'] for o in manifest['options']}=={'touch-enabled','editor-visible','editor-x','editor-y'}
    run(CC+['-std=c11','-Wall','-Wextra','-Werror',HERE/'core_test.c','-lm','-o',OUT/'core-test'])
    run([OUT/'core-test'])
    run(CC+['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function',HERE/'editor_test.c','-o',OUT/'editor-test'])
    run([OUT/'editor-test'])
    run(CXX+['-std=c++20','-O1','-I',REPO/'runtime/src','-c',HERE/'hud_bridge.cpp','-o',OUT/'hud-bridge.o'])
    for region in ('USA','EU'):
        b=by_name(region)
        defs={'PLAY_GET':b.code(0x025200d4),'PLAY':b.data(0x1046f0b0),'SAVE_PTR':b.data(0x101f84dc),
              'MENU_FLAG':b.data(0x101ea069),'LINK_EXECUTE':b.code(0x0240ebb0),
              'SPIN':b.code(0x023e6380),'EQUIPPING':b.code(0x023d794c),
              'GUARDING':b.code(0x02443728),'CURSE':b.data(0x101cef16)}
        (OUT/'fixture.h').write_text(''.join(f'#define {k} 0x{v:08X}u\n' for k,v in defs.items()))
        objects=[]
        for source in (OUT/f'translated-{region}.c',HERE/'module_test.c'):
            obj=OUT/f'{source.stem}.o'
            run(CC+['-std=c11','-O1','-I',REPO/'runtime/include','-I',OUT,'-c',source,'-o',obj])
            objects.append(obj)
        run(CXX+objects+[OUT/'hud-bridge.o','-lm','-o',OUT/f'module-test-{region}'])
        run([OUT/f'module-test-{region}'])
        print(region,'passed',flush=True)


if __name__=='__main__':
    main()
