from pathlib import Path
import sys


TESTS_DIR = Path(__file__).resolve().parent
MODULE_DIR = TESTS_DIR.parent

if str(MODULE_DIR) not in sys.path:
    sys.path.insert(0, str(MODULE_DIR))
