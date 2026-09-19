#!/usr/bin/env python3
"""Merge an LLM-to-LLVM-IR LoRA adapter into the original Qwen BF16 checkpoint."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys

from model_checks import check_tokenizer_embeddings


MODEL_ID = "Qwen/Qwen2.5-Coder-1.5B-Instruct"
HERE = Path(__file__).resolve().parent
ROOT = HERE.parent


def tokenizer_signature(tokenizer) -> str:
    payload = {
        "vocab": sorted(tokenizer.get_vocab().items()),
        "special_tokens_map": tokenizer.special_tokens_map,
        "added_vocab": sorted(tokenizer.get_added_vocab().items()),
    }
    return hashlib.sha256(
        json.dumps(payload, sort_keys=True, ensure_ascii=False).encode("utf-8")
    ).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--adapter", type=Path, default=HERE / "output" / "calc-lora")
    parser.add_argument("--base-model", help="Override the base recorded in adapter_config.json")
    parser.add_argument("--output-dir", type=Path, default=HERE / "output" / "calc-merged")
    parser.add_argument("--device-map", default="auto", help="Transformers device map; use 'cpu' to force CPU")
    parser.add_argument("--max-shard-size", default="5GB")
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args()

    try:
        import torch
        from peft import PeftConfig, PeftModel
        from transformers import AutoModelForCausalLM, AutoTokenizer
    except ImportError as exc:
        raise SystemExit(
            f"Missing merge dependency: {exc}. Install training/requirements.txt first."
        ) from exc

    adapter = args.adapter.resolve()
    output_dir = args.output_dir.resolve()
    if not (adapter / "adapter_config.json").is_file():
        raise SystemExit(f"PEFT adapter not found: {adapter}")
    if output_dir in (adapter, Path(args.base_model).resolve() if args.base_model else None):
        raise SystemExit("--output-dir must differ from the adapter and base-model directories")
    protected = {Path("/").resolve(), Path.home().resolve(), ROOT.resolve(), HERE.resolve()}
    if output_dir in protected:
        raise SystemExit(f"refusing to use protected directory as --output-dir: {output_dir}")
    if output_dir.exists() and not output_dir.is_dir():
        raise SystemExit(f"output path exists and is not a directory: {output_dir}")
    if output_dir.exists() and any(output_dir.iterdir()) and not args.overwrite:
        raise SystemExit(f"output directory is not empty: {output_dir}; pass --overwrite to replace model files")
    if output_dir.exists() and args.overwrite:
        shutil.rmtree(output_dir)

    peft_config = PeftConfig.from_pretrained(str(adapter))
    base_name = args.base_model or peft_config.base_model_name_or_path or MODEL_ID
    tokenizer = AutoTokenizer.from_pretrained(base_name, use_fast=True)
    original_signature = tokenizer_signature(tokenizer)
    original_size = len(tokenizer)
    if tokenizer.pad_token_id is None:
        raise SystemExit("base tokenizer unexpectedly has no pad token; refusing to modify it")

    device_map = args.device_map
    if device_map == "cpu":
        device_map = {"": "cpu"}
    base = AutoModelForCausalLM.from_pretrained(
        base_name,
        dtype=torch.bfloat16,
        low_cpu_mem_usage=True,
        device_map=device_map,
    )
    original_embeddings = base.get_input_embeddings().num_embeddings
    check_tokenizer_embeddings(tokenizer, original_embeddings)
    model = PeftModel.from_pretrained(base, str(adapter), is_trainable=False)
    merged = model.merge_and_unload(progressbar=True, safe_merge=True)
    if merged.get_input_embeddings().num_embeddings != original_embeddings:
        raise RuntimeError("merging changed the embedding vocabulary size")
    wrong_dtypes = sorted(
        {str(parameter.dtype) for parameter in merged.parameters() if parameter.is_floating_point()}
        - {"torch.bfloat16"}
    )
    if wrong_dtypes:
        raise RuntimeError(f"merged checkpoint contains non-BF16 floating weights: {wrong_dtypes}")
    if tokenizer_signature(tokenizer) != original_signature:
        raise RuntimeError("tokenizer changed while merging")

    output_dir.mkdir(parents=True, exist_ok=True)
    merged.save_pretrained(
        str(output_dir),
        safe_serialization=True,
        max_shard_size=args.max_shard_size,
    )
    tokenizer.save_pretrained(str(output_dir))

    # Validate the result through the same strict readers used by the AIOS
    # Qwen exporter. This checks architecture, tokenizer IDs and BF16 tensor
    # names/dtypes before reporting a successful merge.
    sys.path.insert(0, str(ROOT))
    from tools.export_model import SafeTensors, check_config, load_json, tokenizer_tables

    check_config(load_json(output_dir / "config.json"))
    tokenizer_tables(load_json(output_dir / "tokenizer.json"))
    weights = output_dir / "model.safetensors.index.json"
    if not weights.exists():
        weights = output_dir / "model.safetensors"
    SafeTensors(weights)
    reloaded_tokenizer = AutoTokenizer.from_pretrained(str(output_dir), use_fast=True)
    if tokenizer_signature(reloaded_tokenizer) != original_signature:
        raise RuntimeError("saved tokenizer differs from the original base tokenizer")

    metadata = {
        "base_model": base_name,
        "adapter": str(adapter),
        "dtype": "bfloat16",
        "safe_serialization": True,
        "tokenizer_sha256": original_signature,
        "tokenizer_size": original_size,
        "aios_exporter_validated": True,
        "weights": weights.name,
    }
    (output_dir / "calc_merge.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(f"saved merged BF16 safetensors checkpoint to {output_dir}")
    print(f"AIOS exporter compatibility validated with {weights.name}")
    print(f"tokenizer unchanged: {original_size} tokens, sha256={original_signature}")


if __name__ == "__main__":
    main()
