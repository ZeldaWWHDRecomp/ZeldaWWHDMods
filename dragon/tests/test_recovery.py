"""Compile and execute the actual guest lifecycle recovery decisions against synthetic values."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class DragonRecovery(unittest.TestCase):
    def test_wait_cancellation_late_requests_and_rope_failures(self):
        compiler = shutil.which('clang') or shutil.which('cc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        with tempfile.TemporaryDirectory() as temporary:
            executable = Path(temporary) / 'dragon_recovery'
            subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                            '-I', str(Path(__file__).parent / 'include'), str(Path(__file__).with_suffix('.c')), '-o', str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    unittest.main()
