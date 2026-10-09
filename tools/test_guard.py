import sys
from pathlib import Path
from guard import check, load_release_guard

release = load_release_guard(Path(sys.argv[1]))
for name, data in [('bad.rpx', b'synthetic'), ('bad.bti', b'synthetic'),
                   ('archive.bin', b'Yaz' + b'0' + b'synthetic'),
                   ('source.cpp', b'WWHD_' + b'FUNC(0x12345678)'),
                   ('source.cpp', b'gabi::' + b'call<'),
                   ('notes.txt', b'NOT FOR ' + b'PUBLICATION'),
                   ('key.txt', b'1' * 32), ('build/gen/table.c', b'synthetic')]:
    assert check(name, data, release), name
assert not check('mod.c', b'/* authored guest code */', release)
assert not check('assets/original.png', b'\x89PNG\r\n\x1a\nsynthetic original art', release)
assert check('assets/game.bti', b'synthetic', release)
assert check('textures/renamed.png', b'Yaz' + b'0' + b'synthetic', release)
print('Synthetic forbidden extension/magic/key/recompiler/non-public guard checks passed')

from guard import setup_policy
for name,data in [('hidden/.secret.py',b'# secret'),('object.dat',b'\x7fELFsynthetic'),('archive.txt',b'PK\x03\x04synthetic'),('mod.elf',b'text'),('mod.c',b'x'* (2*1024*1024+1)),('encoded.py',b"payload='"+b'A'*1100+b"'")]:
    assert check(name,data,release,source=True),name
assert not check('mod.c',b'/* readable guest source */',release,source=True)
assert not setup_policy('setup.py','import struct, zlib, math, json\nimport safe_io\ncontext=safe_io.arguments()\ndata=context.read_game(0,32)\ncontext.write_data("map.png",data)\n')
for source in ('import subprocess','from urllib.request import urlopen','import pathlib as p','eval("1")','getattr(object,"write")','x.__class__','open("outside","w")','x.write_bytes(b"x")','import importlib','from . import hidden'):
    assert setup_policy('setup.py',source),source
assert setup_policy('safe_io.py','import subprocess',helper=True)
assert setup_policy('safe_io.py','import os\nos.system("bad")',helper=True)
print('Source-only and setup AST checks passed; review still required')
