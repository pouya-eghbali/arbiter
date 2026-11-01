import argparse
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("windows_cross", Path(__file__).with_name("windows.py"))
cross = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(cross)


class CrossBuildTests(unittest.TestCase):
    def test_toolchain_changes_refresh_cmake_but_unchanged_builds_keep_cache(self):
        with tempfile.TemporaryDirectory() as folder:
            build = Path(folder) / "build"
            with patch.object(cross, "toolchain_identity", return_value={"llvm": "22.1.8"}), \
                 patch.object(cross, "run") as run:
                cross.configure(None, Path("source"), build, ["-Dflag=1"])
                self.assertIn("--fresh", run.call_args.args)
                cross.configure(None, Path("source"), build, ["-Dflag=1"])
                self.assertNotIn("--fresh", run.call_args.args)
                cross.configure(None, Path("source"), build, ["-Dflag=2"])
                self.assertIn("--fresh", run.call_args.args)

    def test_binary_attribute_workaround_never_hides_changed_bytes(self):
        with tempfile.TemporaryDirectory() as folder:
            source = Path(folder)
            pdf = source / "src/3rdparty/chromium/third_party/dawn/third_party/khronos/OpenGL-Registry/xml/readme.pdf"
            pdf.parent.mkdir(parents=True)
            pdf.write_bytes(b"changed PDF")
            with patch.object(cross, "run", side_effect=["original\n", "modified\n"]):
                with self.assertRaisesRegex(RuntimeError, "PDF bytes changed"):
                    cross.preserve_binary_pdf(source)
            self.assertFalse((source / ".git/info/attributes").exists())

    def test_rejects_native_and_wrong_architecture_executables(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "binary"
            for content in (b"\x7fELF", b"\xcf\xfa\xed\xfe", b"MZ"):
                path.write_bytes(content)
                with self.assertRaises(RuntimeError):
                    cross.pe_x64(path)
            header = bytearray(64)
            header[:2] = b"MZ"
            struct.pack_into("<I", header, 60, 64)
            for machine in (0xAA64, 0x14C):
                path.write_bytes(header + b"PE\0\0" + struct.pack("<H", machine))
                with self.assertRaises(RuntimeError):
                    cross.pe_x64(path)
            path.write_bytes(header + b"PE\0\0\x64\x86")
            cross.pe_x64(path)

    def test_license_refusal_happens_before_any_download(self):
        with patch.object(cross, "run") as run:
            with self.assertRaisesRegex(RuntimeError, "accept-microsoft-license"):
                cross.prepare(argparse.Namespace(accept_microsoft_license=False))
            run.assert_not_called()

    def test_host_engine_cannot_be_used_as_windows_sdk(self):
        for host in ("darwin", "linux"):
            args = argparse.Namespace(work=Path("unused"))
            with patch.object(cross, "toolchain_identity", return_value={}), \
                 patch.object(cross, "verify_sdk", return_value={"platform": host, "architecture": "x86_64"}), \
                 patch.object(cross, "run") as run:
                with self.assertRaisesRegex(RuntimeError, "Windows x64"):
                    cross.app(args)
                run.assert_not_called()

    def test_changed_cross_patch_invalidates_sdk_before_configuring_app(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "sdk").mkdir()
            (root / "sdk/arbiter-cross-build.json").write_text(json.dumps({"cross_inputs_sha256": "old"}))
            args = argparse.Namespace(work=root)
            with patch.object(cross, "toolchain_identity", return_value={"cross_inputs_sha256": "new"}), \
                 patch.object(cross, "verify_sdk", return_value={"platform": "win32", "architecture": "x86_64"}), \
                 patch.object(cross, "run") as run:
                with self.assertRaisesRegex(RuntimeError, "inputs changed"):
                    cross.app(args)
                run.assert_not_called()

    def test_linux_host_still_uses_windows_media_policy(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "engine").mkdir()
            (root / "engine/CMakeCache.txt").write_text(
                "QT_FEATURE_qtwebengine_build:INTERNAL=ON\nQT_FEATURE_qtpdf_build:INTERNAL=ON\n")
            args = argparse.Namespace(work=root, llvm=root / "llvm", host_qt=root / "hostqt",
                                      jobs=20, configure_only=True)
            with patch.object(cross, "toolchain_identity", return_value={}), \
                 patch.object(cross, "prepare_source", return_value=root / "source"), \
                 patch.object(cross, "common_flags", return_value=[]), \
                 patch.object(cross, "clang_facade", return_value=root / "clang"), \
                 patch.object(cross.sys, "platform", "linux"), patch.object(cross, "run") as run:
                cross.engine(args)
                command = run.call_args.args
                self.assertIn("-DFEATURE_webengine_system_ffmpeg=OFF", command)
                self.assertIn("-DFEATURE_webengine_proprietary_codecs=ON", command)
                self.assertNotIn("-DFEATURE_webengine_system_ffmpeg=ON", command)
                self.assertFalse((root / "sdk/arbiter-engine-build.json").exists())

    def test_failed_compile_does_not_leave_successful_receipt(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "engine").mkdir()
            (root / "engine/CMakeCache.txt").write_text(
                "QT_FEATURE_qtwebengine_build:INTERNAL=ON\nQT_FEATURE_qtpdf_build:INTERNAL=ON\n")
            (root / "sdk").mkdir()
            receipt = root / "sdk/arbiter-engine-build.json"
            receipt.write_text("stale")
            args = argparse.Namespace(work=root, llvm=root / "llvm", host_qt=root / "qt",
                                      jobs=20, configure_only=False)
            with patch.object(cross, "toolchain_identity", return_value={}), \
                 patch.object(cross, "prepare_source", return_value=root / "source"), \
                 patch.object(cross, "common_flags", return_value=[]), \
                 patch.object(cross, "clang_facade", return_value=root / "clang"), \
                 patch.object(cross, "run", side_effect=[None, RuntimeError("compile failed")]), \
                 patch.object(cross, "verify_media") as audit:
                with self.assertRaisesRegex(RuntimeError, "compile failed"):
                    cross.engine(args)
                self.assertFalse(receipt.exists())
                audit.assert_not_called()


if __name__ == "__main__":
    unittest.main()
