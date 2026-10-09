"""Combined packages preserve authored paths without allowing filesystem escapes."""
from pathlib import Path
import tempfile
from package import owned_payload


def rejects(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError('Unsafe package was accepted')


with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    for name in ('assets/icon.png', 'textures/tile.png', 'content/Common/authored.txt'):
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b'original synthetic fixture')
    payload = owned_payload(root, {})
    assert set(payload) == {'assets/icon.png', 'textures/tile.png', 'content/Common/authored.txt'}
    assert all(value == b'original synthetic fixture' for value in payload.values())
    for name in ('../escape', '/absolute', 'C:/absolute', 'nested\\escape', '', '.', 'assets/../textures', None):
        rejects(lambda: owned_payload(root, {'content_dir': name}))
    rejects(lambda: owned_payload(root, {'content_dir': 'missing'}))
    link = root / 'assets/link.png'
    try:
        link.symlink_to(root / 'textures/tile.png')
    except OSError:
        pass  # Windows runners may lack symlink privileges.
    else:
        rejects(lambda: owned_payload(root, {}))
        link.unlink()
        (root / 'alias').symlink_to(root / 'content', target_is_directory=True)
        rejects(lambda: owned_payload(root, {'content_dir': 'alias/Common'}))
print('Combined package path preservation/missing-directory/escape/symlink checks passed')

from package import package_root
revision='1'*40
main=package_root('https://example.invalid/releases/download','main',revision)
devel=package_root('https://example.invalid/releases/download','devel',revision)
assert main != devel and '/main-'+revision in main and '/devel-'+revision in devel
for base,channel,commit in [('relative','main',revision),('http://example.invalid','main',revision),('https://user:pass@example.invalid','main',revision),('https://example.invalid/#fragment','main',revision),('https://example.invalid','../main',revision),('https://example.invalid','main','short')]:
    rejects(lambda: package_root(base,channel,commit))
print('Immutable absolute package channel URL checks passed')
