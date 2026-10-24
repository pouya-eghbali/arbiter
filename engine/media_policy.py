"""Reject SDK builds that bundle H.264/AAC decoders or OpenH264 encoding.

This is a build-integrity check, not a patent-license determination. Linux
deployment must additionally leave the external FFmpeg shared libraries on the
user's system rather than copy them into an application package.
"""
import argparse
from pathlib import Path
import re
import sys


def verify(source: Path, build: Path):
    configurations = list((build / "src/core").glob("**/args.gn"))
    if not configurations:
        raise RuntimeError("No generated Chromium configuration to audit")
    for file in configurations:
        args = {}
        for line in file.read_text().splitlines():
            match = re.fullmatch(r'\s*(\w+)\s*=\s*(.*?)\s*', line)
            if match:
                args[match[1]] = match[2]
        def require(name, value):
            if args.get(name) != value:
                raise RuntimeError(f"{file}: expected {name}={value}, got {args.get(name)}")
        require("proprietary_codecs", "true")
        require("media_use_openh264", "false")
        require("rtc_use_h264", "false")
        host_os = "mac" if sys.platform == "darwin" else "win" if sys.platform == "win32" else "linux"
        target_os = args.get("target_os", f'"{host_os}"').strip('"')
        if args.get("use_system_ffmpeg") == "true":
            if target_os != "linux":
                raise RuntimeError(f"{file}: system FFmpeg policy is only supported on Linux")
            require("ffmpeg_branding", '"Chrome"')
            print(f"Media policy: {file}: external system FFmpeg; no bundled OpenH264")
            continue
        require("ffmpeg_branding", '"Chromium"')
        if target_os not in ("mac", "win"):
            raise RuntimeError(f"{file}: native codecs unavailable for {target_os}")
        cpu = args.get("target_cpu", "").strip('"')
        arch = {"x64": "x64", "x86": "ia32", "arm64": "arm64"}.get(cpu)
        if not arch:
            raise RuntimeError(f"{file}: unsupported native codec CPU {cpu}")
        os_config = "win-msvc" if target_os == "win" and args.get("is_clang") == "false" else target_os
        config = source / "src/3rdparty/chromium/third_party/ffmpeg/chromium/config/Chromium" / os_config / arch
        components = (config / "config_components.h").read_text()
        codecs = (config / "libavcodec/codec_list.c").read_text()
        for codec in ("AAC", "H264"):
            if not re.search(rf"^#define CONFIG_{codec}_DECODER 0$", components, re.M):
                raise RuntimeError(f"{config}: bundled {codec} decoder enabled")
            if f"ff_{codec.lower()}_decoder" in codecs:
                raise RuntimeError(f"{config}: bundled {codec} decoder registered")
        print(f"Media policy: {file}: native {target_os} codecs; bundled AAC/H.264/OpenH264 excluded")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--build", required=True, type=Path)
    options = parser.parse_args()
    verify(options.source, options.build)
