"""Compile and execute the actual guest projection helpers against synthetic values."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class MinimapMath(unittest.TestCase):
    def test_sector_boundaries_projection_and_binary_bounds(self):
        compiler = shutil.which('clang') or shutil.which('cc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        with tempfile.TemporaryDirectory() as temporary:
            executable = Path(temporary) / 'minimap_math'
            subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                            str(Path(__file__).with_suffix('.c')), '-o', str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    unittest.main()
