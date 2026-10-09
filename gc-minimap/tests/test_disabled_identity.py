"""Offline negative checks for the optional capture comparator; no game or Pillow dependency."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('disabled_identity', Path(__file__).with_name('run_disabled_identity.py'))
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)


class IdentityComparison(unittest.TestCase):
    def test_alpha_difference_cannot_pass_rgb_identity(self):
        left = bytes((10, 20, 30, 0, 40, 50, 60, 255))
        right = bytes((10, 20, 30, 255, 40, 50, 60, 255))
        result = harness.compare_rgba((2, 1), left, (2, 1), right)
        self.assertFalse(result['pixels_identical'])
        self.assertEqual(result['different_pixels'], 1)
        self.assertEqual(result['different_channels'], 1)
        self.assertEqual(result['maximum_channel_difference'], 255)

    def test_identical_and_dimension_mismatch(self):
        pixels = bytes(range(8))
        self.assertTrue(harness.compare_rgba((2, 1), pixels, (2, 1), pixels)['pixels_identical'])
        mismatch = harness.compare_rgba((2, 1), pixels, (1, 2), pixels)
        self.assertFalse(mismatch['pixels_identical'])
        self.assertIsNone(mismatch['different_pixels'])
        with self.assertRaises(ValueError):
            harness.compare_rgba((3, 1), pixels, (2, 1), pixels)

    def test_same_starting_configuration_only_private_paths_differ(self):
        a = harness.case_environment(Path('/private/disabled'), 'metal', '30', 3000, [3000, 3002, 3004])
        b = harness.case_environment(Path('/private/absent'), 'metal', '30', 3000, [3000, 3002, 3004])
        a = {key: value.replace('/private/disabled', '$CASE') for key, value in a.items()}
        b = {key: value.replace('/private/absent', '$CASE') for key, value in b.items()}
        self.assertEqual(a, b)
        self.assertNotIn('WWHD_TEST_TRUST_NATIVE_MODS', a)
        self.assertEqual(a['WWHD_CODE_MODS'], '1')

    def test_empty_flat_or_nonmatching_samples_never_pass(self):
        equal = {'pixels_identical': True, 'png_files_identical': True}
        self.assertEqual(harness.capture_status([], True), 'inconclusive')
        self.assertEqual(harness.capture_status([equal], False), 'inconclusive')
        self.assertEqual(harness.capture_status([dict(equal, pixels_identical=False)], True), 'inconclusive')
        self.assertEqual(harness.capture_status([dict(equal, png_files_identical=False)], True), 'inconclusive')
        self.assertEqual(harness.capture_status([equal], True), 'captured_frames_identical')

    def test_save_snapshot_refuses_empty_and_symlink_fixtures(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            with self.assertRaises(ValueError): harness.save_snapshot(root)
            (root / 'authored-save.txt').write_text('authored fixture only')
            expected = harness.save_snapshot(root)
            (root / 'authored-save.txt').write_text('changed authored fixture')
            self.assertNotEqual(harness.save_snapshot(root), expected)
            try:
                (root / 'alias').symlink_to(root / 'authored-save.txt')
            except OSError:
                return
            with self.assertRaises(ValueError): harness.save_snapshot(root)


if __name__ == '__main__':
    unittest.main()
