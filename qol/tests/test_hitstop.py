"""Exercise the actual hitstop observer with synthetic native fields, without a game."""
from pathlib import Path
import os
import shlex
import shutil
import subprocess
import tempfile
import unittest

class Hitstop(unittest.TestCase):
    def test_observer(self):
        compiler = shlex.split(os.environ.get('CC') or shutil.which('clang') or 'cc')
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / ('hitstop.exe' if os.name == 'nt' else 'hitstop')
            subprocess.run([*compiler, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                            str(Path(__file__).with_suffix('.c')), '-o', str(output)], check=True)
            subprocess.run([str(output)], check=True)

if __name__ == '__main__':
    unittest.main()
