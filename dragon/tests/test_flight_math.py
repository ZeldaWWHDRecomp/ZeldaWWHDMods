"""Compile and execute the actual guest quest and song rules against synthetic values."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class DragonFlightMath(unittest.TestCase):
    def test_trigonometry_and_length_error_bounds(self):
        compiler = shutil.which('clang') or shutil.which('cc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        with tempfile.TemporaryDirectory() as temporary:
            executable = Path(temporary) / 'dragon_math'
            subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                            str(Path(__file__).with_suffix('.c')), '-lm', '-o', str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    unittest.main()
