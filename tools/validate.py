#!/usr/bin/env python3
"""Validate generated catalogue metadata against every packaged manifest and payload."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
from urllib.parse import urlsplit
import zipfile


def require(condition, message):
    if not condition: raise ValueError(message)


def https(value):
    parsed = urlsplit(value)
    return (value.startswith('https://') and bool(parsed.hostname) and not parsed.username
            and not parsed.password and not parsed.fragment and not parsed.query and len(value) <= 2048
            and all(ord(c) > 32 and ord(c) != 127 and c != '\\' for c in value))


def metadata(index):
    require(index.get('format_version') == 1 and isinstance(index.get('mods'), list), 'Invalid catalogue format')
    result = {}
    for entry in index['mods']:
        ident = entry['id']
        require(re.fullmatch(r'[a-z0-9][a-z0-9_-]{0,63}', ident) and ident not in result, 'Invalid or duplicate ID')
        require(re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', entry['version']), 'Invalid version')
        require(entry['kind'] == 'guest', 'This workflow packages guest mods only')
        require(entry['authors'] and entry['licences'] and entry['builds'], 'Missing author, licence or game builds')
        require(set(entry['builds']) <= {'USA', 'EU'}, 'Unknown game build')
        require(entry['port_versions'].get('minimum'), 'Missing minimum port version')
        require(set(entry['downloads']) == {'all'}, 'Guest package must be platform independent')
        download = entry['downloads']['all']
        require(https(download['url']), 'Absolute HTTPS download URL required')
        require(re.fullmatch('[0-9a-f]{64}', download['sha256']), 'Invalid package SHA-256')
        require(type(download['size']) is int and 0 < download['size'] <= 256 * 1024 * 1024, 'Invalid package size')
        require(any(step.get('type') == 'build_guest_mod' for step in entry['setup']), 'Missing guest build setup')
        result[ident] = {key: value for key, value in entry.items() if key != 'downloads'}
    return result


def validate(directory, root_index=None):
    index = json.loads((directory / 'index.json').read_text())
    described = metadata(index)
    expected = set()
    for entry in index['mods']:
        archive = directory / (entry['id'] + '.zip')
        expected.add(archive.name)
        data = archive.read_bytes()
        download = entry['downloads']['all']
        require(len(data) == download['size'] and hashlib.sha256(data).hexdigest() == download['sha256'], 'Package checksum/size mismatch')
        with zipfile.ZipFile(archive) as bundle:
            names = bundle.namelist()
            require(len(set(names)) == len(names), 'Duplicate ZIP paths')
            for name in names:
                path = PurePosixPath(name)
                require(not path.is_absolute() and '..' not in path.parts and ':' not in name and '\\' not in name, 'Unsafe ZIP path')
            manifest = json.loads(bundle.read('manifest.json'))
            for key in ['id', 'version', 'kind', 'setup']:
                require(manifest.get(key) == entry[key], 'Manifest/catalogue mismatch: ' + key)
            require([manifest['author']] == entry['authors'], 'Manifest author mismatch')
            require(manifest.get('game_id') == 'wwhd-usa', 'Guest authoring addresses must target USA')
            require([dep['id'] for dep in manifest.get('dependencies', [])] == entry['requires'], 'Dependency mismatch')
            elf = bundle.read(manifest['guest']['elf'])
            require(elf[:6] == b'\x7fELF\x01\x02' and elf[18:20] == b'\x00\x14', 'Not a big-endian PowerPC ELF')
            require('LICENSE' in names and 'src/mod.c' in names, 'Missing licence/source')
    require({path.name for path in directory.glob('*.zip')} == expected, 'Unexpected ZIP in artifact folder')
    if root_index:
        # Binary hashes depend on compiler version; source metadata must match the committed index.
        source=json.loads(root_index.read_text())
        require(not any('downloads' in entry for entry in source['mods']), 'Source metadata cannot provide download URLs/hashes')
        require({entry['id']:entry for entry in source['mods']} == described, 'Source metadata is stale; regenerate it')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--root-index', type=Path)
    args = parser.parse_args()
    validate(args.directory, args.root_index)
