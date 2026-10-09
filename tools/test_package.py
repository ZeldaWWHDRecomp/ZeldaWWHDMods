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

from package import generated_art
art=generated_art(Path(__file__).resolve().parents[1]/'gc-minimap','generate_art.py')
assert art and art==generated_art(Path(__file__).resolve().parents[1]/'gc-minimap','generate_art.py')
assert all(name.startswith('assets/') and data.startswith(b'\x89PNG\r\n\x1a\n') for name,data in art.items())
rejects(lambda: generated_art(Path('.'),'../generate_art.py'))
print('Original art generation is reproducible and confined to assets paths')


# Exercise the complete package assembly without requiring a compiler: the fake
# compiler writes authored marker bytes while we verify command count and ZIP closure.
import json
import zipfile
from unittest import mock
import package

with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    folder = root / 'unity-mod'
    source_dir = folder / 'src'
    source_dir.mkdir(parents=True)
    sources = {'mod.c': '#include "helper.c"\n',
               'helper.c': '#include "helper.h"\nint fixture(void) { return VALUE; }\n',
               'helper.h': '#define VALUE 7\n'}
    for name, data in sources.items(): (source_dir / name).write_text(data)
    (folder / 'README.md').write_text('Authored unity fixture\n')
    (folder / 'LICENSE').write_text('Synthetic test licence\n')
    manifest = dict(format_version=1, id='unity-mod', kind='guest', version='0.1.0', setup=[])
    (folder / 'manifest.json').write_text(json.dumps(manifest))
    (root / 'catalogue.json').write_text(json.dumps(dict(format_version=1, mods=[manifest])))
    revision = 'a' * 40
    (root / 'sdk.json').write_text(json.dumps(dict(commit=revision)))
    def git_output(command, **kwargs):
        return revision if kwargs.get('text') else b''
    def compiler(command, **kwargs):
        Path(command[command.index('-o') + 1]).write_bytes(b'authored compiler marker')
    with mock.patch.object(package, 'ROOT', root), \
         mock.patch.object(package.subprocess, 'check_output', side_effect=git_output), \
         mock.patch.object(package.subprocess, 'run', side_effect=compiler) as compile_commands, \
         mock.patch.object(package, 'scan', return_value=[]), \
         mock.patch.object(package, 'validate'):
        package.build(root / 'sdk', root / 'out', 'clang-fixture', 'lld-fixture', 'https://example.invalid/releases', revision=revision)
    commands = [call.args[0] for call in compile_commands.call_args_list]
    assert len(commands) == 2  # one compile, one relocatable link
    assert commands[0][0] == 'clang-fixture' and commands[0][commands[0].index('-c') + 1] == str(source_dir / 'mod.c')
    assert commands[1][0] == 'lld-fixture'
    with zipfile.ZipFile(root / 'out/unity-mod.zip') as archive:
        for name, data in sources.items(): assert archive.read('src/' + name).decode() == data
    # The older root-level entrypoint uses the same packaged src/ layout.
    root_style = root / 'root-style'
    root_style.mkdir()
    for name, data in sources.items(): (root_style / name).write_text(data)
    source, payload = package.guest_sources(root_style)
    assert source == root_style / 'mod.c' and set(payload) == {'src/' + name for name in sources}
    try:
        (root_style / 'alias.c').symlink_to(root_style / 'helper.c')
    except OSError:
        pass
    else:
        rejects(lambda: package.guest_sources(root_style))
print('Unity helper C/header ZIP closure and single compile/link checks passed')
