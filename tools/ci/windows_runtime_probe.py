"""Fail CI early on missing test DLLs instead of waiting for loader dialogs."""
import os
from pathlib import Path
import re
import subprocess
import sys


def main():
    qt_bin = Path(os.environ["QT_ROOT_DIR"]) / "bin"
    directories = [Path("build-tests").resolve(), qt_bin,
                   Path(os.environ["SystemRoot"]) / "System32"]
    directories.extend(Path(p) for p in os.environ["PATH"].split(os.pathsep) if p)
    queue = [Path("build-tests/siteaudio_test.exe").resolve()]
    visited = set()
    missing = set()
    while queue:
        binary = queue.pop()
        if binary in visited:
            continue
        visited.add(binary)
        result = subprocess.run(["dumpbin", "/NOLOGO", "/DEPENDENTS", str(binary)],
                                capture_output=True, text=True, check=True)
        imports = re.findall(r"^\s+([\w.-]+\.dll)\s*$", result.stdout, re.MULTILINE | re.IGNORECASE)
        for name in imports:
            if name.lower().startswith(("api-ms-", "ext-ms-")):
                continue
            found = next((d / name for d in [binary.parent, *directories] if (d / name).is_file()), None)
            if found is None:
                missing.add(f"{binary.name} needs {name}")
            elif found.parent == qt_bin:
                queue.append(found)
    if missing:
        for message in sorted(missing):
            print(f"::error::{message}", flush=True)
        return 1
    print(f"Windows DLL imports resolved for {len(visited)} application/Qt binaries.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
