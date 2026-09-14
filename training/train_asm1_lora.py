#!/usr/bin/env python3
"""Fine-tune Qwen2.5-Coder-1.5B-Instruct on verified asm1 completions."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


MODEL_ID = "Qwen/Qwen2.5-Coder-1.5B-Instruct"
HERE = Path(__file__).resolve().parent
SHORT_SYSTEM_PROMPT = "You are Qwen, a helpful assistant in AIOS."


def tokenizer_signature(tokenizer) -> str:
    payload = {
        "vocab": sorted(tokenizer.get_vocab().items()),
        "special_tokens_map": tokenizer.special_tokens_map,
        "added_vocab": sorted(tokenizer.get_added_vocab().items()),
    }
    encoded = json.dumps(payload, sort_keys=True, ensure_ascii=False).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", default=MODEL_ID, help="Hub ID or an unmodified local base-model directory")
    parser.add_argument("--train-file", type=Path, default=HERE / "data" / "train.jsonl")
    parser.add_argument("--eval-file", type=Path, default=HERE / "data" / "eval.jsonl")
    parser.add_argument("--output-dir", type=Path, default=HERE / "output" / "asm1-lora")
    parser.add_argument("--epochs", type=float, default=2.0)
    parser.add_argument("--learning-rate", type=float, default=2e-4)
    parser.add_argument("--batch-size", type=int, default=4)
    parser.add_argument("--eval-batch-size", type=int, default=4)
    parser.add_argument("--gradient-accumulation-steps", type=int, default=8)
    parser.add_argument("--max-length", type=int, default=512)
    parser.add_argument(
        "--packing-strategy",
        choices=("bfd", "bfd_split", "wrapped"),
        default="bfd_split",
        help="bfd requires FlashAttention; bfd_split keeps the default portable",
    )
    parser.add_argument("--lora-r", type=int, default=32)
    parser.add_argument("--lora-alpha", type=int, default=64)
    parser.add_argument("--lora-dropout", type=float, default=0.05)
    parser.add_argument("--warmup-ratio", type=float, default=0.03)
    parser.add_argument("--logging-steps", type=int, default=10)
    parser.add_argument("--eval-steps", type=int, default=100)
    parser.add_argument("--save-steps", type=int, default=100)
    parser.add_argument("--save-total-limit", type=int, default=3)
    parser.add_argument("--dataset-num-proc", type=int, default=1)
    parser.add_argument("--seed", type=int, default=20260914)
    parser.add_argument("--dtype", choices=("auto", "bfloat16", "float16", "float32"), default="auto")
    parser.add_argument("--attn-implementation", choices=("eager", "sdpa", "flash_attention_2"))
    parser.add_argument("--system-prompt", default=SHORT_SYSTEM_PROMPT)
    parser.add_argument("--resume-from-checkpoint", nargs="?", const=True)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    try:
        import torch
        from datasets import load_dataset
        from peft import LoraConfig, TaskType
        from transformers import AutoModelForCausalLM, AutoTokenizer
        from trl import SFTConfig, SFTTrainer
    except ImportError as exc:
        raise SystemExit(
            f"Missing training dependency: {exc}. Install training/requirements.txt in a GPU environment."
        ) from exc

    for path in (args.train_file, args.eval_file):
        if not path.is_file():
            raise SystemExit(f"dataset not found: {path}; run generate_asm1_dataset.py first")
    if args.lora_r < 1 or args.lora_alpha < 1 or args.max_length < 64:
        raise SystemExit("LoRA rank/alpha must be positive and --max-length must be at least 64")

    tokenizer = AutoTokenizer.from_pretrained(args.model, use_fast=True)
    before_signature = tokenizer_signature(tokenizer)
    before_size = len(tokenizer)
    if tokenizer.pad_token_id is None:
        raise SystemExit(
            "The base tokenizer has no pad token. Refusing to add one because this pipeline must not modify the tokenizer."
        )

    if args.dtype == "auto":
        if torch.cuda.is_available() and torch.cuda.is_bf16_supported():
            dtype = torch.bfloat16
        elif torch.cuda.is_available():
            dtype = torch.float16
        else:
            dtype = torch.float32
    else:
        dtype = {
            "bfloat16": torch.bfloat16,
            "float16": torch.float16,
            "float32": torch.float32,
        }[args.dtype]

    model_kwargs = {"dtype": dtype}
    if args.attn_implementation:
        model_kwargs["attn_implementation"] = args.attn_implementation
    model = AutoModelForCausalLM.from_pretrained(args.model, **model_kwargs)
    embeddings = model.get_input_embeddings().num_embeddings
    if embeddings != before_size:
        raise SystemExit(f"tokenizer/model vocabulary mismatch: tokenizer={before_size}, embeddings={embeddings}")
    model.config.use_cache = False
    if hasattr(model, "enable_input_require_grads"):
        model.enable_input_require_grads()

    raw = load_dataset(
        "json",
        data_files={"train": str(args.train_file), "eval": str(args.eval_file)},
    )

    def as_conversation(row: dict[str, object]) -> dict[str, list[dict[str, str]]]:
        prompt = []
        if args.system_prompt:
            prompt.append({"role": "system", "content": args.system_prompt})
        prompt.append({"role": "user", "content": str(row["prompt"])})
        return {
            "prompt": prompt,
            "completion": [{"role": "assistant", "content": str(row["completion"])}],
        }

    train_dataset = raw["train"].map(
        as_conversation,
        remove_columns=raw["train"].column_names,
        num_proc=args.dataset_num_proc,
        desc="Formatting training conversations",
    )
    eval_dataset = raw["eval"].map(
        as_conversation,
        remove_columns=raw["eval"].column_names,
        num_proc=args.dataset_num_proc,
        desc="Formatting evaluation conversations",
    )

    lora = LoraConfig(
        task_type=TaskType.CAUSAL_LM,
        r=args.lora_r,
        lora_alpha=args.lora_alpha,
        lora_dropout=args.lora_dropout,
        bias="none",
        target_modules=[
            "q_proj",
            "k_proj",
            "v_proj",
            "o_proj",
            "gate_proj",
            "up_proj",
            "down_proj",
        ],
    )
    config = SFTConfig(
        output_dir=str(args.output_dir),
        num_train_epochs=args.epochs,
        learning_rate=args.learning_rate,
        per_device_train_batch_size=args.batch_size,
        per_device_eval_batch_size=args.eval_batch_size,
        gradient_accumulation_steps=args.gradient_accumulation_steps,
        warmup_ratio=args.warmup_ratio,
        lr_scheduler_type="cosine",
        logging_steps=args.logging_steps,
        eval_strategy="steps",
        eval_steps=args.eval_steps,
        save_strategy="steps",
        save_steps=args.save_steps,
        save_total_limit=args.save_total_limit,
        save_safetensors=True,
        bf16=dtype == torch.bfloat16,
        fp16=dtype == torch.float16,
        tf32=bool(torch.cuda.is_available() and torch.cuda.get_device_capability()[0] >= 8),
        max_length=args.max_length,
        packing=True,
        packing_strategy=args.packing_strategy,
        eval_packing=False,
        completion_only_loss=True,
        eos_token="<|im_end|>",
        gradient_checkpointing=True,
        gradient_checkpointing_kwargs={"use_reentrant": False},
        dataset_num_proc=args.dataset_num_proc,
        report_to="none",
        seed=args.seed,
        data_seed=args.seed,
    )
    trainer = SFTTrainer(
        model=model,
        args=config,
        train_dataset=train_dataset,
        eval_dataset=eval_dataset,
        processing_class=tokenizer,
        peft_config=lora,
    )
    if tokenizer_signature(tokenizer) != before_signature or len(tokenizer) != before_size:
        raise RuntimeError("the tokenizer changed while preparing training; refusing to train")
    trainer.model.print_trainable_parameters()
    trainer.train(resume_from_checkpoint=args.resume_from_checkpoint)
    trainer.save_model(str(args.output_dir))

    metadata = {
        "base_model": args.model,
        "train_file": str(args.train_file.resolve()),
        "eval_file": str(args.eval_file.resolve()),
        "tokenizer_sha256": before_signature,
        "tokenizer_size": before_size,
        "completion_only_loss": True,
        "packing": True,
        "packing_strategy": args.packing_strategy,
        "max_length": args.max_length,
        "lora_r": args.lora_r,
        "lora_alpha": args.lora_alpha,
        "lora_dropout": args.lora_dropout,
        "target_modules": sorted(lora.target_modules),
        "dtype": str(dtype).removeprefix("torch."),
        "system_prompt": args.system_prompt,
    }
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "asm1_training.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(f"saved asm1 LoRA adapter to {args.output_dir}")
    print(f"tokenizer unchanged: {before_size} tokens, sha256={before_signature}")


if __name__ == "__main__":
    main()
