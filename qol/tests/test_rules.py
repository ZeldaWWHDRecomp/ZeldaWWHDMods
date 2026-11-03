"""Compile and run the QoL package's pure rules (src/rules.h) on the host."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class QolRules(unittest.TestCase):
    def test_rules(self):
        compiler = shutil.which('clang') or shutil.which('cc')
        if not compiler:
            self.skipTest('C compiler unavailable')
        with tempfile.TemporaryDirectory() as temporary:
            executable = Path(temporary) / 'qol_rules'
            subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                            str(Path(__file__).with_suffix('.c')), '-lm', '-o', str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    unittest.main()
