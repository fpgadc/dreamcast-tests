#!/usr/bin/env python3
"""ta-probe compares as ta-tests does (regress.sh uses this): ../ta-tests/compare_dumps.py --test ta-probe."""
import os, sys
here = os.path.dirname(os.path.abspath(__file__))
args = [a for a in sys.argv[1:] if a != "ta-probe"]
os.execvp(sys.executable, [sys.executable, os.path.join(here, "..", "ta-tests", "compare_dumps.py"), "--test", "ta-probe"] + args)
