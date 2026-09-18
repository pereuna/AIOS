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

    def test_script_compilation_execution_assertions_and_log(self):
        result, events, transcript = self.run_console(
            "", "--asm-only", "--script", str(ROOT / "linux/examples/smoke.console"))
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual([e["value"] for e in events if e["event"] == "execution"], [42, 0, 6, 42])
        self.assertEqual(len([e for e in events if e["event"] == "assertion"]), 4)
        self.assertIn("asm1: ok; value=42", transcript)
        self.assertEqual(events[-1]["code"], 0)

    def test_compile_errors_do_not_execute_and_allow_next_command(self):
        commands = ("/asm asm1; li r0 6; li r1 7; multiply r0 r1; exit r0; end\n"
                    "/asm asm1; li r0 42; exit r0; end\n/assert 42\n/quit\n")
        result, events, _ = self.run_console(commands, "--asm-only")
        self.assertEqual(result.returncode, 1)
        self.assertIn("compile error", result.stdout)
        self.assertIn("assert: PASS (42)", result.stdout)
        self.assertEqual([e["success"] for e in events if e["event"] == "execution"], [False, True])

    def test_division_by_zero_and_loop_limit(self):
        commands = ("/asm asm1; li r0 1; li r1 0; udiv r0 r1; exit r0; end\n"
                    "/asm asm1; label l0; jmp l0; exit r0; end\n/quit\n")
        result, events, _ = self.run_console(commands, "--asm-only", "--step-limit", "30")
        self.assertEqual(result.returncode, 1)
        executions = [e for e in events if e["event"] == "execution"]
        self.assertEqual([e["error"] for e in executions], ["division_by_zero", "step_limit"])
        self.assertEqual(executions[-1]["steps"], 30)

    def test_failed_assertion_and_incomplete_multiline_fail_batch(self):
        for command in ("/assert 42\n", "/asm\nasm1\nli r0 42\n"):
            result, events, _ = self.run_console(command, "--asm-only")
            self.assertEqual(result.returncode, 1)
            self.assertEqual(events[-1]["code"], 1)

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

    def test_generated_program_requires_explicit_run(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Log(Path(directory))
            console = Console(SimpleNamespace(step_limit=100, asm_only=True), log)
            try:
                console.reply = "/asm asm1; li r0 42; exit r0; end"
                console.command("/last")
                self.assertIsNone(console.result)
                console.command("/run")
                self.assertEqual(console.result["value"], 42)
                console.command("/reset")
                with self.assertRaises(ValueError):
                    console.command("/run")
            finally:
                console.close()
                log.close(0)

    def test_source_file_paths_with_spaces(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "test code.asm1"
            source.write_text("asm1\nli r0 123\nexit r0\nend\n")
            result, _, _ = self.run_console(f"/load {source}\n/assert 123\n", "--asm-only")
            self.assertEqual(result.returncode, 0, result.stdout)

    def test_feedback_includes_failed_source_and_compiler_error(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Log(Path(directory))
            console = Console(SimpleNamespace(step_limit=100), log)
            try:
                source = "asm1; li r0 6; multiply r0 r0; exit r0; end"
                with self.assertRaises(ValueError):
                    console.run_asm(source)
                with patch.object(console, "chat") as chat:
                    console.command("/feedback")
                    prompt = chat.call_args.args[0]
                    self.assertIn(source, prompt)
                    self.assertIn("compile_error", prompt)
                with self.assertRaises(FileNotFoundError):
                    console.command(f"/load {directory}/missing.asm1")
                self.assertIsNone(console.result)
                console.run_asm("asm1; li r0 84; exit r0; end")
                with self.assertRaises(ValueError):
                    console.command("/assert 43")
                with patch.object(console, "chat") as chat:
                    console.command("/feedback")
                    self.assertIn("required result is 43", chat.call_args.args[0])
                    self.assertIn('"value": 84', chat.call_args.args[0])
            finally:
                console.close()
                log.close(0)

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
