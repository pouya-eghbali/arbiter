import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location("arbiter_rc", Path(__file__).with_name("rc.py"))
rc = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(rc)


class ResourceEncodingTests(unittest.TestCase):
    def test_utf16_resource_preserves_non_ascii_text(self):
        text = 'STRINGTABLE\nBEGIN\n1 "Grüße 日本語"\nEND\n'
        for encoding, bom in (("utf-16-le", b"\xff\xfe"), ("utf-16-be", b"\xfe\xff")):
            self.assertEqual(rc.resource_bytes(bom + text.encode(encoding)), text.encode("utf-8"))

    def test_utf8_input_is_preserved(self):
        data = '1 "Grüße"'.encode("utf-8")
        self.assertEqual(rc.resource_bytes(data), data)


if __name__ == "__main__":
    unittest.main()
