"""Fail CI early on missing test DLLs instead of waiting for loader dialogs."""
import os
from pathlib import Path
import re
import subprocess
import sys


def missing_exports(binary, directories):
    imports = subprocess.check_output(["dumpbin", "/NOLOGO", "/IMPORTS", str(binary)], text=True)
    exports = {}
    dependency = None
    for line in imports.splitlines():
        module = re.fullmatch(r"\s+([\w.-]+\.dll)\s*", line, re.IGNORECASE)
        if module:
            name = module[1]
            dependency = next((d / name for d in directories if (d / name).is_file()), None)
            continue
        symbol = re.fullmatch(r"\s+(?:[0-9A-Fa-f]+\s+){1,3}([?A-Za-z_][^\s]*)\s*", line)
        if not dependency or not symbol or symbol[1].startswith("Ordinal"):
            continue
        if dependency not in exports:
            table = subprocess.check_output(["dumpbin", "/NOLOGO", "/EXPORTS", str(dependency)], text=True)
            exports[dependency] = set(re.findall(
                r"^\s+\d+\s+[0-9A-Fa-f]+\s+(?:[0-9A-Fa-f]{8,16}\s+)?([^\s]+)", table, re.MULTILINE))
        if symbol[1] not in exports[dependency]:
            print(f"::error::{binary.name} imports {symbol[1]} missing from {dependency}", flush=True)


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
            missing_exports(qt_bin / name, directories)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
