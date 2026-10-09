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
