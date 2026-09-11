#!/usr/bin/env python3
"""Independent NumPy forward pass against the exact Q4 weights used by C."""
import json
import os
from pathlib import Path
import struct
import subprocess

# Small matrix/vector operations do not benefit from dozens of BLAS threads.
os.environ.setdefault("OPENBLAS_NUM_THREADS", "1")
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
D, H, L, HEADS, HEAD_DIM, VOCAB = 2048, 8192, 24, 32, 64, 49152
TOKENS = [1, 9690, 198, 19556]


def verify():
    metadata = json.loads((ROOT / "model.json").read_text())
    raw = np.memmap(ROOT / "model.bin", mode="r", dtype=np.uint8)
    assert raw.size == metadata["bytes"] == 964120960
    assert struct.unpack_from("<11I", raw, 8) == (
        1, D, H, L, HEADS, HEADS, VOCAB, 8192, 32, 1, 2)
    epsilon, theta = struct.unpack_from("<2f", raw, 64)
    assert theta == 130000
    at = metadata["weights_offset"]

    def norm_weights():
        nonlocal at
        values = np.frombuffer(raw, "<f4", D, at)
        at += D * 4
        return values

    def matrix(rows, columns):
        nonlocal at
        size = rows * columns // 32 * 18
        view = raw[at:at + size].reshape(rows, columns // 32, 18)
        at += size
        return view

    def unpack(packed):
        scales = packed[:, :, :2].copy().view("<f2").astype(np.float32)
        nibbles = packed[:, :, 2:]
        values = np.concatenate((nibbles & 15, nibbles >> 4), axis=-1)
        values = values.astype(np.float32) - np.float32(8)
        return (values * scales).reshape(packed.shape[0], -1)

    embed = matrix(VOCAB, D)
    final_norm = norm_weights()
    layers = []
    for _ in range(L):
        layers.append((norm_weights(), norm_weights(),
                       [matrix(D, D) for _ in range(4)] +
                       [matrix(H, D), matrix(H, D), matrix(D, H)]))
    assert at == raw.size

    def norm(x, weights):
        return x / np.sqrt(np.mean(x * x, axis=-1, keepdims=True) + epsilon) * weights

    positions = np.arange(len(TOKENS), dtype=np.float32)[:, None, None]
    powers = np.arange(0, HEAD_DIM, 2, dtype=np.float32) / HEAD_DIM
    angles = positions / np.power(np.float32(theta), powers)
    cosine, sine = np.cos(angles), np.sin(angles)

    def rotate(x):
        first, second = np.split(x, 2, axis=-1)
        return np.concatenate((first * cosine - second * sine,
                               first * sine + second * cosine), axis=-1)

    x = unpack(embed[TOKENS])
    # All four positions at once, with a causal mask: independent of C's KV loop.
    for index, (n1, n2, packed) in enumerate(layers):
        xb = norm(x, n1)
        q, k, v = [(xb @ unpack(w).T).reshape(-1, HEADS, HEAD_DIM)
                   for w in packed[:3]]
        q, k = rotate(q), rotate(k)
        scores = np.einsum("thd,shd->hts", q, k) / np.float32(8)
        scores[:, np.triu_indices(len(TOKENS), 1)[0],
               np.triu_indices(len(TOKENS), 1)[1]] = -np.inf
        attention = np.exp(scores - scores.max(axis=-1, keepdims=True))
        attention /= attention.sum(axis=-1, keepdims=True)
        output = np.einsum("hts,shd->thd", attention, v).reshape(-1, D)
        x = x + output @ unpack(packed[3]).T
        xb = norm(x, n2)
        gate = xb @ unpack(packed[4]).T
        up = xb @ unpack(packed[5]).T
        with np.errstate(over="ignore"):
            hidden = gate / (1 + np.exp(-gate)) * up
        x = x + hidden @ unpack(packed[6]).T
        assert np.isfinite(x).all(), index
    expected = norm(x, final_norm) @ unpack(embed).T
    command = [str(ROOT / ".build/test-inference"), str(ROOT / "model.bin")]
    result = subprocess.run(command, check=True, capture_output=True,
                            env={**os.environ, "SMOL_THREADS": "1", "SMOL_MP_MODE": "pool"})
    for threads, mode in ((2, "pool"), (4, "pool"), (4, "blocking")):
        parallel = subprocess.run(command, check=True, capture_output=True,
                                  env={**os.environ, "SMOL_THREADS": str(threads),
                                       "SMOL_MP_MODE": mode})
        assert parallel.stdout == result.stdout, (threads, mode, "parallel logits changed")
    print("MP / serial: 2- and 4-worker pool and blocking AP dispatch are bit-identical")
    actual = np.frombuffer(result.stdout, "<f4").reshape(len(TOKENS), VOCAB)
    assert np.isfinite(actual).all()
    np.testing.assert_allclose(actual, expected, atol=0.002, rtol=0.0002)
    np.testing.assert_array_equal(actual.argmax(axis=1), expected.argmax(axis=1))
    print(f"C / NumPy: {actual.size} logits; max error "
          f"{np.max(np.abs(actual - expected)):.8f}; greedy IDs {actual.argmax(axis=1).tolist()}")


if __name__ == "__main__":
    verify()
