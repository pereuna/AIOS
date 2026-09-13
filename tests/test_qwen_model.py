"""Qwen2.5-Coder tokenizer reference cases and sharded BF16 input validation."""
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import numpy as np
from tools import export_model as exporter
from tools.split_model import split_model

ROOT = Path(__file__).resolve().parents[1]


class QwenModelTests(unittest.TestCase):
    def test_tokenizer_matches_huggingface(self):
        # Recorded with the pinned upstream tokenizer.json using HF tokenizers.
        cases = json.loads((ROOT / 'tests/tokenizer_cases.json').read_text())
        for case in cases:
            with self.subTest(text=case['text']):
                output = subprocess.check_output([
                    str(ROOT / '.build/test-inference'), str(ROOT / 'model.bin'),
                    case.get('mode', '--tokenize'), case['text']], text=True)
                self.assertEqual(list(map(int, output.split())), case['ids'])

    def test_sharded_bf16_reader(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            names = {'first', 'second'}
            index = {'weight_map': {n: n + '.safetensors' for n in names}}
            path = root / 'model.safetensors.index.json'
            path.write_text(json.dumps(index))
            for n in names:
                header = json.dumps({n: {'dtype': 'BF16', 'shape': [1, 2],
                                         'data_offsets': [0, 4]}}).encode()
                (root / (n + '.safetensors')).write_bytes(
                    struct.pack('<Q', len(header)) + header + struct.pack('<2H', 0x3f80, 0xc000))
            with patch.object(exporter, 'tensor_names', return_value=names):
                tensors = exporter.SafeTensors(path)
                for n in names:
                    np.testing.assert_array_equal(tensors.bf16(n, (1, 2)), [[1., -2.]])
                for n in names:
                    with patch.object(exporter, 'tensor_names', return_value={n}):
                        single = exporter.SafeTensors(root / (n + '.safetensors'))
                    np.testing.assert_array_equal(single.bf16(n, (1, 2)), [[1., -2.]])
                with self.assertRaisesRegex(ValueError, 'metadata'):
                    tensors.bf16('first', (2, 1))
                shard = root / 'second.safetensors'
                shard.write_bytes(shard.read_bytes()[:-1])
                with self.assertRaisesRegex(ValueError, 'truncated tensor'):
                    tensors.bf16('second', (1, 2))

    def test_fat32_parts_reassemble_exactly(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'model.bin'
            content = bytes(range(251)) * 7
            source.write_bytes(content)
            with patch('tools.split_model.PART_BYTES', 512):
                parts = split_model(source, root / 'dist')
                self.assertEqual(len(parts), 4)
                self.assertEqual(b''.join(p.read_bytes() for p in parts), content)
                self.assertTrue(all(p.stat().st_size <= 512 for p in parts))
                source.write_bytes(content[:20])
                parts = split_model(source, root / 'dist')
                self.assertEqual(len(parts), 1)
                self.assertFalse((root / 'dist/model.001').exists())
