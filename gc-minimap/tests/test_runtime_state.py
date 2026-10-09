"""Verify restore evidence is ordered and includes the actual HUD overlay."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from PIL import Image, ImageDraw

spec = importlib.util.spec_from_file_location('runtime_e2e', Path(__file__).with_name('run_runtime_e2e.py'))
harness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(harness)


class StateEvidenceTest(unittest.TestCase):
    def fixture(self, out):
        (out / 'states').mkdir()
        (out / 'states/slot1.bin').write_bytes(b'private-test-state')
        Image.new('RGB', (1280, 720)).save(out / 'state_load1_3.png')
        for frame in (3000, 3002, 3004):
            image = Image.new('RGB', (1280, 720))
            draw = ImageDraw.Draw(image)
            draw.rectangle((28, 499, 207, 678), fill=(31, 201, 109))
            draw.rectangle((110, 600, 119, 609), fill=(249, 118, 63))
            image.save(out / ('frame_%d_present.png' % frame))
        log = '[savestate] slot 1: written (100 MB on disk)\n[savestate] slot 1: restored in 5 ms\n[gfx] wrote state_load1_3.png\n'
        log += ''.join('[gfx] wrote frame_%d_present.png\n' % frame for frame in (3000, 3002, 3004))
        (out / 'runtime.log').write_text(log)
        return log

    def test_actual_restore_and_overlay_required(self):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            log = self.fixture(out)
            self.assertTrue(harness.state_cycle_result(out, True, 3000)['passed'])
            real = harness.state_cycle_result(out, False, 3000)
            self.assertTrue(real['passed'])
            self.assertTrue(real['real_map_visual_review_required'])
            self.assertIsNone(real['synthetic_hud_recovery_pass'])
            self.assertNotIn('green_pixels', real['post_restore_present_hud'][0])
            (out / 'runtime.log').write_text(log.replace('[gfx] wrote frame_', '[display] present dump frame_'))
            self.assertTrue(harness.state_cycle_result(out, True, 3000)['passed'])
            # Captures made before restoration cannot prove recovery.
            # Explicitly move the only frame-3000 write before restore.
            reordered = '[gfx] wrote frame_3000_present.png\n' + log.replace('[gfx] wrote frame_3000_present.png\n', '')
            (out / 'runtime.log').write_text(reordered)
            self.assertFalse(harness.state_cycle_result(out, True, 3000)['passed'])
            (out / 'runtime.log').write_text(log)
            Image.new('RGB', (1280, 720), (31, 201, 109)).save(out / 'frame_3002_present.png')
            self.assertFalse(harness.state_cycle_result(out, True, 3000)['passed'])

    def test_restore_log_and_native_state_required(self):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            log = self.fixture(out)
            (out / 'runtime.log').write_text(log.replace('restored in', 'cannot restore'))
            self.assertFalse(harness.state_cycle_result(out, True, 3000)['passed'])
            (out / 'runtime.log').write_text(log)
            (out / 'states/slot1.bin').unlink()
            self.assertFalse(harness.state_cycle_result(out, True, 3000)['passed'])


if __name__ == '__main__':
    unittest.main()
