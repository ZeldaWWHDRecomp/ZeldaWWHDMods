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

assert check('bytes.c',b'const char data[]={'+b'0x12,'*130+b'};',release,source=True)
assert check('nested/.hidden.py',b'pass',release,source=True)
assert check('.github/.hidden',b'pass',release,source=True)
assert check('control.py',b'pass'+bytes([1]),release,source=True)

# These strings are parsed only; none of their effects are ever executed.
for source in ('from safe_io import os', 'import safe_io\nsafe_io.os',
               'import safe_io as capability\ncapability.os',
               'import safe_io as capability\nother=capability',
               'import safe_io\nsafe_io.arguments(["--data","elsewhere"])',
               'from safe_io import arguments as context',
               'from prepare import safe_io as capability',
               'from json import __builtins__ as hidden',
               'context._data', 'context._source'):
    assert setup_policy('setup.py',source,local_modules=['prepare']),source
assert not setup_policy('setup.py','import safe_io as capability\ncontext=capability.arguments()\ncontext.write_data("map.png",b"data")')

# Exercise the same trusted-base diff function used by PR CI against a real repo.
import subprocess
import tempfile
from guard import protected_pr_changes
with tempfile.TemporaryDirectory(prefix='trusted-policy-') as temporary:
    root=Path(temporary)
    def git(*args):
        return subprocess.check_output(['git','-C',str(root),*args],text=True).strip()
    git('init','-q')
    git('config','user.name','Synthetic policy test')
    git('config','user.email','policy@example.invalid')
    for name in ['tools/package.py','tools/validate.py','tools/test_package.py','gc-minimap/tools/safe_io.py','index.json','sdk.json','.github/workflows/packages.yml','.github/CODEOWNERS','gc-minimap/mod.c']:
        path=root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('authored synthetic baseline\n')
    git('add','.');git('commit','-qm','authored baseline');base=git('rev-parse','HEAD')
    for path in root.rglob('*'):
        if path.is_file() and '.git' not in path.parts:path.write_text('authored synthetic change\n')
    git('add','.');git('commit','-qm','authored changes')
    problems=protected_pr_changes(root,base)
    for name in ['tools/package.py','tools/validate.py','tools/test_package.py','gc-minimap/tools/safe_io.py','index.json','sdk.json','.github/workflows/packages.yml','.github/CODEOWNERS']:
        assert any(problem.startswith(name+':') for problem in problems),name
    assert not any(problem.startswith('gc-minimap/mod.c:') for problem in problems)
print('Trusted-base policy protects all CI entrypoints and setup capability context')
