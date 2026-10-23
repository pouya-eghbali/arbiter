#!/usr/bin/env python3
"""Build Arbiter's pinned Qt WebEngine without replacing the installed Qt kit."""
import argparse
import json
import os
import platform
from pathlib import Path
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
MANIFEST = json.loads((HERE / "manifest.json").read_text())

def run(*args, cwd=None, capture=False):
    print("+", subprocess.list2cmdline([str(a) for a in args]), flush=True)
    return subprocess.run([str(a) for a in args], cwd=cwd, check=True, text=True,
                          stdout=subprocess.PIPE if capture else None).stdout

def patch(root, file):
    # Apply once; refuse incompatible revisions or unrelated edited source.
    text = file.read_text()
    current = run("git", "diff", "--", cwd=root, capture=True)
    if current == text:
        return
    if current:
        raise RuntimeError(f"Refusing to overwrite edited files in {root}")
    run("git", "apply", "--check", file, cwd=root)
    run("git", "apply", file, cwd=root)

def configure_macos_helper(sdk, qt_prefix):
    # CMake's install_name_tool optimization preserves existing RPATH order.
    # A base-Qt path retained from the build can therefore precede the SDK and
    # load stock WebEngine in the child. Normalize the installed helper order.
    for helper in sdk.rglob("QtWebEngineProcess"):
        if not helper.is_file() or any(part.endswith(".dSYM") for part in helper.parts):
            continue
        output = run("otool", "-l", helper, capture=True).splitlines()
        paths = []
        for i, line in enumerate(output):
            if line.strip() == "cmd LC_RPATH":
                paths.append(output[i + 2].strip().removeprefix("path ").rsplit(" (offset ", 1)[0])
        first, last = str(sdk / "lib"), str(qt_prefix / "lib")
        ordered = [first] + [p for p in paths if p not in (first, last)] + [last]
        if paths != ordered:
            # install_name_tool rejects deleting and adding the same path in
            # one invocation, even when the purpose is changing its order.
            if paths:
                run("install_name_tool", *[arg for p in paths for arg in ("-delete_rpath", p)], helper)
            run("install_name_tool", *[arg for p in ordered for arg in ("-add_rpath", p)], helper)
        # RPATH edits invalidate the linker's ad-hoc signature on Apple Silicon.
        run("codesign", "--force", "--sign", "-", helper)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qt-prefix", type=Path, required=True)
    parser.add_argument("--work", type=Path, default=HERE.parent / "build-passkey-toolchain")
    parser.add_argument("--jobs", type=int, default=6)
    parser.add_argument("--macos-arch", choices=("arm64", "x86_64"),
                        default=platform.machine(), help="macOS SDK architecture (defaults to the running Python architecture)")
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--configure-only", action="store_true")
    args = parser.parse_args()
    args.work = args.work.resolve(); args.qt_prefix = args.qt_prefix.resolve()
    if args.jobs < 1: parser.error("--jobs must be positive")
    args.work.mkdir(parents=True, exist_ok=True)
    source, build, sdk = ((args.work / name).resolve() for name in ("source", "build", "sdk"))
    if not source.exists():
        run("git", "init", source)
        run("git", "remote", "add", "origin", MANIFEST["webengine_repository"], cwd=source)
        run("git", "fetch", "--depth=1", "origin", MANIFEST["webengine_commit"], cwd=source)
        run("git", "checkout", "--detach", "FETCH_HEAD", cwd=source)
    head = run("git", "rev-parse", "HEAD", cwd=source, capture=True).strip()
    if head != MANIFEST["webengine_commit"]:
        raise RuntimeError("Source revision differs from engine/manifest.json")
    run("git", "-c", "url.https://github.com/.insteadOf=git@github.com:",
        "submodule", "update", "--init", "--depth=1", "src/3rdparty", cwd=source)
    chromium = source / "src/3rdparty"
    if run("git", "rev-parse", "HEAD", cwd=chromium, capture=True).strip() != MANIFEST["chromium_commit"]:
        raise RuntimeError("Chromium revision differs from engine/manifest.json")
    patch(source, HERE / "patches/qtwebengine.patch")
    patch(chromium, HERE / "patches/chromium.patch")
    for file in (HERE / "overlay").rglob("*"):
        if file.is_file():
            destination = source / file.relative_to(HERE / "overlay")
            if destination.exists():
                if destination.read_bytes() != file.read_bytes():
                    raise RuntimeError(f"Overlay differs: {destination}. Review/remove the old overlay before updating.")
                # Preserve timestamps so a resumed build reuses compiled objects.
                continue
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(file, destination)
    if args.prepare_only: return
    flags = ["-DCMAKE_BUILD_TYPE=Release", "-DQT_BUILD_EXAMPLES=OFF", "-DQT_BUILD_TESTS=OFF",
             "-DFEATURE_qtwebengine_build=ON", "-DFEATURE_qtpdf_build=OFF",
             "-DQT_SHOW_EXTRA_IDE_SOURCES=OFF",
             f"-DPython3_EXECUTABLE={sys.executable}", f"-DCMAKE_PREFIX_PATH={args.qt_prefix}",
             f"-DCMAKE_INSTALL_PREFIX={sdk}"]
    if sys.platform != "win32":
        # This development SDK contains WebEngine only. Its helper is a separate
        # executable and cannot inherit the application's RPATH to base Qt.
        flags += [f"-DQT_EXTRA_RPATHS={sdk / 'lib'};{args.qt_prefix / 'lib'}"]
    if sys.platform == "darwin":
        # The official Qt kit is universal and exports QT_IS_MACOS_UNIVERSAL=ON.
        # Our Chromium build is single-architecture; inheriting that flag makes
        # Qt select its universal linker path and a nonexistent x86_64 response file.
        flags += ["-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0",
                  f"-DCMAKE_OSX_ARCHITECTURES={args.macos_arch}",
                  "-DQT_IS_MACOS_UNIVERSAL=OFF"]
    # Qt launches its own nested Ninja; the outer --parallel flag does not limit it.
    os.environ["NINJAFLAGS"] = f"-j{args.jobs}"
    run("cmake", "-G", "Ninja", "-S", source, "-B", build, *flags)
    if args.configure_only: return
    run("cmake", "--build", build, "--parallel", args.jobs)
    run("cmake", "--install", build)
    if sys.platform == "darwin": configure_macos_helper(sdk, args.qt_prefix)
    shutil.copyfile(HERE / "manifest.json", sdk / "arbiter-engine-manifest.json")
    shutil.copyfile(HERE / "LICENSE", sdk / "arbiter-engine-LICENSE.txt")
    print(f"Patched engine installed to {sdk}. Base Qt remains at {args.qt_prefix}.")

if __name__ == "__main__":
    main()
