"""Compile and execute the actual guest quest and song rules against synthetic values."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class DragonDialogueInput(unittest.TestCase):
    def test_close_hold_release_and_deliberate_new_press(self):
        compiler = shutil.which('clang') or shutil.which('cc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        with tempfile.TemporaryDirectory() as temporary:
            executable = Path(temporary) / 'dragon_dialogue_input'
            subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                            str(Path(__file__).with_suffix('.c')), '-o', str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    unittest.main()
