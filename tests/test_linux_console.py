"""End-to-end console tests; no model download or Python packages required."""
import json
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "linux"))
from console import Console, Log, ModelWorker, visible
from types import SimpleNamespace


class LinuxConsoleTests(unittest.TestCase):
    def run_console(self, commands, *args):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run(
                [sys.executable, str(ROOT / "linux/console.py"), "--log-dir", directory, *args],
                input=commands, capture_output=True, text=True, timeout=30, cwd=ROOT)
            events = [json.loads(line) for line in
                      (Path(directory) / "console.jsonl").read_text().splitlines()]
            transcript = (Path(directory) / "console.log").read_text()
        self.assertEqual(result.stderr, "")
        return result, events, transcript

    def test_model_is_lazy_and_bad_model_failure_is_logged(self):
        with tempfile.TemporaryDirectory() as directory:
            model = Path(directory) / "invalid.bin"
            model.write_bytes(b"invalid model")
            result, _, _ = self.run_console("/stats\n/quit\n", "--model", str(model))
            self.assertEqual(result.returncode, 0)
            self.assertIn("loaded=False", result.stdout)
            result, events, transcript = self.run_console("Hello\n/quit\n", "--model", str(model))
            self.assertEqual(result.returncode, 1)
            self.assertIn("expected an 872253632-byte QWENQ4", transcript)
            self.assertTrue(any(e["event"] == "error" for e in events))

    def test_terminal_controls_are_escaped(self):
        self.assertEqual(visible("\x1b[2Jtest\r\x9b\nö"), "\\x1b[2Jtest\\x0d\\x9b\nö")

    def test_stream_framing_handles_split_utf8_and_command_like_text(self):
        # Byte-level token boundaries can split a Unicode character; the
        # generated word ERROR must remain output, not become a worker error.
        wire = (b"BEGIN 3 0\nPREFILL 3 3\nTEXT 1\n\xc3TEXT 1\n\xb6"
                b"TEXT 12\n\nERROR fake\nDONE 8 4 complete 0.1 0.2\n")
        worker = ModelWorker.__new__(ModelWorker)
        worker.process = SimpleNamespace(stdout=io.BytesIO(wire), stdin=io.BytesIO())
        with tempfile.TemporaryDirectory() as directory:
            log = Log(Path(directory))
            try:
                reply = worker.chat("hi", log)
                self.assertEqual(reply, "ö\nERROR fake\n")
                self.assertEqual(worker.process.stdin.getvalue(), b"CHAT 2\nhi")
            finally:
                log.close(0)


if __name__ == "__main__":
    unittest.main()
