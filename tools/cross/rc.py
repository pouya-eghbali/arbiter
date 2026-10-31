#!/usr/bin/env python3
"""Chromium RC bridge using native Clang preprocessing and LLVM's RC parser."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main(argv):
    llvm = Path(os.environ["ARBITER_WINDOWS_LLVM"])
    sysroot = Path(os.environ["ARBITER_WINDOWS_SYSROOT"])
    flags, includes, output = [], [], None
    source = Path(argv[-1])
    for arg in argv[:-1]:
        if arg.lower().startswith("/fo"):
            output = Path(arg[3:])
        elif arg.startswith("-I"):
            includes.append(arg[2:])
            flags.append(arg)
        elif arg.startswith("-D") or arg == "/WX":
            flags.append(arg)
        elif arg.lower() != "/nologo":
            raise RuntimeError(f"Unsupported resource flag: {arg}")
    if output is None:
        raise RuntimeError("Missing /fo output")
    output.parent.mkdir(parents=True, exist_ok=True)
    system_includes = [str(sysroot / suffix) for suffix in (
        "crt/include", "sdk/include/ucrt", "sdk/include/shared", "sdk/include/um", "sdk/include/winrt")]
    with tempfile.TemporaryDirectory(prefix="arbiter-rc-") as folder:
        preprocessed = Path(folder) / "input.rc"
        command = [str(llvm / "bin/clang-cl"), "--target=x86_64-pc-windows-msvc",
                   "/clang:-fms-compatibility-version=19.44", "/P", "/TC", "/DRC_INVOKED",
                   "/showIncludes", "/Fi" + str(preprocessed), *flags]
        command += ["/imsvc" + name for name in system_includes]
        command += ["--", str(source)]
        subprocess.run(command, check=True)
        search = [str(source.parent), *includes, *system_includes]
        # LLVM's parser consumes the already preprocessed file. Search paths
        # are still needed for referenced icon/bitmap/manifest resources.
        command = [str(llvm / "bin/llvm-rc"), "/no-preprocess", "/C", "65001",
                   "/FO", str(output)]
        for name in search:
            command += ["/I", name]
        subprocess.run([*command, str(preprocessed)], check=True)
        for name in re.findall(r'"([^"\n]+\.(?:ico|cur|bmp|png|manifest))"', preprocessed.read_text(), re.I):
            for directory in [".", *search]:
                candidate = Path(directory) / name.replace("\\\\", "/").replace("\\", "/")
                if candidate.is_file():
                    print(f"Note: including file: {candidate.resolve()}")
                    break
    print(f"Note: including file: {Path(__file__).resolve()}")


if __name__ == "__main__":
    main(sys.argv[1:])
