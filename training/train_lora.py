#!/usr/bin/env python3
"""Fine-tune Qwen Coder on verified calc completions."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import time

from model_checks import check_tokenizer_embeddings
from training_diagnostics import FiniteLossMixin, diagnose_model


MODEL_ID = "Qwen/Qwen2.5-Coder-1.5B-Instruct"
HERE = Path(__file__).resolve().parent
SHORT_SYSTEM_PROMPT = "You are Qwen, a helpful assistant in AIOS."
SMALL_MODEL_ID = "Qwen/Qwen2.5-Coder-0.5B-Instruct"
SMALL_DEFAULTS = {
    "model": SMALL_MODEL_ID,
    "batch_size": 1,
    "eval_batch_size": 1,
    "lora_r": 8,
    "lora_alpha": 16,
    "dtype": "float16",
    "attn_implementation": "sdpa",
    "warmup_ratio": 0.0,
}
PRESETS = {
    "small-smoke": {
        **SMALL_DEFAULTS,
        "max_steps": 2,
        "gradient_accumulation_steps": 1,
        "max_length": 256,
        "train_limit": 64,
        "eval_limit": 4,
        "logging_steps": 1,
        "output_dir": HERE / "output" / "calc-0.5b-smoke",
    },
    "small-test": {
        **SMALL_DEFAULTS,
        "max_steps": 50,
        "gradient_accumulation_steps": 4,
        "max_length": 512,
        "train_limit": 512,
        "eval_limit": 32,
        "logging_steps": 5,
        "eval_steps": 25,
        "save_steps": 25,
        "output_dir": HERE / "output" / "calc-0.5b-test",
    },
}


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
    parser.add_argument("--preset", choices=tuple(PRESETS),
                        help="Short 0.5B runs for a small GPU; explicit arguments override preset defaults")
    parser.add_argument("--model", default=MODEL_ID, help="Hub ID or an unmodified local base-model directory")
    parser.add_argument("--train-file", type=Path, default=HERE / "data" / "train.jsonl")
    parser.add_argument("--eval-file", type=Path, default=HERE / "data" / "eval.jsonl")
    parser.add_argument("--supplemental-file", type=Path, help="Additional prompt/completion JSONL, kept out of evaluation")
    parser.add_argument("--supplemental-repeats", type=int, default=1, help="Explicit sampling weight for supplemental examples")
    parser.add_argument("--empty-system-prompt-fraction", type=float, default=0.0,
                        help="Train some conversations without a system message, as in ai.py")
    parser.add_argument("--output-dir", type=Path, default=HERE / "output" / "calc-lora")
    parser.add_argument("--epochs", type=float, default=2.0)
    parser.add_argument("--max-steps", type=int, default=-1, help="Positive values override --epochs")
    parser.add_argument("--train-limit", type=int, help="Use a seeded subset before preprocessing (diagnostics only)")
    parser.add_argument("--eval-limit", type=int, help="Use a seeded eval subset (diagnostics only)")
    parser.add_argument("--diagnose", action="store_true", help="Compare base/LoRA forward passes without training or saving")
    parser.add_argument("--learning-rate", type=float, default=2e-4)
    parser.add_argument("--batch-size", type=int, default=4)
    parser.add_argument("--eval-batch-size", type=int, default=4)
    parser.add_argument("--gradient-accumulation-steps", type=int, default=8)
    parser.add_argument("--max-length", type=int, default=512)
    parser.add_argument(
        "--packing-strategy",
        choices=("bfd", "bfd-requeue", "bfd_split", "wrapped"),
        default="bfd-requeue",
        help="bfd requires FlashAttention; bfd_split is an alias for bfd-requeue",
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
    preset_parser = argparse.ArgumentParser(add_help=False)
    preset_parser.add_argument("--preset", choices=tuple(PRESETS))
    preset, _ = preset_parser.parse_known_args()
    if preset.preset:
        parser.set_defaults(**PRESETS[preset.preset])
    return parser.parse_args()


def warmup_options(config_class, ratio: float) -> dict[str, float]:
    # Transformers 5 moved fractional warmup from warmup_ratio to warmup_steps.
    field = "warmup_ratio" if "warmup_ratio" in config_class.__dataclass_fields__ else "warmup_steps"
    return {field: ratio}


def nonfinite_training_metric(history: list[dict[str, object]]) -> tuple[str, object] | None:
    """Return the first non-finite training metric, if the run became invalid."""
    for record in history:
        for key in ("loss", "eval_loss", "grad_norm", "entropy", "eval_entropy"):
            value = record.get(key)
            if isinstance(value, (int, float)) and not math.isfinite(value):
                return key, value
    return None


def cast_trainable_parameters_to_float32(model) -> None:
    """Keep LoRA parameters in FP32 when the frozen base model is loaded in FP16."""
    for parameter in model.parameters():
        if parameter.requires_grad:
            parameter.data = parameter.data.float()


def conversation(row, system_prompt: str, empty_fraction: float = 0.0):
    # Stable across dataset workers, restarts and repeated supplemental rows.
    bucket = int.from_bytes(hashlib.sha256(str(row["prompt"]).encode("utf-8")).digest()[:8], "big") / 2**64
    prompt = []
    if system_prompt and bucket >= empty_fraction:
        prompt.append({"role": "system", "content": system_prompt})
    prompt.append({"role": "user", "content": str(row["prompt"])})
    return {"prompt": prompt, "completion": [{"role": "assistant", "content": str(row["completion"])}]}


def main() -> None:
    args = parse_args()
    try:
        import torch
        from datasets import concatenate_datasets, load_dataset
        from peft import LoraConfig, TaskType
        from transformers import AutoModelForCausalLM, AutoTokenizer, TrainerCallback
        from trl import SFTConfig, SFTTrainer
    except ImportError as exc:
        raise SystemExit(
            f"Missing training dependency: {exc}. Install training/requirements.txt in a GPU environment."
        ) from exc

    for path in (args.train_file, args.eval_file):
        if not path.is_file():
            raise SystemExit(f"dataset not found: {path}; provide verified LLM-to-LLVM-IR prompt/completion JSONL first")
    if args.lora_r < 1 or args.lora_alpha < 1 or args.max_length < 64:
        raise SystemExit("LoRA rank/alpha must be positive and --max-length must be at least 64")
    if not 0 <= args.warmup_ratio < 1:
        raise SystemExit("--warmup-ratio must be at least 0 and less than 1")
    if not 0 <= args.empty_system_prompt_fraction <= 1 or args.supplemental_repeats < 1:
        raise SystemExit("empty system fraction must be in [0, 1] and supplemental repeats must be positive")
    if args.max_steps == 0 or args.max_steps < -1:
        raise SystemExit("--max-steps must be positive or -1")
    if any(limit is not None and limit < 1 for limit in (args.train_limit, args.eval_limit)):
        raise SystemExit("--train-limit and --eval-limit must be positive")
    if args.packing_strategy == "bfd_split":
        args.packing_strategy = "bfd-requeue"
    dataset_workers = args.dataset_num_proc if args.dataset_num_proc > 1 else None

    tokenizer = AutoTokenizer.from_pretrained(args.model, use_fast=True)
    before_signature = tokenizer_signature(tokenizer)
    before_size = len(tokenizer)
    if tokenizer.pad_token_id is None:
        raise SystemExit(
            "The base tokenizer has no pad token. Refusing to add one because this pipeline must not modify the tokenizer."
        )

    if args.dtype == "auto":
        if torch.cuda.is_available() and torch.cuda.is_bf16_supported(including_emulation=False):
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
    check_tokenizer_embeddings(tokenizer, embeddings)
    model.config.use_cache = False
    if hasattr(model, "enable_input_require_grads"):
        model.enable_input_require_grads()

    raw = load_dataset(
        "json",
        data_files={"train": str(args.train_file), "eval": str(args.eval_file)},
    )
    for split, limit in (("train", args.train_limit), ("eval", args.eval_limit)):
        if limit is not None:
            raw[split] = raw[split].shuffle(seed=args.seed).select(range(min(limit, len(raw[split]))))
            print(f"Diagnostic subset: {split}={len(raw[split])} examples", flush=True)

    train_dataset = raw["train"].map(
        conversation,
        fn_kwargs={"system_prompt": args.system_prompt, "empty_fraction": args.empty_system_prompt_fraction},
        remove_columns=raw["train"].column_names,
        num_proc=dataset_workers,
        desc="Formatting training conversations",
    )
    eval_dataset = raw["eval"].map(
        conversation,
        fn_kwargs={"system_prompt": args.system_prompt},
        remove_columns=raw["eval"].column_names,
        num_proc=dataset_workers,
        desc="Formatting evaluation conversations",
    )

    supplemental_count = 0
    if args.supplemental_file:
        supplemental = load_dataset("json", data_files=str(args.supplemental_file), split="train")
        if not {"prompt", "completion"} <= set(supplemental.column_names):
            raise ValueError("supplemental data needs prompt and completion fields")
        eval_prompts = set(raw["eval"]["prompt"])
        if any(prompt in eval_prompts for prompt in supplemental["prompt"]):
            raise ValueError("supplemental data contains an evaluation prompt")
        supplemental_count = len(supplemental)
        supplemental = supplemental.map(
            conversation,
            fn_kwargs={"system_prompt": args.system_prompt, "empty_fraction": args.empty_system_prompt_fraction},
            remove_columns=supplemental.column_names,
            desc="Formatting supplemental conversations",
        )
        train_dataset = concatenate_datasets([train_dataset] + [supplemental] * args.supplemental_repeats).shuffle(seed=args.seed)
    training_conversations = len(train_dataset)

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
        max_steps=args.max_steps,
        learning_rate=args.learning_rate,
        per_device_train_batch_size=args.batch_size,
        per_device_eval_batch_size=args.eval_batch_size,
        gradient_accumulation_steps=args.gradient_accumulation_steps,
        **warmup_options(SFTConfig, args.warmup_ratio),
        lr_scheduler_type="cosine",
        optim="adamw_torch",
        logging_steps=args.logging_steps,
        eval_strategy="steps",
        eval_steps=args.eval_steps,
        save_strategy="steps",
        save_steps=args.save_steps,
        save_total_limit=args.save_total_limit,
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
        dataset_num_proc=dataset_workers,
        report_to="none",
        logging_nan_inf_filter=False,
        seed=args.seed,
        data_seed=args.seed,
    )
    class CheckedSFTTrainer(FiniteLossMixin, SFTTrainer):
        pass

    trainer = CheckedSFTTrainer(
        model=model,
        args=config,
        train_dataset=train_dataset,
        eval_dataset=eval_dataset,
        processing_class=tokenizer,
        peft_config=lora,
    )
    if tokenizer_signature(tokenizer) != before_signature or len(tokenizer) != before_size:
        raise RuntimeError("the tokenizer changed while preparing training; refusing to train")
    cast_trainable_parameters_to_float32(trainer.model)
    trainer.model.print_trainable_parameters()
    if args.diagnose:
        raise SystemExit(diagnose_model(trainer))

    args.output_dir.mkdir(parents=True, exist_ok=True)
    run_config = {
        **{key: str(value) if isinstance(value, Path) else value for key, value in vars(args).items()},
        "raw_train_examples": len(raw["train"]),
        "raw_eval_examples": len(raw["eval"]),
        "supplemental_unique_examples": supplemental_count,
        "training_conversations_including_repeats": training_conversations,
        "supplemental_sha256": hashlib.sha256(args.supplemental_file.read_bytes()).hexdigest() if args.supplemental_file else None,
        "prepared_train_sequences": len(trainer.train_dataset),
        "prepared_eval_sequences": len(trainer.eval_dataset),
        "train_sha256": hashlib.sha256(args.train_file.read_bytes()).hexdigest(),
        "eval_sha256": hashlib.sha256(args.eval_file.read_bytes()).hexdigest(),
    }
    (args.output_dir / "calc_run_config.json").write_text(
        json.dumps(run_config, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(
        f"Training plan: {training_conversations} conversations, {len(trainer.train_dataset)} packed sequences, "
        f"{args.epochs} epochs, max_steps={args.max_steps}", flush=True
    )

    class ProgressCallback(TrainerCallback):
        def __init__(self):
            self.started = time.monotonic()

        def on_step_end(self, training_args, state, control, **kwargs):
            elapsed = time.monotonic() - self.started
            progress = {
                "global_step": state.global_step, "max_steps": state.max_steps,
                "epoch": state.epoch, "elapsed_seconds": elapsed,
                "last_metrics": state.log_history[-1] if state.log_history else {},
                "cuda_peak_allocated_mib": torch.cuda.max_memory_allocated() / 2**20 if torch.cuda.is_available() else 0,
                "cuda_peak_reserved_mib": torch.cuda.max_memory_reserved() / 2**20 if torch.cuda.is_available() else 0,
            }
            temporary = args.output_dir / "progress.json.tmp"
            temporary.write_text(json.dumps(progress, indent=2) + "\n", encoding="utf-8")
            temporary.replace(args.output_dir / "progress.json")

    trainer.add_callback(ProgressCallback())
    train_result = trainer.train(resume_from_checkpoint=args.resume_from_checkpoint)
    eval_result = trainer.evaluate()
    invalid_metric = nonfinite_training_metric(trainer.state.log_history)
    if invalid_metric is not None:
        key, value = invalid_metric
        raise RuntimeError(
            f"training produced a non-finite metric ({key}={value}); refusing to save an invalid LoRA adapter"
        )
    trainer.save_model(str(args.output_dir))
    trainer.save_state()

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
        "max_steps": args.max_steps,
        "train_limit": args.train_limit,
        "eval_limit": args.eval_limit,
        "lora_r": args.lora_r,
        "lora_alpha": args.lora_alpha,
        "lora_dropout": args.lora_dropout,
        "target_modules": sorted(lora.target_modules),
        "dtype": str(dtype).removeprefix("torch."),
        "system_prompt": args.system_prompt,
        "epochs_requested": args.epochs,
        "epochs_completed": trainer.state.epoch,
        "global_step": trainer.state.global_step,
        "train_examples": len(raw["train"]),
        "supplemental_unique_examples": supplemental_count,
        "supplemental_repeats": args.supplemental_repeats,
        "training_conversations_including_repeats": training_conversations,
        "empty_system_prompt_fraction": args.empty_system_prompt_fraction,
        "eval_examples": len(raw["eval"]),
        "training_metrics": train_result.metrics,
        "evaluation_metrics": eval_result,
    }
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "calc_training.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(f"saved calc LoRA adapter to {args.output_dir}")
    print(f"tokenizer unchanged: {before_size} tokens, sha256={before_signature}")


if __name__ == "__main__":
    main()
