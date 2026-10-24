import contextlib
import io
from pathlib import Path
import tempfile
import unittest

from media_policy import verify


class MediaPolicyTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.args = self.root / 'build/src/core/Release/arm64/args.gn'
        self.args.parent.mkdir(parents=True)
        self.config = self.root / 'source/src/3rdparty/chromium/third_party/ffmpeg/chromium/config/Chromium/mac/arm64'
        (self.config / 'libavcodec').mkdir(parents=True)
        (self.config / 'config_components.h').write_text('#define CONFIG_AAC_DECODER 0\n#define CONFIG_H264_DECODER 0\n')
        (self.config / 'libavcodec/codec_list.c').write_text('static const FFCodec * const codec_list[] = { NULL };\n')
        self.native = 'target_os="mac"\ntarget_cpu="arm64"\nproprietary_codecs=true\nmedia_use_openh264=false\nrtc_use_h264=false\nffmpeg_branding="Chromium"\n'
        self.args.write_text(self.native)

    def audit(self):
        with contextlib.redirect_stdout(io.StringIO()):
            verify(self.root / 'source', self.root / 'build')

    def test_native_build(self):
        self.audit()

    def test_rejects_bundled_decoders(self):
        for codec in ('AAC', 'H264'):
            with self.subTest(codec=codec):
                file = self.config / 'config_components.h'
                original = file.read_text()
                file.write_text(original.replace(f'CONFIG_{codec}_DECODER 0', f'CONFIG_{codec}_DECODER 1'))
                with self.assertRaises(RuntimeError):
                    self.audit()
                file.write_text(original)

    def test_rejects_decoder_registration(self):
        (self.config / 'libavcodec/codec_list.c').write_text('&ff_aac_decoder,\n')
        with self.assertRaises(RuntimeError):
            self.audit()

    def test_rejects_software_encoder_or_chrome_config(self):
        for old, new in [('rtc_use_h264=false', 'rtc_use_h264=true'),
                         ('media_use_openh264=false', 'media_use_openh264=true'),
                         ('ffmpeg_branding="Chromium"', 'ffmpeg_branding="Chrome"')]:
            with self.subTest(new=new):
                self.args.write_text(self.native.replace(old, new))
                with self.assertRaises(RuntimeError):
                    self.audit()

    def test_linux_requires_external_ffmpeg(self):
        linux = self.native.replace('target_os="mac"', 'target_os="linux"').replace('"Chromium"', '"Chrome"')
        self.args.write_text(linux)
        with self.assertRaises(RuntimeError):
            self.audit()
        self.args.write_text(linux + 'use_system_ffmpeg=true\n')
        self.audit()

    def test_missing_build_is_not_success(self):
        self.args.unlink()
        with self.assertRaises(RuntimeError):
            self.audit()


if __name__ == '__main__':
    unittest.main()
