#!/usr/bin/env python3
"""Compare base and fine-tuned Qwen models on held-out asm1 execution."""

from __future__ import annotations

import argparse
import gc
import hashlib
import json
from pathlib import Path
from typing import Any

from asm1_runtime import ASM1_OK, Asm1Compiler, execute, extract_program, verify_program


MODEL_ID = "Qwen/Qwen2.5-Coder-1.5B-Instruct"
HERE = Path(__file__).resolve().parent
SHORT_SYSTEM_PROMPT = "You are Qwen, a helpful assistant in AIOS."


def tokenizer_signature(tokenizer) -> str:
    payload = {
        "vocab": sorted(tokenizer.get_vocab().items()),
        "special_tokens_map": tokenizer.special_tokens_map,
        "added_vocab": sorted(tokenizer.get_added_vocab().items()),
    }
    return hashlib.sha256(
        json.dumps(payload, sort_keys=True, ensure_ascii=False).encode("utf-8")
    ).hexdigest()


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows = []
    with path.open(encoding="utf-8") as source:
        for line_number, line in enumerate(source, 1):
            try:
                row = json.loads(line)
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{line_number}: {exc}") from exc
            required = {"prompt", "completion", "expected", "instance_id", "category"}
            if not required <= row.keys():
                raise ValueError(f"{path}:{line_number}: missing fields {sorted(required - row.keys())}")
            rows.append(row)
    if not rows:
        raise ValueError(f"empty evaluation dataset: {path}")
    return rows


def load_model_and_tokenizer(model_path: str, base_model: str, dtype, device_map: str):
    from peft import PeftConfig, PeftModel
    from transformers import AutoModelForCausalLM, AutoTokenizer

    path = Path(model_path)
    if device_map == "cpu":
        device_map = {"": "cpu"}
    is_adapter = path.is_dir() and (path / "adapter_config.json").is_file()
    if is_adapter:
        config = PeftConfig.from_pretrained(model_path)
        adapter_base = base_model or config.base_model_name_or_path or MODEL_ID
        tokenizer = AutoTokenizer.from_pretrained(adapter_base, use_fast=True)
        base = AutoModelForCausalLM.from_pretrained(
            adapter_base, dtype=dtype, device_map=device_map, low_cpu_mem_usage=True
        )
        model = PeftModel.from_pretrained(base, model_path, is_trainable=False)
    else:
        tokenizer = AutoTokenizer.from_pretrained(model_path, use_fast=True)
        model = AutoModelForCausalLM.from_pretrained(
            model_path, dtype=dtype, device_map=device_map, low_cpu_mem_usage=True
        )
    if tokenizer.pad_token_id is None:
        raise RuntimeError(f"tokenizer for {model_path} has no pad token; refusing to add one")
    if model.get_input_embeddings().num_embeddings != len(tokenizer):
        raise RuntimeError(f"model/tokenizer vocabulary mismatch for {model_path}")
    tokenizer.padding_side = "left"
    model.eval()
    return model, tokenizer


def metrics(results: list[dict[str, Any]]) -> dict[str, float | int]:
    total = len(results)
    compiled = sum(bool(row["compile_success"]) for row in results)
    executed = sum(bool(row["execution_success"]) for row in results)
    correct = sum(bool(row["correct_result"]) for row in results)
    return {
        "examples": total,
        "compile_success": compiled,
        "compile_success_pct": 100.0 * compiled / total,
        "execution_success": executed,
        "execution_success_pct": 100.0 * executed / total,
        "correct_result": correct,
        "correct_result_pct": 100.0 * correct / total,
    }


def evaluate_model(
    label: str,
    model_path: str,
    base_model: str,
    rows: list[dict[str, Any]],
    compiler: Asm1Compiler,
    dtype,
    device_map: str,
    batch_size: int,
    max_new_tokens: int,
    system_prompt: str,
    expected_tokenizer_signature: str,
) -> tuple[dict[str, float | int], list[dict[str, Any]]]:
    import torch

    print(f"loading {label}: {model_path}")
    model, tokenizer = load_model_and_tokenizer(model_path, base_model, dtype, device_map)
    signature = tokenizer_signature(tokenizer)
    if signature != expected_tokenizer_signature:
        raise RuntimeError(f"{label} tokenizer differs from the base tokenizer")
    input_device = model.get_input_embeddings().weight.device
    results: list[dict[str, Any]] = []
    for start in range(0, len(rows), batch_size):
        batch = rows[start : start + batch_size]
        conversations = []
        for row in batch:
            messages = []
            if system_prompt:
                messages.append({"role": "system", "content": system_prompt})
            messages.append({"role": "user", "content": str(row["prompt"])})
            conversations.append(messages)
        inputs = tokenizer.apply_chat_template(
            conversations,
            tokenize=True,
            add_generation_prompt=True,
            padding=True,
            return_tensors="pt",
            return_dict=True,
        )
        inputs = {name: tensor.to(input_device) for name, tensor in inputs.items()}
        prompt_length = inputs["input_ids"].shape[1]
        with torch.inference_mode():
            generated = model.generate(
                **inputs,
                max_new_tokens=max_new_tokens,
                do_sample=False,
                pad_token_id=tokenizer.pad_token_id,
                eos_token_id=tokenizer.eos_token_id,
            )
        responses = tokenizer.batch_decode(generated[:, prompt_length:], skip_special_tokens=True)
        for row, response in zip(batch, responses):
            source = extract_program(response)
            compile_status = None
            error_line = None
            compile_success = False
            execution_success = False
            value = None
            execution_error = "no_complete_asm_call"
            if source is not None:
                compiled = compiler.compile(source)
                compile_status = compiled.status
                error_line = compiled.error_line
                compile_success = compiled.status == ASM1_OK and compiled.complete
                if compile_success:
                    executed = execute(source)
                    execution_success = executed.success
                    value = executed.value
                    execution_error = executed.error
                else:
                    execution_error = "compile_error"
            expected = int(row["expected"])
            results.append(
                {
                    "instance_id": row["instance_id"],
                    "category": row["category"],
                    "prompt": row["prompt"],
                    "expected": expected,
                    "response": response,
                    "program": source,
                    "compile_status": compile_status,
                    "compile_error_line": error_line,
                    "compile_success": compile_success,
                    "execution_success": execution_success,
                    "execution_error": execution_error,
                    "value": value,
                    "correct_result": execution_success and value == expected,
                }
            )
        done = min(start + len(batch), len(rows))
        print(f"{label}: {done}/{len(rows)}", end="\r", flush=True)
    print()
    summary = metrics(results)
    del model, tokenizer
    gc.collect()
    if torch.cuda.is_available():
        torch.cuda.empty_cache()
    return summary, results


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-model", default=MODEL_ID)
    parser.add_argument("--fine-tuned-model", default=str(HERE / "output" / "asm1-merged"))
    parser.add_argument("--eval-file", type=Path, default=HERE / "data" / "eval.jsonl")
    parser.add_argument("--train-file", type=Path, default=HERE / "data" / "train.jsonl", help="Used only for leakage checking")
    parser.add_argument("--report", type=Path, default=HERE / "output" / "eval_report.json")
    parser.add_argument("--batch-size", type=int, default=4)
    parser.add_argument("--max-new-tokens", type=int, default=256)
    parser.add_argument("--limit", type=int, help="Explicit smoke-test limit; omitted means every held-out prompt")
    parser.add_argument("--device-map", default="auto")
    parser.add_argument("--dtype", choices=("auto", "bfloat16", "float16", "float32"), default="auto")
    parser.add_argument("--system-prompt", default=SHORT_SYSTEM_PROMPT)
    args = parser.parse_args()
    if args.batch_size < 1 or args.max_new_tokens < 1 or (args.limit is not None and args.limit < 1):
        parser.error("batch size, max new tokens and limit must be positive")

    try:
        import torch
        from transformers import AutoTokenizer
    except ImportError as exc:
        raise SystemExit(
            f"Missing evaluation dependency: {exc}. Install training/requirements.txt first."
        ) from exc

    rows = read_jsonl(args.eval_file)
    if len({str(row["instance_id"]) for row in rows}) != len(rows):
        raise SystemExit("held-out dataset contains duplicate problem instances")
    if args.train_file.is_file():
        train_ids = {str(row["instance_id"]) for row in read_jsonl(args.train_file)}
        overlap = train_ids & {str(row["instance_id"]) for row in rows}
        if overlap:
            raise SystemExit(f"train/eval leakage: {len(overlap)} shared problem instances")
    if args.limit is not None:
        rows = rows[: args.limit]

    compiler = Asm1Compiler()
    for row in rows:
        reference = extract_program(str(row["completion"]))
        if reference is None:
            raise SystemExit(f"invalid reference completion for {row['instance_id']}")
        verify_program(compiler, reference, int(row["expected"]))

    base_tokenizer = AutoTokenizer.from_pretrained(args.base_model, use_fast=True)
    base_signature = tokenizer_signature(base_tokenizer)
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

    summaries: dict[str, dict[str, float | int]] = {}
    predictions: dict[str, list[dict[str, Any]]] = {}
    for label, model_path in (("base", args.base_model), ("fine_tuned", args.fine_tuned_model)):
        summaries[label], predictions[label] = evaluate_model(
            label=label,
            model_path=model_path,
            base_model=args.base_model,
            rows=rows,
            compiler=compiler,
            dtype=dtype,
            device_map=args.device_map,
            batch_size=args.batch_size,
            max_new_tokens=args.max_new_tokens,
            system_prompt=args.system_prompt,
            expected_tokenizer_signature=base_signature,
        )

    delta = {
        name: float(summaries["fine_tuned"][name]) - float(summaries["base"][name])
        for name in ("compile_success_pct", "execution_success_pct", "correct_result_pct")
    }
    report = {
        "eval_file": str(args.eval_file.resolve()),
        "base_model": args.base_model,
        "fine_tuned_model": args.fine_tuned_model,
        "system_prompt": args.system_prompt,
        "executor": "AIOS baremetal/asm1.c compiler plus portable asm1 reference execution",
        "metrics": summaries,
        "fine_tuned_minus_base_percentage_points": delta,
        "predictions": predictions,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print("model       compile       execute       correct")
    for label in ("base", "fine_tuned"):
        item = summaries[label]
        print(
            f"{label:11} {item['compile_success_pct']:7.2f}% "
            f"{item['execution_success_pct']:11.2f}% {item['correct_result_pct']:11.2f}%"
        )
    print(
        "delta (pp)  "
        f"{delta['compile_success_pct']:+7.2f}  "
        f"{delta['execution_success_pct']:+10.2f}  "
        f"{delta['correct_result_pct']:+10.2f}"
    )
    print(f"full report: {args.report}")


if __name__ == "__main__":
    main()
