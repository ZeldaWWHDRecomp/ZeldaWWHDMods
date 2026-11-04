"""Paths shared by the relocated synthetic tests; no SDK code is vendored."""
from pathlib import Path
import sys
PROJECT = Path(__file__).resolve().parent.parent
SOURCE = PROJECT / 'src'
TOOLS = PROJECT / 'tools'
HELPERS = PROJECT / 'tests' / 'helpers'
sys.path.insert(0, str(TOOLS))
