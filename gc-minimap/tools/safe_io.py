"""Reviewed local setup I/O: read a selected dump, write flat files to mod data.

This is a capability boundary for audited setup code, not an operating-system sandbox.
No subprocesses, network access or executable loading belong in this module.
"""
import argparse
import os
import tempfile
from pathlib import Path

MAX_READ = 32 * 1024 * 1024
MAX_OUTPUT = 1024 * 1024


def _without_links(path):
    path = Path(os.path.abspath(path))
    for part in (path, *path.parents):
        if part.is_symlink():
            raise ValueError('Symbolic links are not accepted for setup storage')
    return path


def _flat(name):
    if not isinstance(name, str) or not name or len(name) > 128:
        raise ValueError('Invalid mod-data filename')
    if name.startswith('.') or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_. ' for c in name):
        raise ValueError('Mod-data filenames must be flat and visible')
    return name


class Context:
    def __init__(self, source, data):
        # Resolve platform aliases such as macOS /var before establishing roots.
        # The selected leaf itself must never be an alias to another file/tree.
        source, data = Path(source).expanduser(), Path(data).expanduser()
        if source.is_symlink() or data.is_symlink():
            raise ValueError('Selected setup roots must not be symbolic links')
        self._source = _without_links(source.parent.resolve() / source.name)
        self._data = _without_links(data.parent.resolve() / data.name)
        if not self._source.is_file() and not self._source.is_dir():
            raise ValueError('Selected game source is unavailable')
        if self._data == self._source or self._data.is_relative_to(self._source):
            raise ValueError('Mod data must be separate from the game source')
        if any((parent / '.git').exists() for parent in (self._data, *self._data.parents)):
            raise ValueError('Derived map data must stay outside Git checkouts')
        self._data.mkdir(parents=True, exist_ok=True)
        _without_links(self._data)

    def is_disc(self):
        """Describe the selected input without exposing its filesystem path."""
        return _without_links(self._source).is_file()

    def read_game(self, offset, size):
        """Read a bounded range of a selected plain disc image, without mutation."""
        if not isinstance(offset, int) or not isinstance(size, int) or offset < 0 or not 0 <= size <= MAX_READ:
            raise ValueError('Invalid game-source read range')
        source = _without_links(self._source)
        if not source.is_file():
            raise ValueError('This operation needs a plain disc image')
        try:
            nofollow = os.O_NOFOLLOW
        except AttributeError:
            nofollow = 0
        descriptor = os.open(source, os.O_RDONLY | nofollow)
        with os.fdopen(descriptor, 'rb') as stream:
            length = os.fstat(stream.fileno()).st_size
            if offset > length or size > length - offset:
                raise ValueError('Truncated game-source range')
            stream.seek(offset)
            value = stream.read(size)
        if len(value) != size:
            raise ValueError('Truncated game-source range')
        return value

    def read_input(self, relative):
        """Read one relative file from the selected extracted game folder."""
        if not isinstance(relative, str) or '\\' in relative or ':' in relative:
            raise ValueError('Invalid relative game-source path')
        parts = relative.split('/')
        if any(not part or part in ('.', '..') or part.startswith('.') for part in parts):
            raise ValueError('Invalid relative game-source path')
        source = _without_links(self._source)
        if not source.is_dir():
            raise ValueError('This operation needs an extracted game folder')
        target = _without_links(source.joinpath(*parts))
        try:
            nofollow = os.O_NOFOLLOW
        except AttributeError:
            nofollow = 0
        descriptor = os.open(target, os.O_RDONLY | nofollow)
        with os.fdopen(descriptor, 'rb') as stream:
            if os.fstat(stream.fileno()).st_size > MAX_READ:
                raise ValueError('Game-source file exceeds the setup read cap')
            value = stream.read(MAX_READ + 1)
        if len(value) > MAX_READ:
            raise ValueError('Game-source file exceeds the setup read cap')
        return value

    def write_data(self, name, value):
        """Write a bounded flat output only inside the selected mod-data folder."""
        name = _flat(name)
        if not isinstance(value, bytes) or len(value) > MAX_OUTPUT:
            raise ValueError('Split mod-data outputs larger than 1 MiB')
        directory = _without_links(self._data)
        target = _without_links(directory / name)
        # Replacing a fresh file also avoids modifying an existing hard-link target.
        descriptor, temporary = tempfile.mkstemp(prefix='.wwhd-setup-', dir=directory)
        try:
            with os.fdopen(descriptor, 'wb') as stream:
                stream.write(value)
            os.replace(temporary, target)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)


def arguments(argv=None):
    parser = argparse.ArgumentParser(description='Prepare local minimap data from your own game dump')
    parser.add_argument('--source', required=True)
    parser.add_argument('--data', required=True)
    args = parser.parse_args(argv)
    return Context(args.source, args.data)
