"""Fail CI early on missing test DLLs instead of waiting for loader dialogs."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys


def repair_qt612_accessibility(qt_bin):
    # CI's stock Windows WebEngine 6.140 still imports the public constructor.
    # Qt 6.12 made it protected without changing its signature, layout or body:
    # https://github.com/qt/qtbase/commit/34523a5d7a90ca2c4a117766381b88986d01fd28
    # MSVC encodes this access change in the imported name. Adjust that one
    # name in the disposable CI kit; release SDKs are built from matching headers.
    manifest = json.loads(Path("engine/manifest.json").read_text())
    if os.environ.get("GITHUB_ACTIONS") != "true":
        raise RuntimeError("The stock-Qt compatibility repair is restricted to GitHub CI.")
    if (manifest["qt"], manifest["webengine_version"]) != ("6.12.0", "6.140.0"):
        raise RuntimeError("Re-review the stock-Qt compatibility repair for the new dependency versions.")
    old = b"??0QAccessibleInterface@@QEAA@XZ\0"
    new = b"??0QAccessibleInterface@@IEAA@XZ\0"
    core = qt_bin / "Qt6WebEngineCore.dll"
    data = core.read_bytes()
    if old not in data:
        print("Stock WebEngine does not require the Qt 6.12 accessibility repair.", flush=True)
        return
    table = subprocess.check_output(["dumpbin", "/NOLOGO", "/EXPORTS", str(qt_bin / "Qt6Gui.dll")], text=True)
    exports = set(re.findall(
        r"^\s+\d+\s+[0-9A-Fa-f]+\s+(?:[0-9A-Fa-f]{8,16}\s+)?([^\s]+)", table, re.MULTILINE))
    if old[:-1].decode() in exports:
        return
    if new[:-1].decode() not in exports or data.count(old) != 1:
        raise RuntimeError("Unexpected accessibility constructor exports/imports; refusing to alter Qt.")
    core.write_bytes(data.replace(old, new, 1))
    print("Applied the CI-only Qt 6.12 accessibility constructor import repair.", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repair-qt612-accessibility", action="store_true")
    args = parser.parse_args()
    qt_bin = Path(os.environ["QT_ROOT_DIR"]) / "bin"
    if args.repair_qt612_accessibility:
        repair_qt612_accessibility(qt_bin)
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
    loader = (
        "import ctypes,os,sys; "
        "scope=os.add_dll_directory(sys.argv[1]); "
        "ctypes.windll.kernel32.SetErrorMode(0x8003); "
        "ctypes.WinDLL(os.path.join(sys.argv[1],sys.argv[2])); "
        "print('Loaded '+sys.argv[2],flush=True)"
    )
    for name in ("Qt6Core.dll", "Qt6Quick.dll", "Qt6WebEngineCore.dll", "Qt6WebEngineQuick.dll"):
        print(f"Checking Windows loader initialization: {name}", flush=True)
        try:
            result = subprocess.run([sys.executable, "-c", loader, str(qt_bin), name],
                                    capture_output=True, text=True, timeout=20)
        except subprocess.TimeoutExpired:
            print(f"::error::Windows loader stalled initializing {name}", flush=True)
            return 1
        print(result.stdout, end="", flush=True)
        if result.returncode:
            print(result.stderr, end="", flush=True)
            print(f"::error::Windows could not initialize {name}: exit {result.returncode}", flush=True)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
