"""Mutate copies of authored package output to prove catalogue checks fail closed."""
import json
from pathlib import Path
import shutil
import sys
import tempfile
from validate import validate


def rejects(action):
    try: action()
    except ValueError: return
    raise AssertionError('Invalid catalogue was accepted')


with tempfile.TemporaryDirectory(prefix='mod-catalogue-check-') as temporary:
    root = Path(temporary)
    for file in Path(sys.argv[1]).iterdir():
        if file.suffix == '.zip' or file.name == 'index.json': shutil.copyfile(file, root / file.name)
    original = (root / 'index.json').read_text()
    validate(root)
    value = json.loads(original)
    value['mods'][0]['downloads']['all']['size'] += 1
    (root / 'index.json').write_text(json.dumps(value))
    rejects(lambda: validate(root))
    value = json.loads(original)
    value['mods'][0]['version'] = '999.0.0'
    (root / 'index.json').write_text(json.dumps(value))
    rejects(lambda: validate(root))
    value = json.loads(original)
    value['mods'][0]['downloads']['all']['url'] = 'http://example.invalid/package.zip'
    (root / 'index.json').write_text(json.dumps(value))
    rejects(lambda: validate(root))
    (root / 'index.json').write_text(original)
    (root / 'unexpected.zip').write_bytes(b'authored invalid extra package')
    rejects(lambda: validate(root))
print('Catalogue checksum, manifest version, insecure URL and extra-artifact checks passed')
