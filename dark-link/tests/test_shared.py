#!/usr/bin/env python3
"""Run actual translated Dark Link + Enhanced Controls together; no game required.
Build both packages first. Pass --enhanced-root pointing at enhanced-mod checkout.
"""
import argparse,os,shlex,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
ROOT=Path(os.environ["WWHD_SDK"]).resolve()
CHECKOUT=HERE.parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--enhanced-root',type=Path,required=True)
p.add_argument('--cc',default=os.environ.get('CC','clang'))
p.add_argument('--cxx',default=os.environ.get('CXX','clang++'))
a=p.parse_args();cc=shlex.split(a.cc);cxx=shlex.split(a.cxx)
sys.path.insert(0,str(ROOT/'tools/guestmod'))
import build_guest_mod as builder
from builds import by_name
out=CHECKOUT/'build/shared-touch';out.mkdir(parents=True,exist_ok=True)
peer=a.enhanced_root.resolve()
for name in ('editor.h','editor_geometry.h','editor_view.h'):
    assert (HERE.parent/"src"/name).read_bytes()==(peer/'enhanced-controls/src'/name).read_bytes(),name
mods=[]
for root,folder,base in ((CHECKOUT,'dark-link',0x7f000000),(peer,'enhanced-controls',0x7f200000)):
    r=builder.build(str(root/'build'/folder/'package'),str(out/'cache'),base,cc,str(ROOT/'runtime/include'),game_build=by_name('USA'))
    mods.append(r['module'])
def run(cmd):subprocess.run([str(x) for x in cmd],check=True)
run(cc+['-std=c11','-O1','-UNDEBUG','-I'+str(ROOT/'runtime/include'),'-c',HERE/'test_shared.c','-o',out/'driver.o'])
run(cxx+['-I'+str(ROOT/'runtime/src'),'-std=c++20','-O1','-UNDEBUG','-c',HERE/'test_shared_hud.cpp','-o',out/'hud.o'])
run(cxx+[out/'driver.o',out/'hud.o','-ldl','-o',out/'test-shared'])
run([out/'test-shared',*mods])
