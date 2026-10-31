#!/usr/bin/env python3
"""Experimental Windows x64 MSVC-ABI cross-build from macOS or Linux.

Host tools always run natively. Target Qt/SDK binaries are never executed.
Each phase is explicit; probe success is not a successful browser release.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "engine"))
from identity import fingerprint, verify_sdk
from media_policy import verify as verify_media

PINS = json.loads((HERE / "versions.json").read_text())
ENGINE = json.loads((ROOT / "engine/manifest.json").read_text())
DEPS = json.loads((ROOT / "dependencies.json").read_text())


def run(*args, cwd=None, env=None, capture=False):
    print("+ " + shlex.join(str(a) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], cwd=cwd, env=env, check=True,
                          text=True, stdout=subprocess.PIPE if capture else None).stdout


def atomic_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(data, indent=2) + "\n")
    tmp.replace(path)


def pe_x64(path):
    """Reject wrong-host binaries without trying to execute a target program."""
    with path.open("rb") as stream:
        header = stream.read(64)
        if len(header) != 64 or header[:2] != b"MZ":
            raise RuntimeError(f"Not a Windows PE: {path}")
        stream.seek(struct.unpack_from("<I", header, 60)[0])
        if stream.read(6) != b"PE\0\0\x64\x86":
            raise RuntimeError(f"Expected a Windows x64 PE: {path}")


def native_tool(prefix, name):
    path = prefix / "bin" / name
    if not path.is_file() or not os.access(path, os.X_OK):
        raise RuntimeError(f"Missing native executable: {path}")
    return path


def toolchain_identity(args):
    clang = run(native_tool(args.llvm, "clang-cl"), "--version", capture=True)
    lld = run(native_tool(args.lld, "lld-link"), "--version", capture=True)
    versions = []
    for output in (clang, lld):
        match = re.search(r"(?:version\s+|LLD\s+)(\d+\.\d+\.\d+)", output)
        if not match:
            raise RuntimeError("Cannot determine LLVM/LLD versions")
        versions.append(match[1])
    if versions[0] != versions[1] or int(versions[0].split(".")[0]) != PINS["llvm_major"]:
        raise RuntimeError(f"Use matching LLVM/LLD {PINS['llvm_major']}.x: {versions}")
    for name in ("clang", "clang++", "llvm-lib", "llvm-rc", "llvm-mt"):
        native_tool(args.llvm, name)
    host_version = run(args.host_qt / "bin/qtpaths", "--query", "QT_VERSION", capture=True).strip()
    if host_version != ENGINE["qt"]:
        raise RuntimeError(f"Host Qt must match target Qt {ENGINE['qt']}, got {host_version}")
    digest = hashlib.sha256()
    for path in [HERE / "windows.py", HERE / "rc.py", HERE / "versions.json", HERE / "qtwebengine.patch",
                 ROOT / "cmake/toolchains/WindowsClang.cmake"]:
        digest.update(path.name.encode() + b"\0" + path.read_bytes())
    return {"engine_inputs_sha256": fingerprint(), "cross_inputs_sha256": digest.hexdigest(),
            "llvm": versions[0], "host": f"{sys.platform}-{platform.machine()}",
            "target": "x86_64-pc-windows-msvc", "qt": ENGINE["qt"], "pins": PINS}


def python_path(args):
    return args.work / "python/bin/python"


def target_qt(args):
    return args.work / "qt" / ENGINE["qt"] / "msvc2022_64"


def prepare(args):
    if not args.accept_microsoft_license:
        raise RuntimeError("Preparing the SDK requires --accept-microsoft-license. Read "
                           "https://go.microsoft.com/fwlink/?LinkId=2086102 first.")
    args.work.mkdir(parents=True, exist_ok=True)
    py = python_path(args)
    if not py.exists():
        run(sys.executable, "-m", "venv", py.parent.parent)
    run(py, "-m", "pip", "install",
        f"git+https://github.com/miurahr/aqtinstall.git@{PINS['aqt_commit']}", "html5lib==1.1")
    env = os.environ.copy()
    env["CARGO_HOME"] = str(args.work / "cargo")
    # rustup locates the pinned host cargo without depending on ~/.cargo/env.
    cargo = run("rustup", "which", "--toolchain", DEPS["rust"], "cargo", capture=True).strip()
    env["PATH"] = str(Path(cargo).parent) + os.pathsep + env["PATH"]
    xwin = args.work / "tools/bin/xwin"
    if not xwin.exists() or run(xwin, "--version", capture=True).strip() != f"xwin {PINS['xwin']}":
        run(cargo, "install", "xwin", "--version", PINS["xwin"], "--locked",
            "--root", args.work / "tools", "--jobs", args.jobs, env=env)
    run(xwin, "--accept-license", "--cache-dir", args.work / "xwin-cache",
        "--arch", "x86_64", "--include-atl", "--sdk-version", PINS["sdk"],
        "--crt-version", PINS["crt"], "splat", "--output", args.work / "sysroot")
    run(py, "-m", "aqt", "install-qt", "windows", "desktop", ENGINE["qt"],
        "win64_msvc2022_64", "--outputdir", args.work / "qt",
        "--modules", *PINS["qt_modules"], cwd=args.work)
    # Chromium expects the SDK's legacy Redist layout. Qt ships this exact
    # redistributable too; it is target data, never a host executable.
    d3d = target_qt(args) / "bin/d3dcompiler_47.dll"
    pe_x64(d3d)
    destination = args.work / "sysroot/sdk/Redist/D3D/x64/d3dcompiler_47.dll"
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(d3d, destination)
    atomic_json(args.work / "prepared.json", {"pins": PINS, "qt": ENGINE["qt"]})


def common_flags(args):
    qt = target_qt(args)
    version_file = qt / "lib/cmake/Qt6Core/Qt6CoreConfigVersionImpl.cmake"
    if f'set(PACKAGE_VERSION "{ENGINE["qt"]}")' not in version_file.read_text():
        raise RuntimeError("Windows Qt version does not match engine pins")
    pe_x64(qt / "bin/Qt6Core.dll")
    return ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
            f"-DCMAKE_TOOLCHAIN_FILE={ROOT / 'cmake/toolchains/WindowsClang.cmake'}",
            f"-DARBITER_WINDOWS_SYSROOT={args.work / 'sysroot'}",
            f"-DARBITER_WINDOWS_LLVM={args.llvm}", f"-DARBITER_WINDOWS_LLD={args.lld}",
            f"-DARBITER_WINDOWS_QT={qt}", f"-DCMAKE_PREFIX_PATH={qt}",
            f"-DQT_HOST_PATH={args.host_qt}"]


def configure(args, source, build, flags, env=None):
    # CMake's *_FLAGS_INIT are only read on the first configure. A changed
    # toolchain must refresh its cache, while unchanged builds retain objects.
    stamp = build / ".arbiter-configure.json"
    expected = {"identity": toolchain_identity(args), "source": str(source), "flags": flags}
    refresh = not stamp.exists() or json.loads(stamp.read_text()) != expected
    stamp.unlink(missing_ok=True)
    run("cmake", *(["--fresh"] if refresh else []), "-S", source, "-B", build, *flags, env=env)
    atomic_json(stamp, expected)


def probe(args):
    identity = toolchain_identity(args)
    build = args.work / "probe"
    receipt = args.work / "probe.json"
    receipt.unlink(missing_ok=True)
    flags = common_flags(args) + ["-DARBITER_PROBE_QT=ON"]
    openssl = args.openssl_prefix or args.work / "openssl/sdk"
    if (openssl / "include/openssl/crypto.h").exists():
        flags.append(f"-DARBITER_WINDOWS_OPENSSL={openssl.resolve()}")
    configure(args, HERE / "probe", build, flags)
    run("cmake", "--build", build, "--parallel", args.jobs)
    for name in ("windows-probe.exe", "qt-probe.exe"):
        pe_x64(build / name)
        imports = run(args.llvm / "bin/llvm-readobj", "--coff-imports", build / name, capture=True)
        expected = "Qt6Core.dll" if name == "qt-probe.exe" else "bcrypt.dll"
        if expected.lower() not in imports.lower():
            raise RuntimeError(f"Missing expected import {expected} in {name}")
    atomic_json(receipt, {"identity": identity, "status": "compiled-not-runtime-tested"})
    print(f"Windows x64/Qt probes passed. Outputs: {build}. Windows runtime testing is separate.")


def prepare_source(args):
    source = args.work / "source"
    # The native preparer verifies commits, native patches and overlays. It
    # only prepares source here; target-specific configure/build happens below.
    patch = HERE / "qtwebengine.patch"
    if source.exists():
        result = subprocess.run(["patch", "--dry-run", "--force", "--fuzz=0", "--batch", "-R", "-p1", "-i", str(patch)],
                                cwd=source, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if result.returncode == 0:
            run("patch", "--force", "--fuzz=0", "--batch", "-R", "-p1", "-i", patch, cwd=source)
    preserve_binary_pdf(source)
    command = (python_path(args), ROOT / "engine/build.py", "--prepare-only",
               "--qt-prefix", args.host_qt, "--work", args.work)
    try:
        run(*command)
    except subprocess.CalledProcessError:
        # A fresh checkout may have reached the Chromium validation before
        # this known upstream attributes problem could be corrected.
        if not preserve_binary_pdf(source):
            raise
        run(*command)
    if run("git", "rev-parse", "HEAD", cwd=source, capture=True).strip() != ENGINE["webengine_commit"]:
        raise RuntimeError("Cross source has the wrong Qt WebEngine revision")
    if run("git", "rev-parse", "HEAD", cwd=source / "src/3rdparty", capture=True).strip() != ENGINE["chromium_commit"]:
        raise RuntimeError("Cross source has the wrong Chromium revision")
    # The compatibility patch is relative to Arbiter's already patched source.
    # patch(1), unlike git apply, can span the Chromium submodule boundary.
    run("patch", "--dry-run", "--force", "--fuzz=0", "--batch", "--forward", "-p1", "-i", patch, cwd=source)
    run("patch", "--force", "--fuzz=0", "--batch", "--forward", "-p1", "-i", patch, cwd=source)
    shutil.copyfile(HERE / "rc.py", source / "src/3rdparty/chromium/build/toolchain/win/arbiter_rc.py")
    # GN renders files outside Chromium's root as absolute paths. clang-cl
    # treats a macOS /Users/... input as /U (undefine). Stage the verified Qt
    # core tree inside Chromium, preserving unchanged files for incremental builds.
    staged = source / "src/3rdparty/chromium/qtwebengine/arbiter_core"
    original = source / "src/core"
    for item in original.rglob("*"):
        if item.is_file() and not item.name.endswith((".orig", ".rej")):
            destination = staged / item.relative_to(original)
            if not destination.exists() or destination.read_bytes() != item.read_bytes():
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(item, destination)
    return source


def preserve_binary_pdf(source):
    # Upstream marks this binary PDF as text/eol=lf, although its committed
    # bytes contain CRLF. A stat refresh after a reflink copy exposes a false
    # diff. Only override text conversion after proving byte equality to HEAD.
    module = source / "src/3rdparty"
    name = "chromium/third_party/dawn/third_party/khronos/OpenGL-Registry/xml/readme.pdf"
    if not (module / name).exists():
        return False
    original = run("git", "rev-parse", f"HEAD:{name}", cwd=module, capture=True).strip()
    actual = run("git", "hash-object", "--no-filters", name, cwd=module, capture=True).strip()
    if original != actual:
        raise RuntimeError("Upstream PDF bytes changed; refusing to hide source modifications")
    gitdir = Path(run("git", "rev-parse", "--absolute-git-dir", cwd=module, capture=True).strip())
    if not gitdir.is_relative_to(source):
        raise RuntimeError("Source copy points to an external Git directory; use an isolated checkout")
    attributes = gitdir / "info/attributes"
    current = attributes.read_text() if attributes.exists() else ""
    rule = f"{name} -text -diff\n"
    if rule in current:
        return False
    attributes.parent.mkdir(parents=True, exist_ok=True)
    attributes.write_text(current + "\n" + rule)
    return True


def clang_facade(args):
    # Homebrew keeps LLVM and LLD in separate prefixes; GN needs one bin dir.
    facade = args.work / "clang"
    (facade / "bin").mkdir(parents=True, exist_ok=True)
    for item in (args.llvm / "bin").iterdir():
        link = facade / "bin" / item.name
        if link.is_symlink():
            link.unlink()
        if not link.exists():
            link.symlink_to(item)
    for name, target in (("bin/lld-link", args.lld / "bin/lld-link"),
                         ("bin/ld.lld", args.lld / "bin/ld.lld"),
                         ("lib", args.llvm / "lib"), ("include", args.llvm / "include")):
        link = facade / name
        if link.is_symlink():
            link.unlink()
        if link.exists():
            raise RuntimeError(f"Refusing to replace non-symlink {link}")
        link.symlink_to(target)
    return facade


def engine(args):
    identity = toolchain_identity(args)
    source = prepare_source(args)
    build, sdk = args.work / "engine", args.work / "sdk"
    sdk_receipt = sdk / "arbiter-engine-build.json"
    sdk_receipt.unlink(missing_ok=True)
    env = os.environ.copy()
    env.update(ARBITER_WINDOWS_SYSROOT=str(args.work / "sysroot"),
               ARBITER_WINDOWS_LLVM=str(args.llvm),
               WindowsSDKVersion=PINS["sdk"] + ".0",
               WINDOWSSDKDIR=str(args.work / "sysroot/sdk"),
               VSINSTALLDIR=str(args.work / "sysroot"), NINJAFLAGS=f"-j{args.jobs}")
    env["PATH"] = str(args.work / "clang/bin") + os.pathsep + env["PATH"]
    flags = common_flags(args) + [
        f"-DARBITER_WINDOWS_CLANG_PREFIX={clang_facade(args)}",
        f"-DCMAKE_INSTALL_PREFIX={sdk}", f"-DPython3_EXECUTABLE={python_path(args)}",
        f"-DQWELibClang_BIN_PATH={args.llvm / 'bin'}",
        f"-DLibclang_LIBRARY={args.llvm / 'lib' / ('libclang.dylib' if sys.platform == 'darwin' else 'libclang.so')}",
        f"-DGn_EXECUTABLE={args.host_qt / 'libexec/gn'}",
        "-DQT_BUILD_EXAMPLES=OFF", "-DQT_BUILD_TESTS=OFF", "-DFEATURE_qtwebengine_build=ON",
        "-DFEATURE_qtpdf_build=ON", "-DQT_SHOW_EXTRA_IDE_SOURCES=OFF",
        "-DFEATURE_webengine_extensions=ON", "-DFEATURE_webengine_geolocation=ON",
        # Windows uses native decoders with codec-free FFmpeg, even on Linux hosts.
        "-DFEATURE_webengine_proprietary_codecs=ON", "-DFEATURE_webengine_system_ffmpeg=OFF"]
    configure(args, source, build, flags, env=env)
    if args.configure_only:
        return
    run("cmake", "--build", build, "--parallel", args.jobs, env=env)
    if toolchain_identity(args) != identity:
        raise RuntimeError("Build inputs changed during compilation; refusing to install")
    verify_media(source, build)
    run("cmake", "--install", build, env=env)
    pe_x64(sdk / "bin/Qt6WebEngineCore.dll")
    shutil.copyfile(ROOT / "engine/manifest.json", sdk / "arbiter-engine-manifest.json")
    shutil.copyfile(ROOT / "engine/LICENSE", sdk / "arbiter-engine-LICENSE.txt")
    atomic_json(sdk / "arbiter-cross-build.json", identity)
    atomic_json(sdk_receipt, {"inputs_sha256": fingerprint(), "qt": ENGINE["qt"],
                             "platform": "win32", "architecture": "x86_64"})


def app(args):
    identity = toolchain_identity(args)
    sdk = args.work / "sdk"
    receipt = verify_sdk(sdk)
    if receipt.get("platform") != "win32" or receipt.get("architecture") != "x86_64":
        raise RuntimeError("The engine SDK must target Windows x64, not the host OS")
    if json.loads((sdk / "arbiter-cross-build.json").read_text()) != identity:
        raise RuntimeError("Cross toolchain/engine inputs changed; rebuild the engine")
    pe_x64(sdk / "bin/Qt6WebEngineCore.dll")
    openssl = (args.openssl_prefix or args.work / "openssl/sdk").resolve()
    library = openssl / "lib/libcrypto.lib"
    if not (openssl / "include/openssl/crypto.h").is_file() or not library.is_file():
        raise RuntimeError("OpenSSL SDK needs include/openssl/crypto.h and lib/libcrypto.lib")
    info = run(args.llvm / "bin/llvm-readobj", "--file-headers", library, capture=True)
    if "COFF-x86-64" not in info or "Mach-O" in info or "ELF" in info:
        raise RuntimeError("OpenSSL must contain Windows x64 COFF objects/imports")
    env = os.environ.copy()
    cargo = run("rustup", "which", "--toolchain", DEPS["rust"], "cargo", capture=True).strip()
    env["PATH"] = str(Path(cargo).parent) + os.pathsep + env["PATH"]
    env["RUSTUP_TOOLCHAIN"] = DEPS["rust"]
    env["CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER"] = str(args.lld / "bin/lld-link")
    env["CC_x86_64_pc_windows_msvc"] = str(args.llvm / "bin/clang-cl")
    env["AR_x86_64_pc_windows_msvc"] = str(args.llvm / "bin/llvm-lib")
    sysroot = args.work / "sysroot"
    env["INCLUDE"] = ";".join(str(sysroot / suffix) for suffix in (
        "crt/include", "sdk/include/ucrt", "sdk/include/shared", "sdk/include/um", "sdk/include/winrt", "sdk/include/cppwinrt"))
    # LLD's COFF driver uses Windows separators even when running on macOS.
    env["LIB"] = ";".join(str(sysroot / suffix) for suffix in (
        "crt/lib/x86_64", "sdk/lib/ucrt/x86_64", "sdk/lib/um/x86_64"))
    run("rustup", "target", "add", "--toolchain", DEPS["rust"], "x86_64-pc-windows-msvc")
    build = args.work / "app"
    flags = common_flags(args) + [
        f"-DARBITER_WINDOWS_OPENSSL={openssl}", f"-DOPENSSL_ROOT_DIR={openssl}",
        "-DOPENSSL_USE_STATIC_LIBS=TRUE",
        "-DARBITER_PATCHED_WEBENGINE=ON", "-DARBITER_STRICT_ENGINE_IDENTITY=ON",
        f"-DARBITER_WEBENGINE_PREFIX={sdk}", "-DARBITER_WEBENGINE_SDK_RUNTIME=OFF",
        "-DARBITER_RUST_TARGET=x86_64-pc-windows-msvc",
        f"-DPython3_EXECUTABLE={python_path(args)}"]
    configure(args, ROOT, build, flags, env=env)
    run("cmake", "--build", build, "--parallel", args.jobs, env=env)
    pe_x64(build / "arbiter.exe")
    print(f"Application compiled: {build / 'arbiter.exe'}. This is not a deployed release bundle.")


def openssl(args):
    from openssl import build
    print(f"Windows OpenSSL SDK: {build(args, run, atomic_json)}")


def all_phases(args):
    prepared = args.work / "prepared.json"
    if not prepared.exists() or json.loads(prepared.read_text()) != {"pins": PINS, "qt": ENGINE["qt"]}:
        prepare(args)
    openssl(args)
    probe(args)
    engine(args)
    app(args)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=("prepare", "probe", "engine", "openssl", "app", "all"))
    parser.add_argument("--work", type=Path, default=ROOT / "build-windows-cross")
    parser.add_argument("--host-qt", type=Path, required=True)
    parser.add_argument("--llvm", type=Path, required=True)
    parser.add_argument("--lld", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=20)
    parser.add_argument("--accept-microsoft-license", action="store_true")
    parser.add_argument("--configure-only", action="store_true")
    parser.add_argument("--openssl-prefix", type=Path)
    args = parser.parse_args()
    if sys.platform not in ("darwin", "linux"):
        parser.error("Use tools/release/build-windows.ps1 for native Windows builds")
    import fcntl
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if args.configure_only and args.phase != "engine":
        parser.error("--configure-only applies to the engine phase")
    for key in ("work", "host_qt", "llvm", "lld"):
        setattr(args, key, getattr(args, key).resolve())
    if args.phase in ("engine", "app", "all") and any(
            any(character.isspace() for character in str(path))
            for path in (ROOT, args.work, args.host_qt, args.llvm, args.lld)):
        parser.error("Chromium cross-build paths must not contain whitespace")
    try:
        args.work.mkdir(parents=True, exist_ok=True)
        # Preparation replaces SDK files; never allow it alongside a build.
        with (args.work / ".cross-build.lock").open("w") as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                raise RuntimeError("Another cross-build phase is using this work directory") from None
            toolchain_identity(args)
            (all_phases if args.phase == "all" else globals()[args.phase])(args)
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Cross-build failed: {error}\n")


if __name__ == "__main__":
    main()
