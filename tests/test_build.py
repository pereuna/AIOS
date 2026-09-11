"""Check the pinned tokenizer data and the generated UEFI executable."""
import hashlib
from pathlib import Path
import struct
import unittest

from tools.export_model import unicode_ranges

ROOT = Path(__file__).resolve().parents[1]


class BuildTests(unittest.TestCase):
    def test_unicode_table_matches_original_model(self):
        # Digest of the 807 packed ranges in the original pinned model.bin.
        # A host Unicode upgrade must not change any tokenizer categories.
        ranges = unicode_ranges()
        packed = b"".join(struct.pack("<3I", *item) for item in ranges)
        self.assertEqual(len(ranges), 807)
        self.assertEqual(
            hashlib.sha256(packed).hexdigest(),
            "6fb5d92fe572c5c6625489dd634a3f7ad1f70d365627bfe30b78c0b764204c0e",
        )

    def test_efi_is_x86_64_application(self):
        raw = (ROOT / "dist/EFI/BOOT/BOOTX64.EFI").read_bytes()
        self.assertEqual(raw[:2], b"MZ")
        pe, = struct.unpack_from("<I", raw, 0x3C)
        self.assertEqual(raw[pe:pe + 4], b"PE\0\0")
        machine, = struct.unpack_from("<H", raw, pe + 4)
        self.assertEqual(machine, 0x8664)  # AMD64
        optional = pe + 24
        magic, = struct.unpack_from("<H", raw, optional)
        self.assertEqual(magic, 0x20B)  # PE32+
        subsystem, = struct.unpack_from("<H", raw, optional + 68)
        self.assertEqual(subsystem, 10)  # EFI application
        # Firmware must be able to relocate the executable when loading it.
        directories, = struct.unpack_from("<I", raw, optional + 108)
        self.assertGreater(directories, 5)
        reloc_rva, reloc_size = struct.unpack_from("<2I", raw, optional + 112 + 5 * 8)
        self.assertGreater(reloc_rva, 0)
        self.assertGreater(reloc_size, 0)


if __name__ == "__main__":
    unittest.main()
