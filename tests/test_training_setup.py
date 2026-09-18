"""Checks for padded vocabularies and the Windows training entry point."""

from dataclasses import dataclass
import io
from contextlib import nullcontext, redirect_stdout
from pathlib import Path
from types import SimpleNamespace
import sys
import unittest
from unittest.mock import Mock, patch


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "training"))

import AIOS_TRAINING_WINDOWS as launcher
from model_checks import check_tokenizer_embeddings
from train_asm1_lora import nonfinite_training_metric, parse_args, warmup_options


class VocabularyChecks(unittest.TestCase):
    def test_qwen_padding_is_allowed_without_changing_vocabulary(self):
        vocab = {f"token-{index}": index for index in range(151665)}
        tokenizer = SimpleNamespace(get_vocab=lambda: vocab)
        check_tokenizer_embeddings(tokenizer, 151936)
        self.assertEqual(len(vocab), 151665)
        self.assertEqual(max(vocab.values()), 151664)

    def test_invalid_ids_are_rejected_even_when_token_count_fits(self):
        for vocab in ({}, {"token": -1}, {"token": 151936}, {"a": 0, "b": 200000}):
            with self.subTest(vocab=vocab), self.assertRaises(ValueError):
                check_tokenizer_embeddings(SimpleNamespace(get_vocab=lambda: vocab), 151936)


class TrainingConfigurationChecks(unittest.TestCase):
    def test_small_presets_keep_runs_separate_and_allow_explicit_overrides(self):
        with patch.object(sys, "argv", ["train", "--preset", "small-smoke"]):
            smoke = parse_args()
        with patch.object(sys, "argv", ["train", "--max-steps", "20", "--preset", "small-test",
                                        "--model", "local-base", "--output-dir", "/tmp/my-small-run"]):
            small = parse_args()
        with patch.object(sys, "argv", ["train"]):
            normal = parse_args()
        self.assertEqual(smoke.model, "Qwen/Qwen2.5-Coder-0.5B-Instruct")
        self.assertEqual(smoke.max_steps, 2)
        self.assertEqual(smoke.attn_implementation, "sdpa")
        self.assertEqual(smoke.dtype, "float16")
        self.assertEqual(smoke.batch_size, 1)
        self.assertEqual(smoke.train_limit, 64)
        self.assertEqual(smoke.eval_limit, 4)
        self.assertEqual(small.max_steps, 20)
        self.assertEqual(small.model, "local-base")
        self.assertEqual(small.output_dir, Path("/tmp/my-small-run"))
        self.assertEqual(small.train_limit, 512)
        self.assertEqual(small.eval_limit, 32)
        self.assertEqual(normal.model, "Qwen/Qwen2.5-Coder-1.5B-Instruct")
        self.assertEqual(normal.max_steps, -1)
        self.assertNotEqual(normal.output_dir, smoke.output_dir)
        self.assertIsNone(normal.train_limit)

    def test_warmup_fraction_is_accepted_by_both_api_families(self):
        @dataclass
        class Legacy:
            warmup_ratio: float = 0

        @dataclass
        class Current:
            warmup_steps: float = 0

        self.assertEqual(Legacy(**warmup_options(Legacy, 0.03)).warmup_ratio, 0.03)
        self.assertEqual(Current(**warmup_options(Current, 0.03)).warmup_steps, 0.03)

    def test_step_limit_is_opt_in(self):
        with patch.object(sys, "argv", ["train"]):
            self.assertEqual(parse_args().max_steps, -1)
        with patch.object(sys, "argv", ["train", "--max-steps", "2"]):
            self.assertEqual(parse_args().max_steps, 2)

    def test_nonfinite_metrics_are_rejected_before_adapter_save(self):
        self.assertIsNone(nonfinite_training_metric([{"loss": 1.2, "eval_loss": 0.9}]))
        metric = nonfinite_training_metric([{"loss": 0.0}, {"eval_loss": float("nan")}])
        self.assertIsNotNone(metric)
        self.assertEqual(metric[0], "eval_loss")


class WindowsLauncherChecks(unittest.TestCase):
    def test_forwarded_arguments_use_same_interpreter_and_bundle_directory(self):
        with patch.object(launcher.subprocess, "Popen") as run:
            child = run.return_value.__enter__.return_value
            child.stdout = iter([])
            child.wait.return_value = 0
            with redirect_stdout(io.StringIO()):
                launcher.run_stage("train", ["--output-dir", "C:/temp/a model"])
        self.assertEqual(run.call_args.args[0], [
            sys.executable, str(ROOT / "training" / "train_asm1_lora.py"),
            "--output-dir", "C:/temp/a model",
        ])
        self.assertEqual(run.call_args.kwargs["cwd"], ROOT)

    def test_child_failure_reaches_shell(self):
        with patch.object(launcher.subprocess, "Popen") as run:
            child = run.return_value.__enter__.return_value
            child.stdout = iter([])
            child.wait.return_value = 7
            with redirect_stdout(io.StringIO()), self.assertRaises(SystemExit) as error:
                launcher.run_stage("train", [])
        self.assertEqual(error.exception.code, 7)

    def test_smoke_does_not_start_after_failed_environment_check(self):
        with patch.object(sys, "argv", ["launcher", "smoke"]):
            with patch.object(launcher, "check_environment", return_value=1):
                with patch.object(launcher, "run_stage") as run, self.assertRaises(SystemExit):
                    launcher.main()
        run.assert_not_called()

    def test_smoke_is_short_and_keeps_normal_training_output_separate(self):
        with patch.object(sys, "argv", ["launcher", "smoke"]):
            with patch.object(launcher, "check_environment", return_value=0):
                with patch.object(launcher, "run_stage") as run:
                    launcher.main()
        stage, arguments = run.call_args.args
        with patch.object(sys, "argv", ["train", *arguments]):
            options = parse_args()
        self.assertEqual(stage, "train")
        self.assertEqual(options.max_steps, 2)
        self.assertEqual(options.gradient_accumulation_steps, 1)
        self.assertNotEqual(options.output_dir, ROOT / "training" / "output" / "asm1-lora")
        self.assertEqual(options.dtype, "float16")
        self.assertEqual(options.attn_implementation, "sdpa")
        self.assertEqual(options.train_limit, 64)
        self.assertEqual(options.eval_limit, 4)
        self.assertEqual(options.warmup_ratio, 0)

    def test_model_check_uses_diagnostic_path_and_forwards_overrides(self):
        with patch.object(sys, "argv", ["launcher", "model-check", "--train-limit", "8"]):
            with patch.object(launcher, "check_environment", return_value=0):
                with patch.object(launcher, "run_stage") as run:
                    launcher.main()
        stage, arguments = run.call_args.args
        with patch.object(sys, "argv", ["train", *arguments]):
            options = parse_args()
        self.assertEqual(stage, "train")
        self.assertTrue(options.diagnose)
        self.assertEqual(options.train_limit, 8)


class GpuDiagnosticChecks(unittest.TestCase):
    def fake_torch(self):
        torch = Mock()
        torch.__version__ = "test"
        torch.version.cuda = "12.6"
        torch.cuda.is_available.return_value = True
        torch.cuda.device_count.return_value = 1
        torch.cuda.get_arch_list.return_value = ["sm_60"]
        torch.cuda.get_device_name.return_value = "Quadro P2000"
        torch.cuda.get_device_capability.return_value = (6, 1)
        torch.cuda.get_device_properties.return_value = SimpleNamespace(total_memory=5 * 2**30)
        torch.cuda.device.return_value = nullcontext()
        return torch

    def test_detected_gpu_with_failed_allocation_cannot_pass_check(self):
        torch = self.fake_torch()
        torch.empty.side_effect = RuntimeError("CUDA-capable devices are busy or unavailable")
        output = io.StringIO()
        with patch.dict(sys.modules, {"torch": torch}), redirect_stdout(output):
            self.assertEqual(launcher.check_gpu(), 1)
        self.assertIn("CUDA available: True", output.getvalue())
        self.assertIn("failed at GPU 0 memory allocation", output.getvalue())
        torch.full.assert_not_called()

    def test_kernel_failure_is_distinguished_from_memory_allocation(self):
        torch = self.fake_torch()
        torch.empty.return_value.fill_.side_effect = RuntimeError("no kernel image available")
        output = io.StringIO()
        with patch.dict(sys.modules, {"torch": torch}), redirect_stdout(output):
            self.assertEqual(launcher.check_gpu(), 1)
        self.assertIn("memory allocation: OK", output.getvalue())
        self.assertIn("failed at GPU 0 FP32 kernel", output.getvalue())
        torch.full.assert_not_called()

    def test_windows_diagnostic_does_not_launch_from_wsl(self):
        with patch.object(sys, "platform", "linux"), patch.object(launcher.subprocess, "run") as run:
            with patch.object(launcher, "check_gpu") as gpu, redirect_stdout(io.StringIO()):
                self.assertEqual(launcher.diagnose_gpu(), 1)
        run.assert_not_called()
        gpu.assert_not_called()

    def test_driver_status_is_printed_without_mutating_commands(self):
        smi = "C:/Windows/System32/nvidia-smi.exe"
        output = io.StringIO()
        status = SimpleNamespace(returncode=0, stdout="Driver Version: 531.18\n", stderr="")
        with patch.object(sys, "platform", "win32"), patch.object(launcher.shutil, "which", return_value=smi):
            with patch.object(launcher.subprocess, "run", return_value=status) as run:
                with patch.object(launcher, "check_gpu", return_value=1), redirect_stdout(output):
                    self.assertEqual(launcher.diagnose_gpu(), 1)
        self.assertIn("Driver Version: 531.18", output.getvalue())
        self.assertEqual(run.call_args.args[0], [smi])
        self.assertNotIn("shell", run.call_args.kwargs)


if __name__ == "__main__":
    unittest.main()
