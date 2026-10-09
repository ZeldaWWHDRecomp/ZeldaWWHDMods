import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile
import json
from types import SimpleNamespace
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('combined', Path(__file__).with_name('run_combined_runtime_e2e.py'))
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)

class CombinedEvidence(unittest.TestCase):
    def test_single_loaded_mod_or_conflict_cannot_pass(self):
        log = '[guestmods] loaded /cache/dragon.dylib: 22 hooks\n'
        caps = dict.fromkeys(harness.CAPTURES, True)
        self.assertFalse(harness.runtime_evidence(log, True, caps)['runtime_smoke_passed'])
        log += '[guestmods] loaded /cache/gc-minimap.dylib: 1 hooks\n'
        self.assertTrue(harness.runtime_evidence(log, True, caps)['runtime_smoke_passed'])
        self.assertFalse(harness.runtime_evidence(log + 'Hook conflict: replace\n', True, caps)['runtime_smoke_passed'])
        self.assertFalse(harness.runtime_evidence(log, False, caps)['runtime_smoke_passed'])
        caps['after_close'] = False
        self.assertFalse(harness.runtime_evidence(log, True, caps)['runtime_smoke_passed'])
        self.assertTrue(harness.runtime_evidence(log, True, caps)['visual_review_required'])

    def test_default_overlap_and_relocated_panels(self):
        minimap = [6,477,231,702]
        self.assertTrue(harness.rectangles_overlap(minimap, [24,476,584,696]))
        self.assertFalse(harness.rectangles_overlap(minimap, [360,476,920,696]))
        self.assertFalse(harness.rectangles_overlap(minimap, [231,477,400,702]))

    def test_cache_reuse_refuses_wrong_key_without_compiling(self):
        builder = SimpleNamespace(compiler_version=lambda cc:'reviewed compiler',
            package_elf=lambda package: ({'id':package.name}, b'elf'),
            cache_key=lambda *args:'required-key', module_ext=lambda:'.so')
        builds = SimpleNamespace(identify=lambda path:object())
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'cache/wrong-key').mkdir(parents=True)
            (root/'cache/wrong-key/dragon.so').write_bytes(b'authored fake cache')
            with patch.dict('sys.modules', {'build_guest_mod':builder, 'builds':builds}):
                with self.assertRaisesRegex(ValueError, 'Missing exact verified cache'):
                    harness.seed_cache(root/'sdk', root/'manager', root/'cache', root/'game')
            self.assertFalse((root/'manager/GuestBuild').exists())

    def test_wrong_identity_and_archive_escape_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for entries in ({'../escaped': b'x'}, {'manifest.json': json.dumps({'id':'other','kind':'guest'})}):
                archive = root/'mod.zip'
                with zipfile.ZipFile(archive, 'w') as out:
                    for name, data in entries.items():out.writestr(name, data)
                with self.assertRaises(ValueError):harness.unpack(archive, root/'out', 'dragon')
            self.assertFalse((root/'escaped').exists())

if __name__ == '__main__':unittest.main()
