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


def run(simd, threads):
    started = time.perf_counter()
    result = subprocess.run(COMMAND, capture_output=True, check=True,
                            env={**os.environ, "SMOL_THREADS": str(threads),
                                 "SMOL_MP_MODE": "pool", "SMOL_SIMD": simd})
    seconds = time.perf_counter() - started
    if result.stderr.strip() != f"Matvec: {simd.upper()}".encode():
        raise RuntimeError(f"Requested {simd} was not used: {result.stderr!r}")
    return seconds, hashlib.sha256(result.stdout).digest()


if __name__ == "__main__":
    capability = subprocess.check_output([COMMAND[0], "--simd"], text=True).strip()
    kernels = ["sse2", "avx2"] if capability == "AVX2" else ["sse2"]
    _, expected = run("sse2", 1)  # Warm filesystem caches before measuring.
    cases = [(simd, threads) for threads in (1, 2, 4) for simd in kernels]
    timings = {case: [] for case in cases}
    for repeat in range(3):
        shift = repeat * len(kernels)
        for simd, threads in cases[shift:] + cases[:shift]:
            seconds, digest = run(simd, threads)
            if digest != expected:
                raise RuntimeError(f"{simd}, {threads} workers changed the logits")
            timings[simd, threads].append(seconds)
    baseline = statistics.median(timings["sse2", 1])
    print("Host benchmark: four full forward passes, median of three runs.")
    print("Includes process startup, model mapping and pool startup/shutdown.")
    print("UEFI firmware overhead is not measured; compare /threads on hardware.")
    for (simd, threads), samples in timings.items():
        seconds = statistics.median(samples)
        same_count = statistics.median(timings["sse2", threads])
        print(f"{simd.upper()}, {threads} workers: {seconds:.3f} s; {4 / seconds:.2f} token/s; "
              f"{baseline / seconds:.2f}x SSE2 serial; {same_count / seconds:.2f}x SSE2/{threads}",
              flush=True)
