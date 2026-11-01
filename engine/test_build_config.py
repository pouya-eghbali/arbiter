from pathlib import Path
import tempfile
import unittest

from build import require_enabled_modules


class BuildConfigurationTest(unittest.TestCase):
    def test_rejects_silently_disabled_modules(self):
        for disabled in ("qtwebengine", "qtpdf"):
            with self.subTest(disabled=disabled), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                (root / "CMakeCache.txt").write_text("".join(
                    f"QT_FEATURE_{module}_build:INTERNAL={'OFF' if module == disabled else 'ON'}\n"
                    for module in ("qtwebengine", "qtpdf")))
                with self.assertRaisesRegex(RuntimeError, f"Qt disabled {disabled}"):
                    require_enabled_modules(root)

    def test_requires_both_modules(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CMakeCache.txt").write_text(
                "QT_FEATURE_qtwebengine_build:INTERNAL=ON\nQT_FEATURE_qtpdf_build:INTERNAL=ON\n")
            require_enabled_modules(root)
