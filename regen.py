"""Write a test's cases.h and cases.json from an archived set (fit/<test>_data/<set>.cases.json).

    python3 regen.py <test> <set> [folder, default: the test's own]

run2.sh <test>@<set> calls this with the run's folder. A test with a gen.cpp does it with its generator
(<test>/gen --regen, built by model/build.sh); the others have a gen.py with write(folder, data)."""
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).parent
test, name = sys.argv[1], sys.argv[2]
folder = Path(sys.argv[3]) if len(sys.argv) > 3 else HERE / test
archive = HERE / f"fit/{test}_data/{name}.cases.json"
if (HERE / test / "gen.cpp").exists():
    subprocess.run(["bash", str(HERE / "model/build.sh"), test], check=True, stdout=subprocess.DEVNULL)
    subprocess.run([str(HERE / test / "gen"), "--regen", str(archive), str(folder)], check=True)
else:
    sys.path.insert(0, str(HERE / test))
    spec = importlib.util.spec_from_file_location(f"{test}_gen", HERE / test / "gen.py")
    gen = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gen)
    gen.write(folder, json.loads(archive.read_text()))
print(f"{test} {name}: tables written from the archive")
