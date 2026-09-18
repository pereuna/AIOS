#!/usr/bin/env python3
"""Windows entry point for the AIOS asm1 training workflow.

This script never installs packages.  Run ``check`` first, then choose one of
the workflow stages explicitly.
"""

from __future__ import annotations

import argparse
from contextlib import redirect_stderr, redirect_stdout
from datetime import datetime
import importlib
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import traceback


ROOT = Path(__file__).resolve().parent
if (ROOT / "train_asm1_lora.py").is_file():
    ROOT = ROOT.parent
SCRIPTS = {
    "generate": ROOT / "training" / "generate_asm1_dataset.py",
    "train": ROOT / "training" / "train_asm1_lora.py",
    "merge": ROOT / "training" / "merge_asm1_lora.py",
    "eval": ROOT / "training" / "eval_asm1.py",
}
PYTHON_PACKAGES = ("torch", "transformers", "datasets", "trl", "peft", "accelerate", "safetensors", "numpy")
TRAINING_APIS = {
    "transformers": ("AutoModelForCausalLM", "AutoTokenizer"),
    "peft": ("LoraConfig", "TaskType"),
    "trl": ("SFTConfig", "SFTTrainer"),
}


class LogTee:
    """Keep console output visible and flush every write to the shared log."""

    def __init__(self, console, log):
        self.console = console
        self.log = log

    def write(self, text):
        self.log.write(text)
        self.log.flush()
        if self.console is not None:
            try:
                self.console.write(text)
            except UnicodeEncodeError:
                # Preserve Unicode in the log even with a legacy Windows console.
                encoding = getattr(self.console, "encoding", None) or "utf-8"
                self.console.write(text.encode(encoding, errors="replace").decode(encoding))
            self.console.flush()
        return len(text)

    def flush(self):
        self.log.flush()
        if self.console is not None:
            self.console.flush()

    def isatty(self):
        return False


def logged_main() -> None:
    log_path = ROOT / "training" / "logs" / "windows.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("a", encoding="utf-8", buffering=1) as log:
        with redirect_stdout(LogTee(sys.stdout, log)), redirect_stderr(LogTee(sys.stderr, log)):
            started = datetime.now().astimezone().isoformat(timespec="seconds")
            print(f"\n=== Run started: {started}, PID={os.getpid()} ===")
            print(f"Log: {log_path}")
            print("Arguments:", subprocess.list2cmdline(sys.argv[1:]))
            exit_code = 0
            try:
                main()
            except SystemExit as exc:
                exit_code = exc.code if isinstance(exc.code, int) else (0 if exc.code is None else 1)
                if isinstance(exc.code, str):
                    print(f"Exit message: {exc.code}", file=sys.stderr)
                raise
            except BaseException:
                exit_code = 1
                traceback.print_exc()
                raise
            finally:
                ended = datetime.now().astimezone().isoformat(timespec="seconds")
                print(f"=== Run ended: {ended}, PID={os.getpid()}, exit={exit_code} ===", flush=True)


def check_environment() -> int:
    failures = 0
    print(f"Python: {sys.version.replace(chr(10), ' ')}")
    print(f"Executable: {sys.executable}")
    print(f"Bundle: {ROOT}")
    for name in PYTHON_PACKAGES:
        spec = importlib.util.find_spec(name)
        if spec is None:
            print(f"{name}: MISSING")
            failures += 1
            continue
        try:
            module = __import__(name)
            print(f"{name}: {getattr(module, '__version__', '?')}")
        except Exception as exc:
            print(f"{name}: import failed: {type(exc).__name__}: {exc}")
            failures += 1

    # These libraries load their training classes lazily, after the top-level import.
    for name, attributes in TRAINING_APIS.items():
        try:
            module = importlib.import_module(name)
            for attribute in attributes:
                getattr(module, attribute)
            print(f"{name} training API: OK")
        except Exception as exc:
            print(f"{name} training API failed: {type(exc).__name__}: {exc}")
            failures += 1

    failures += check_gpu()
    return int(failures > 0)


def check_gpu() -> int:
    """Exercise CUDA in stages so initialization failures can be distinguished."""
    phase = "import torch"
    try:
        import torch

        print(f"Torch: {torch.__version__}")
        print(f"Torch CUDA build: {torch.version.cuda}")
        phase = "CUDA device detection"
        available = torch.cuda.is_available()
        print(f"CUDA available: {available}")
        if not available:
            return 1
        print(f"Torch compiled GPU architectures: {torch.cuda.get_arch_list()}")
        for index in range(torch.cuda.device_count()):
            phase = f"GPU {index} properties"
            print(
                f"GPU {index}: {torch.cuda.get_device_name(index)} "
                f"compute={torch.cuda.get_device_capability(index)} "
                f"VRAM={torch.cuda.get_device_properties(index).total_memory / 2**30:.1f} GiB"
            )
            phase = f"GPU {index} device selection"
            with torch.cuda.device(index):
                phase = f"GPU {index} memory allocation"
                print(f"Testing {phase}...", flush=True)
                probe = torch.empty(1, device=f"cuda:{index}", dtype=torch.float32)
                torch.cuda.synchronize()
                print(f"{phase}: OK")
                phase = f"GPU {index} FP32 kernel"
                probe.fill_(1)
                if (probe + 1).item() != 2:
                    raise RuntimeError("GPU arithmetic returned an incorrect result")
                print(f"{phase}: OK")
                del probe
                phase = f"GPU {index} FP16 tensor creation"
                value = torch.full((32, 32), 1 / 32, device=f"cuda:{index}",
                                   dtype=torch.float16, requires_grad=True)
                torch.cuda.synchronize()
                phase = f"GPU {index} FP16 matrix multiplication"
                product = value @ value
                torch.cuda.synchronize()
                phase = f"GPU {index} FP16 backward"
                product.float().sum().backward()
                torch.cuda.synchronize()
                phase = f"GPU {index} result verification"
                if not torch.allclose(product, torch.full_like(product, 1 / 32)):
                    raise RuntimeError("GPU matrix multiplication returned an incorrect result")
                if value.grad is None or not torch.allclose(value.grad, torch.full_like(value, 2)):
                    raise RuntimeError("GPU backpropagation returned an incorrect gradient")
                del value, product
            print(f"GPU {index} FP16 forward/backward: OK")
    except Exception as exc:
        print(f"GPU check failed at {phase}: {type(exc).__name__}: {exc}")
        return 1
    return 0


def diagnose_gpu() -> int:
    """Read Windows driver status and run only Torch, without training libraries."""
    if sys.platform != "win32":
        print("Run the gpu diagnostic yourself in Windows PowerShell.")
        return 1
    print(f"Python: {sys.version.replace(chr(10), ' ')}")
    print(f"Executable: {sys.executable}")
    for name in ("CUDA_VISIBLE_DEVICES", "CUDA_LAUNCH_BLOCKING", "CUDA_MODULE_LOADING",
                 "PYTORCH_NVML_BASED_CUDA_CHECK"):
        print(f"{name}: {os.environ.get(name, '(unset)')}")
    smi = shutil.which("nvidia-smi")
    if smi:
        print("NVIDIA driver and GPU status (nvidia-smi):", flush=True)
        try:
            result = subprocess.run([smi], capture_output=True, text=True, errors="replace", timeout=15)
            if result.stdout:
                print(result.stdout.rstrip())
            if result.stderr:
                print(result.stderr.rstrip())
            if result.returncode:
                print(f"nvidia-smi failed with exit code {result.returncode}")
        except (OSError, subprocess.TimeoutExpired) as exc:
            print(f"nvidia-smi failed: {exc}")
    else:
        print("nvidia-smi not found on PATH; driver status could not be read.")
    return check_gpu()


def run_stage(stage: str, extra: list[str]) -> None:
    script = SCRIPTS[stage]
    if not script.is_file():
        raise SystemExit(f"Missing workflow script: {script}")
    # Trainer uses Accelerate internally. One GPU needs no external launch/config.
    command = [sys.executable, str(script), *extra]
    print("Running:", subprocess.list2cmdline(command), flush=True)
    # Pipe both child streams through the parent's tee, including native stderr
    # and tracebacks. Explicit UTF-8 also handles Windows' legacy console encoding.
    env = {**os.environ, "PYTHONIOENCODING": "utf-8", "PYTHONUNBUFFERED": "1"}
    with subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, encoding="utf-8", errors="replace", env=env) as child:
        try:
            for line in child.stdout:
                print(line, end="", flush=True)
            returncode = child.wait()
        except BaseException:
            child.terminate()
            child.wait()
            raise
    if returncode:
        raise SystemExit(returncode)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "stage",
        nargs="?",
        choices=("check", "gpu", "model-check", "smoke", "generate", "train", "merge", "eval"),
        default="check",
    )
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.stage == "check":
        raise SystemExit(check_environment())
    elif args.stage == "gpu":
        raise SystemExit(diagnose_gpu())
    elif args.stage in ("smoke", "model-check"):
        if check_environment():
            raise SystemExit("Environment check failed; training was not started.")
        run_stage("train", [
            "--max-steps", "2", "--batch-size", "1", "--eval-batch-size", "1",
            "--gradient-accumulation-steps", "1", "--max-length", "256",
            "--lora-r", "8", "--lora-alpha", "16", "--dtype", "float16",
            "--attn-implementation", "sdpa", "--logging-steps", "1",
            "--train-limit", "64", "--eval-limit", "4", "--warmup-ratio", "0",
            "--output-dir", str(ROOT / "training" / "output" / "asm1-smoke"),
            *(["--diagnose"] if args.stage == "model-check" else []),
            *args.arguments,
        ])
    else:
        run_stage(args.stage, args.arguments)


if __name__ == "__main__":
    logged_main()
