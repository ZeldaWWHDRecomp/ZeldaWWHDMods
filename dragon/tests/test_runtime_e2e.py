"""Harness checks only; these never start a game or count as gameplay proof."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

from run_runtime_e2e import (capture_frames, continue_progress, inspect_outputs, plan, safe_copy_save,
                             sha256, unpack_package, validate_route)


class RuntimeHarness(unittest.TestCase):
    def route(self):
        route = copy.deepcopy(plan()['route_template'])
        route['ready'] = True
        return route

    def test_unrecorded_route_refused(self):
        with self.assertRaisesRegex(ValueError, 'unready'):
            validate_route(plan()['route_template'])

    def test_normal_route_rejects_injected_progress(self):
        route = self.route()
        route['fixture'] = {'progress': {'0': [4, 7]}}
        with self.assertRaisesRegex(ValueError, 'classified as normal'):
            validate_route(route)
        route['classification'] = 'instrumented'
        with self.assertRaisesRegex(ValueError, 'Normal quest completion'):
            validate_route(route)

    def test_state_load_requires_same_run_save(self):
        route = self.route(); route['case'] = 'full-state'
        route['full_load'] = [[4000, 1]]
        with self.assertRaisesRegex(ValueError, 'earlier save'):
            validate_route(route)
        route['full_save'] = [[3900, 1]]
        validate_route(route)

    def test_finite_input_and_review_required(self):
        route = self.route(); route['stick'] = [[1, 2, float('nan'), 0]]
        with self.assertRaisesRegex(ValueError, 'finite'):
            validate_route(route)
        route = self.route(); route['review_points'] = []
        with self.assertRaisesRegex(ValueError, 'review points'):
            validate_route(route)

    def test_unsafe_package_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary); archive = folder / 'package.zip'
            with zipfile.ZipFile(archive, 'w') as output:
                output.writestr('../escaped', 'no')
            with self.assertRaisesRegex(ValueError, 'Unsafe'):
                unpack_package(archive, folder / 'mod')
            self.assertFalse((folder / 'escaped').exists())

    def test_normal_save_refuses_external_state(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary); source = folder / 'save'; source.mkdir()
            (source / 'slot1.bin').write_bytes(b'not a current normal save')
            with self.assertRaisesRegex(ValueError, 'old full/portable'):
                safe_copy_save(source, folder / 'copy')

    def test_module_load_log_does_not_fake_full_restore(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            (folder / 'runtime.log').write_text('[guestmods] loaded dragon\n[savestate] enabled\n')
            (folder / 'test_done').touch()
            route = self.route(); route['expect'] = {'log_contains': ['loaded dragon']}
            route['full_load'] = [[4000, 1]]
            checks, _, _ = inspect_outputs(folder, route, '30')
            self.assertFalse(checks['full_restore:1'])
            (folder / 'runtime.log').write_text('[guestmods] loaded dragon\n[savestate] slot 1: restored in 30 ms\n')
            checks, _, _ = inspect_outputs(folder, route, '30')
            self.assertTrue(checks['full_restore:1'])

    def test_interpolation_requires_actual_chosen_rate_and_hold_passes(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            route = self.route()
            for log, expected in [
                ('[interp] display refresh rate 120 Hz: frame interpolation draws up to 120 fps', False),
                ('[interp] frame interpolation on (120 fps)\n[interp] 30 logic steps/s (120 fps, 4.00 frames per step)', False),
                ('[interp] frame interpolation on (60 fps)\n[interp] 30 logic steps/s (60 fps, 2.00 frames per step)', True),
            ]:
                (folder / 'runtime.log').write_text(log)
                checks, _, _ = inspect_outputs(folder, route, 'interp60')
                self.assertEqual(checks['interpolation_enabled'] and checks['interpolation_two_frames'], expected)

    def test_interpolation_capture_clock_counts_hold_swaps(self):
        route = self.route()
        route['origin_frame'] = 3300
        route['capture_frames'] = [3100, 3110, 3300, 5700, 6120]
        self.assertEqual(capture_frames(route, 'interp60'), [3100, 3110, 3490, 8290, 9130])
        self.assertEqual(capture_frames(route, '30'), route['capture_frames'])
        self.assertEqual(capture_frames(route, 'true60'), route['capture_frames'])

    def test_continuation_requires_reviewed_current_normal_flow(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary); source = folder / 'prior'; source.mkdir()
            data = source / 'manager/Data/dragon'; data.mkdir(parents=True)
            target = folder / 'next'; target.mkdir()
            (data / 'dragon-quest-slot0.txt').write_text('WWHD_DRAGON_QUEST 1\n4 7\n')
            identity = {'binary_sha256': 'binary', 'package_sha256': 'package', 'sdk_commit': 'current', 'region': 'EU'}
            report = {**identity, 'classification': 'instrumented', 'automatic_pass': True}
            result = source / 'result.json'; result.write_text(json.dumps(report))
            receipt = source / 'gameplay-review.json'
            receipt.write_text(json.dumps({'result_sha256': sha256(result), 'all_review_points_passed': True}))
            with self.assertRaisesRegex(ValueError, 'normal-flow'):
                continue_progress(source, target, identity)
            report['classification'] = 'normal'; result.write_text(json.dumps(report))
            with self.assertRaisesRegex(ValueError, 'normal-flow'):
                continue_progress(source, target, identity)  # Stale review hash.
            receipt.write_text(json.dumps({'result_sha256': sha256(result), 'all_review_points_passed': True}))
            with self.assertRaisesRegex(ValueError, 'different current'):
                continue_progress(source, target, {**identity, 'sdk_commit': 'old'})
            observed = continue_progress(source, target, identity)
            self.assertEqual(observed['progress_sha256']['dragon-quest-slot0.txt'], sha256(target / 'dragon-quest-slot0.txt'))


if __name__ == '__main__':
    unittest.main()
