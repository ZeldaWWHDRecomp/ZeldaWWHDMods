"""Synthetic inputs only: setup writes cannot escape their mod-data directory."""
import importlib.util
import os
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('safe_io', Path(__file__).parents[1] / 'tools/safe_io.py')
safe_io = importlib.util.module_from_spec(spec)
spec.loader.exec_module(safe_io)


class SafeIOTests(unittest.TestCase):
    def test_bounded_read_and_flat_write(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'disc.bin'
            source.write_bytes(b'authored synthetic fixture')
            context = safe_io.Context(source, root / 'data')
            self.assertEqual(context.read_game(9, 9), b'synthetic')
            context.write_data('room-01.png', b'authored output')
            self.assertEqual((root / 'data/room-01.png').read_bytes(), b'authored output')
            self.assertEqual(source.read_bytes(), b'authored synthetic fixture')
            for offset, size in ((-1, 1), (0, safe_io.MAX_READ + 1), (100, 1)):
                with self.assertRaises(ValueError):
                    context.read_game(offset, size)
            for name in ('../outside', '/outside', '.hidden', 'dir/file', 'C:outside'):
                with self.assertRaises(ValueError):
                    context.write_data(name, b'no')
            with self.assertRaises(ValueError):
                context.write_data('large.bin', b'x' * (safe_io.MAX_OUTPUT + 1))

    def test_source_and_output_symlinks_are_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'source'
            source.mkdir()
            (source / 'map').write_bytes(b'fixture')
            context = safe_io.Context(source, root / 'data')
            self.assertEqual(context.read_input('map'), b'fixture')
            (source / 'alias').symlink_to(source / 'map')
            with self.assertRaises(ValueError):
                context.read_input('alias')
            (root / 'data/escape').symlink_to(source / 'map')
            with self.assertRaises(ValueError):
                context.write_data('escape', b'no')
            self.assertEqual((source / 'map').read_bytes(), b'fixture')
            os.link(source / 'map', root / 'data/hardlink')
            context.write_data('hardlink', b'replacement')
            self.assertEqual((source / 'map').read_bytes(), b'fixture')
            with self.assertRaises(ValueError):
                context.read_input('../source/map')

    def test_output_refuses_game_and_git_trees(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / 'source'
            source.mkdir()
            with self.assertRaises(ValueError):
                safe_io.Context(source, source / 'output')
            checkout = root / 'checkout'
            checkout.mkdir()
            (checkout / '.git').mkdir()
            with self.assertRaises(ValueError):
                safe_io.Context(source, checkout / 'output')


if __name__ == '__main__':
    unittest.main()
