"""Build static OpenSSL COFF libraries with Clang's Windows MSVC ABI.

OpenSSL's mingw64 configuration supplies the Unix make/Windows platform rules.
No MinGW compiler, headers, import libraries, or runtime are used. Assembly and
loadable modules are disabled. The application and library both use /MD.
"""
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import tarfile
import urllib.request


def build(args, run, atomic_json):
    root = Path(__file__).resolve().parents[2]
    pin = json.loads((root / "tools/release/openssl.json").read_text())
    work = args.work / "openssl"
    work.mkdir(parents=True, exist_ok=True)
    sdk = work / "sdk"
    stamp = sdk / "arbiter-openssl-build.json"
    stamp.unlink(missing_ok=True)
    archive = work / f"openssl-{pin['version']}.tar.gz"
    if not archive.exists():
        with urllib.request.urlopen(pin["url"], timeout=60) as source, archive.open("wb") as target:
            shutil.copyfileobj(source, target)
    with archive.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    if digest != pin["sha256"]:
        archive.unlink()
        raise RuntimeError("OpenSSL archive checksum mismatch")
    source = work / "source" / f"openssl-{pin['version']}"
    if not source.exists():
        with tarfile.open(archive) as bundle:
            bundle.extractall(work / "source", filter="data")
    output = work / "build"
    output.mkdir(exist_ok=True)
    env = os.environ.copy()
    flags = ["--target=x86_64-pc-windows-msvc", "-fms-compatibility-version=19.44",
             "-fms-runtime-lib=dll", "-Wno-ignored-attributes", "-Wno-deprecated-declarations"]
    for suffix in ("crt/include", "sdk/include/ucrt", "sdk/include/shared", "sdk/include/um", "sdk/include/winrt"):
        flags += ["-isystem", str(args.work / "sysroot" / suffix)]
    env.update(CC=shlex.join([str(args.llvm / "bin/clang"), *flags]),
               AR=str(args.llvm / "bin/llvm-ar"), RANLIB=str(args.llvm / "bin/llvm-ranlib"))
    run("perl", source / "Configure", "mingw64", "no-asm", "no-shared", "no-apps", "no-tests",
        "no-module", "no-legacy", f"--prefix={sdk}", "--libdir=lib", cwd=output, env=env)
    run("make", f"-j{args.jobs}", "build_libs", cwd=output, env=env)
    run("make", "install_dev", cwd=output, env=env)
    for name in ("crypto", "ssl"):
        archive_path = sdk / f"lib/lib{name}.a"
        info = run(args.llvm / "bin/llvm-readobj", "--file-headers", archive_path, capture=True)
        if "COFF-x86-64" not in info or "Mach-O" in info or "ELF" in info:
            raise RuntimeError("OpenSSL produced a library for the wrong target")
        symbols = run(args.llvm / "bin/llvm-nm", "--undefined-only", archive_path, capture=True)
        if "mingw" in symbols.lower():
            raise RuntimeError("Unexpected MinGW runtime reference in the MSVC-ABI library")
        shutil.copy2(archive_path, sdk / f"lib/lib{name}.lib")
    shutil.copyfile(source / "LICENSE.txt", sdk / "OpenSSL-LICENSE.txt")
    atomic_json(stamp, dict(pin, platform="win32", architecture="x86_64", linkage="static",
                           runtime="MD", compiler=str(args.llvm),
                           builder_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()))
    return sdk
