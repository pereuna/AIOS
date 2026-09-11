#!/usr/bin/env python3
"""Host throughput using the real MP pool and pthread-backed fake firmware."""
import hashlib
import os
from pathlib import Path
import statistics
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
COMMAND = [str(ROOT / ".build/test-inference"), str(ROOT / "model.bin")]


def run(threads):
    started = time.perf_counter()
    result = subprocess.run(COMMAND, capture_output=True, check=True,
                            env={**os.environ, "SMOL_THREADS": str(threads),
                                 "SMOL_MP_MODE": "pool"})
    seconds = time.perf_counter() - started
    return seconds, hashlib.sha256(result.stdout).digest()


if __name__ == "__main__":
    _, expected = run(1)  # Warm filesystem caches before measuring.
    timings = {1: [], 2: [], 4: []}
    for order in ((1, 2, 4), (2, 4, 1), (4, 1, 2)):
        for threads in order:
            seconds, digest = run(threads)
            if digest != expected:
                raise RuntimeError(f"{threads} workers changed the logits")
            timings[threads].append(seconds)
    baseline = statistics.median(timings[1])
    print("Host benchmark: four full forward passes, median of three runs.")
    print("Includes process startup, model mapping and pool startup/shutdown.")
    print("UEFI firmware overhead is not measured; compare /threads on hardware.")
    for threads, samples in timings.items():
        seconds = statistics.median(samples)
        print(f"{threads} workers: {seconds:.3f} s; {4 / seconds:.2f} token/s; "
              f"{baseline / seconds:.2f}x serial")
