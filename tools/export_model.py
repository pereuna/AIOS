#!/usr/bin/env python3
"""Export the pinned Qwen2.5-Coder-1.5B-Instruct checkpoint to the project's QWENQ4 format."""

import argparse
import json
import math
import struct
from pathlib import Path

try:
    import numpy as np
except ImportError as exc:
    raise SystemExit(
        f"export_model.py requires a working NumPy installation: {exc}. "
        "On Debian/Devuan, install the python3-numpy system package."
    ) from exc


D = 1536
H = 8960
LAYERS = 28
HEADS = 12
KV_HEADS = 2
KV_DIM = 256
VOCAB = 151936
MAX_CONTEXT = 32768
GROUP = 32
TOKEN_BASE = 151643
SPECIALS = 22  # All added tokens, including eight non-special code/tool markers.
EXPECTED_RANGES = 807
EXPECTED_SIZE = 872253632


def fail(message):
    raise ValueError(message)


def load_json(path):
    with path.open(encoding="utf-8") as source:
        return json.load(source)


def check_config(config):
    expected = {
        "model_type": "qwen2",
        "hidden_act": "silu",
        "hidden_size": D,
        "intermediate_size": H,
        "num_hidden_layers": LAYERS,
        "num_attention_heads": HEADS,
        "num_key_value_heads": KV_HEADS,
        "vocab_size": VOCAB,
        "max_position_embeddings": MAX_CONTEXT,
        "bos_token_id": 151643,
        "eos_token_id": 151645,
        "rms_norm_eps": 1e-6,
        "rope_theta": 1000000,
        "tie_word_embeddings": True,
        "use_sliding_window": False,
        "rope_scaling": None,
    }
    for name, value in expected.items():
        if config.get(name) != value:
            fail(f"unsupported config value {name}={config.get(name)!r}")


def byte_alphabet():
    direct = list(range(ord("!"), ord("~") + 1))
    direct += list(range(ord("¡"), ord("¬") + 1))
    direct += list(range(ord("®"), ord("ÿ") + 1))
    codepoints = list(direct)
    extra = 0
    for byte in range(256):
        if byte not in direct:
            direct.append(byte)
            codepoints.append(256 + extra)
            extra += 1
    return {byte: chr(codepoint) for byte, codepoint in zip(direct, codepoints)}


def unicode_ranges():
    # Tokenizer categories are pinned model data, independent of host Python.
    path = Path(__file__).with_name("unicode-15.1.0.txt")
    ranges = []
    previous = -1
    for line in path.read_text(encoding="ascii").splitlines():
        line = line.partition("#")[0].strip()
        if not line:
            continue
        first, last, kind = line.split()
        first, last, kind = int(first, 16), int(last, 16), int(kind)
        if not previous < first <= last <= 0x10FFFF or kind not in (1, 2, 3):
            fail(f"invalid Unicode range: {line}")
        ranges.append((first, last, kind))
        previous = last
    if len(ranges) != EXPECTED_RANGES:
        fail(f"unexpected Unicode range count {len(ranges)}")
    return ranges


def tokenizer_tables(tokenizer):
    model = tokenizer.get("model", {})
    if model.get("type") != "BPE" or model.get("byte_fallback"):
        fail("unsupported tokenizer model")

    vocab = model.get("vocab", {})
    if len(vocab) != TOKEN_BASE or set(vocab.values()) != set(range(TOKEN_BASE)):
        fail("tokenizer vocabulary is not a complete 151643-entry ID map")
    tokens = [f"<[unused_{i}]>" for i in range(VOCAB)]
    for token, token_id in vocab.items():
        tokens[token_id] = token

    added = tokenizer.get("added_tokens", [])
    if len(added) != SPECIALS or [item.get("id") for item in added] != list(
        range(TOKEN_BASE, TOKEN_BASE + SPECIALS)
    ):
        fail("expected special tokens at IDs 151643 through 151664")
    for item in added:
        token_id = item["id"]
        if item.get("special") != (token_id < 151657):
            fail(f"invalid special-token flag {token_id}")
        tokens[token_id] = item["content"]

    encoded_bytes = byte_alphabet()
    decoded_bytes = {character: byte for byte, character in encoded_bytes.items()}
    pieces = []
    for token_id, token in enumerate(tokens):
        if token_id >= TOKEN_BASE:
            piece = token.encode("utf-8")
        else:
            try:
                piece = bytes(decoded_bytes[character] for character in token)
            except KeyError as exc:
                fail(f"token {token_id} contains a non-ByteLevel character {exc.args[0]!r}")
        if not piece:
            fail(f"token {token_id} is empty")
        pieces.append(piece)

    missing = 0xFFFFFFFF
    byte_ids = [vocab.get(encoded_bytes[byte], missing) for byte in range(256)]

    merges = []
    for rank, entry in enumerate(model.get("merges", [])):
        if isinstance(entry, str):
            pair = entry.split(" ", 1)
        else:
            pair = entry
        if len(pair) != 2:
            fail(f"invalid merge at rank {rank}")
        left, right = pair
        try:
            merges.append((vocab[left], vocab[right], vocab[left + right]))
        except KeyError as exc:
            fail(f"merge at rank {rank} references missing token {exc.args[0]!r}")
    if len(merges) != 151387:
        fail(f"unexpected merge count {len(merges)}")

    return byte_ids, pieces, merges, unicode_ranges()


def tensor_names():
    names = {"model.embed_tokens.weight", "model.norm.weight"}
    suffixes = (
        "input_layernorm.weight",
        "post_attention_layernorm.weight",
        "self_attn.q_proj.bias",
        "self_attn.k_proj.bias",
        "self_attn.v_proj.bias",
        "self_attn.q_proj.weight",
        "self_attn.k_proj.weight",
        "self_attn.v_proj.weight",
        "self_attn.o_proj.weight",
        "mlp.gate_proj.weight",
        "mlp.up_proj.weight",
        "mlp.down_proj.weight",
    )
    for layer in range(LAYERS):
        names.update(f"model.layers.{layer}.{suffix}" for suffix in suffixes)
    return names


class SafeTensors:
    def __init__(self, path):
        index = load_json(path) if path.suffix == ".json" else None
        self.metadata = {}
        self.locations = {}
        filenames = sorted(set(index["weight_map"].values())) if index else [path.name]
        for filename in filenames:
            shard = path.parent / filename
            if shard.parent != path.parent or shard.name != filename:
                fail("invalid shard filename")
            with shard.open("rb") as source:
                encoded_length = source.read(8)
                if len(encoded_length) != 8:
                    fail("truncated safetensors header")
                length = struct.unpack("<Q", encoded_length)[0]
                if length > min(shard.stat().st_size - 8, 16 * 1024 * 1024):
                    fail("invalid safetensors header length")
                metadata = json.loads(source.read(length))
            for name, entry in metadata.items():
                if name == "__metadata__":
                    continue
                if name in self.metadata or (index and index["weight_map"].get(name) != filename):
                    fail("inconsistent safetensors index")
                self.metadata[name] = entry
                self.locations[name] = (shard, 8 + length)
        if set(self.metadata) != tensor_names():
            fail("safetensors tensor names do not match Qwen2.5-Coder-1.5B")

    def bf16(self, name, shape):
        metadata = self.metadata[name]
        if metadata.get("dtype") != "BF16" or metadata.get("shape") != list(shape):
            fail(f"unexpected tensor metadata for {name}")
        first, last = metadata["data_offsets"]
        count = math.prod(shape)
        if first < 0 or last - first != count * 2:
            fail(f"invalid tensor offsets for {name}")
        path, data_offset = self.locations[name]
        if data_offset + last > path.stat().st_size:
            fail(f"truncated tensor {name}")
        raw = np.memmap(
            path,
            mode="r",
            dtype="<u2",
            offset=data_offset + first,
            shape=(count,),
        )
        values = (raw.astype("<u4") << np.uint32(16)).view("<f4")
        return values.reshape(shape)


def write_norm(output, tensors, name, width=D):
    values = tensors.bf16(name, (width,))
    output.write(values.astype("<f4", copy=False).tobytes())


def write_q4(output, tensors, name, shape):
    rows, columns = shape
    if columns % GROUP:
        fail(f"{name} columns are not divisible by {GROUP}")
    values = tensors.bf16(name, shape)
    groups = columns // GROUP
    for first in range(0, rows, 256):
        chunk = values[first : first + 256].reshape(-1, groups, GROUP)
        maximum = np.max(np.abs(chunk), axis=2)
        scales = (maximum / np.float32(7.0)).astype(np.float16)
        divisors = scales.astype(np.float32)[..., None]
        normalized = np.divide(
            chunk,
            divisors,
            out=np.zeros_like(chunk),
            where=divisors != 0,
        )
        quantized = np.clip(np.rint(normalized), -7, 7).astype(np.int8)
        shifted = (quantized + 8).astype(np.uint8)
        packed = shifted[:, :, :16] | (shifted[:, :, 16:] << 4)
        encoded = np.empty((chunk.shape[0], groups, 18), dtype=np.uint8)
        encoded[:, :, :2] = scales.astype("<f2", copy=False).view(np.uint8).reshape(
            chunk.shape[0], groups, 2
        )
        encoded[:, :, 2:] = packed
        output.write(encoded.tobytes())


def write_model(output_path, config, tokenizer, tensors):
    byte_ids, pieces, merges, ranges = tokenizer_tables(tokenizer)
    temporary = output_path
    with temporary.open("wb") as output:
        header = struct.pack(
            "<8s14I2f",
            b"QWENQ4\0\0",
            1,
            D,
            H,
            LAYERS,
            HEADS,
            KV_HEADS,
            VOCAB,
            MAX_CONTEXT,
            GROUP,
            config["bos_token_id"],
            config["eos_token_id"],
            SPECIALS,
            len(merges),
            len(ranges),
            config["rms_norm_eps"],
            config["rope_theta"],
        )
        output.write(header)
        output.write(bytes(256 - len(header)))
        output.write(struct.pack("<256I", *byte_ids))
        for piece in pieces:
            output.write(struct.pack("<I", len(piece)))
            output.write(piece)
        for merge in merges:
            output.write(struct.pack("<3I", *merge))
        for item in ranges:
            output.write(struct.pack("<3I", *item))
        output.write(bytes((-output.tell()) % 64))

        write_q4(output, tensors, "model.embed_tokens.weight", (VOCAB, D))
        write_norm(output, tensors, "model.norm.weight")
        for layer in range(LAYERS):
            base = f"model.layers.{layer}."
            write_norm(output, tensors, base + "input_layernorm.weight")
            write_norm(output, tensors, base + "post_attention_layernorm.weight")
            write_norm(output, tensors, base + "self_attn.q_proj.bias", D)
            write_norm(output, tensors, base + "self_attn.k_proj.bias", KV_DIM)
            write_norm(output, tensors, base + "self_attn.v_proj.bias", KV_DIM)
            write_q4(output, tensors, base + "self_attn.q_proj.weight", (D, D))
            write_q4(output, tensors, base + "self_attn.k_proj.weight", (KV_DIM, D))
            write_q4(output, tensors, base + "self_attn.v_proj.weight", (KV_DIM, D))
            write_q4(output, tensors, base + "self_attn.o_proj.weight", (D, D))
            write_q4(output, tensors, base + "mlp.gate_proj.weight", (H, D))
            write_q4(output, tensors, base + "mlp.up_proj.weight", (H, D))
            write_q4(output, tensors, base + "mlp.down_proj.weight", (D, H))


    size = temporary.stat().st_size
    if EXPECTED_SIZE is not None and size != EXPECTED_SIZE:
        fail(f"unexpected output size {size}, expected {EXPECTED_SIZE}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--tokenizer", type=Path, required=True)
    parser.add_argument("--weights", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    config = load_json(args.config)
    check_config(config)
    tokenizer = load_json(args.tokenizer)
    tensors = SafeTensors(args.weights)
    write_model(args.output, config, tokenizer, tensors)
    print(f"{args.output}: wrote {args.output.stat().st_size:,} bytes")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        raise SystemExit(f"export_model.py: {exc}") from exc
