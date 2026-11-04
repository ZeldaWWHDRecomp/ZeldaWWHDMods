"""Compile and run the QoL package's actual block movement and animation hooks on the host."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class BlockPush(unittest.TestCase):
    def test_block_push(self):
        compiler = shutil.which('clang') or shutil.which('cc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        with tempfile.TemporaryDirectory() as temporary:
            executable = Path(temporary) / 'block_push'
            subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-pointer-to-int-cast',
                            str(Path(__file__).with_suffix('.c')), '-lm', '-o', str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    unittest.main()
