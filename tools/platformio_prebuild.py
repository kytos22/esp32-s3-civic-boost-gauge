Import("env")

import subprocess
import sys
from pathlib import Path


root = Path(env.subst("$PROJECT_DIR"))
source = root / "tools" / "prebaked_gauge_cache.bin"
generator = root / "tools" / "generate_prebaked_cache.py"
outputs = [
    root / "src" / "prebaked_gauge_cache.h",
    root / "src" / "prebaked_gauge_cache.cpp",
]

if not source.exists():
    raise RuntimeError(f"Missing prebaked cache source: {source}")

newest_input = max(source.stat().st_mtime, generator.stat().st_mtime)
if any(not output.exists() or output.stat().st_mtime < newest_input for output in outputs):
    subprocess.check_call([sys.executable, str(generator)], cwd=root)

