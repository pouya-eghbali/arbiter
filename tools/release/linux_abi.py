"""Identify Linux media ABIs from ELF dependencies, never from a build label."""
import re

ABIS = {
    6: {'libavcodec': 60, 'libavformat': 60, 'libavutil': 58},
    7: {'libavcodec': 61, 'libavformat': 61, 'libavutil': 59},
    8: {'libavcodec': 62, 'libavformat': 62, 'libavutil': 60},
    9: {'libavcodec': 63, 'libavformat': 63, 'libavutil': 61},
}


def identify(needed):
    libraries = {}
    for line in needed.splitlines():
        match = re.fullmatch(r'(libavcodec|libavformat|libavutil)\.so\.(\d+)', line.strip())
        if match:
            name, version = match.groups()
            if name in libraries:
                raise RuntimeError('Engine links multiple versions of ' + name)
            libraries[name] = int(version)
    for major, expected in ABIS.items():
        if libraries == expected:
            return dict(major=major, libraries=libraries)
    raise RuntimeError('Unsupported or inconsistent system FFmpeg ABI: ' + repr(libraries))


def suffix(major):
    if major not in ABIS:
        raise RuntimeError('Unsupported FFmpeg major: ' + str(major))
    # Preserve the existing Ubuntu archive name. Additional ABIs are explicit.
    return '' if major == 6 else f'-ffmpeg{major}'
