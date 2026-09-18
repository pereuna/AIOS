"""Exercise launcher logging with a real, tiny Python child process."""

from contextlib import redirect_stderr, redirect_stdout
import io
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "training"))
import AIOS_TRAINING_WINDOWS as launcher


class WindowsLoggingChecks(unittest.TestCase):
    def test_child_streams_utf8_and_exit_code_are_logged_and_runs_append(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            console = io.StringIO()
            with patch.object(launcher, "ROOT", root):
                with patch.dict(launcher.SCRIPTS, {"train": Path(__file__)}):
                    for code in (0, 7, 1):
                        def action():
                            launcher.run_stage("train", ["--log-probe", str(code)])
                        with patch.object(launcher, "main", side_effect=action):
                            with redirect_stdout(console), redirect_stderr(console):
                                if code:
                                    with self.assertRaises(SystemExit) as error:
                                        launcher.logged_main()
                                    self.assertEqual(error.exception.code, code)
                                else:
                                    launcher.logged_main()
            contents = (root / "training/logs/windows.log").read_text(encoding="utf-8")
            self.assertEqual(contents.count("=== Run started:"), 3)
            for fragment in ("stdout: ää 中文", "stderr: warning", "exit=0", "exit=7",
                             "exit=1", "RuntimeError: child failure", "Traceback"):
                self.assertIn(fragment, contents)
                self.assertIn(fragment, console.getvalue())

    def test_parent_traceback_is_recorded_and_streams_are_restored(self):
        with TemporaryDirectory() as directory:
            stdout, stderr = sys.stdout, sys.stderr
            with patch.object(launcher, "ROOT", Path(directory)):
                with patch.object(launcher, "main", side_effect=ValueError("parent failure")):
                    with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
                        with self.assertRaises(ValueError):
                            launcher.logged_main()
            self.assertIs(sys.stdout, stdout)
            self.assertIs(sys.stderr, stderr)
            contents = (Path(directory) / "training/logs/windows.log").read_text(encoding="utf-8")
            self.assertIn("Traceback", contents)
            self.assertIn("ValueError: parent failure", contents)
            self.assertIn("exit=1", contents)

    def test_partial_line_is_flushed_before_log_is_closed(self):
        with TemporaryDirectory() as directory:
            path = Path(directory) / "live.log"
            with path.open("w", encoding="utf-8") as log:
                console = io.StringIO()
                tee = launcher.LogTee(console, log)
                tee.write("still running")
                self.assertEqual(path.read_text(encoding="utf-8"), "still running")
                self.assertEqual(console.getvalue(), "still running")

    def test_legacy_console_keeps_unicode_intact_in_log(self):
        buffer = io.BytesIO()
        console = io.TextIOWrapper(buffer, encoding="ascii")
        log = io.StringIO()
        launcher.LogTee(console, log).write("ää 中文")
        self.assertEqual(log.getvalue(), "ää 中文")
        self.assertEqual(buffer.getvalue(), b"?? ??")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--log-probe":
        print("stdout: ää 中文", flush=True)
        print("stderr: warning", file=sys.stderr, flush=True)
        if sys.argv[2] == "1":
            raise RuntimeError("child failure")
        raise SystemExit(int(sys.argv[2]))
    unittest.main()
