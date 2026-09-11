#!/usr/bin/env python3
"""Pack the model and settings into the self-contained UEFI application."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib


def write_changed(path, data):
    if path.exists() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_bytes(data)
    temp.replace(path)


def build(args):
    model = args.model.read_bytes()
    if not 64 <= args.context <= 8192 or not 1 <= args.tokens <= 8192:
        raise ValueError("context must be 64..8192 and tokens 1..8192")
    if model[:8] != b"SMOLQ4\0\0" or not 256 <= len(model) <= 100_000_000:
        raise ValueError("invalid Q4 model")
    crc = zlib.crc32(model)
    header = struct.pack("<8s6I", b"SMOLRAM\0", 2, len(model), 64,
                         crc, args.context, args.tokens)
    write_changed(args.output, header.ljust(64, b"\0") + model)
    report = {"format": "self-contained x86-64 UEFI application payload",
              "model_bytes": len(model), "model_crc32": f"{crc:08x}",
              "model_sha256": hashlib.sha256(model).hexdigest(),
              "context": args.context, "tokens": args.tokens}
    write_changed(args.output.with_suffix(".json"), (json.dumps(report, indent=2) + "\n").encode())
    print(f"{args.output}: model {len(model):,} bytes; context {args.context}")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--model", type=Path, default=Path("model.bin"))
    p.add_argument("--context", type=int, default=1024)
    p.add_argument("--tokens", type=int, default=128)
    p.add_argument("--output", type=Path, default=Path(".build/payload.bin"))
    build(p.parse_args())
