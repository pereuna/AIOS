#!/usr/bin/env python3
"""Generate predictions on Windows/CUDA, then compile and score them in WSL."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import time

from asm1_runtime import ASM1_OK, Asm1Compiler, execute, extract_program, verify_program
from eval_asm1 import MODEL_ID, SHORT_SYSTEM_PROMPT, load_model_and_tokenizer, metrics, read_jsonl, tokenizer_signature


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    temporary.replace(path)


def read_requests(path):
    rows = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if not rows or any(not {"instance_id", "prompt"} <= row.keys() for row in rows):
        raise ValueError("requests need instance_id and prompt")
    if len({str(row["instance_id"]) for row in rows}) != len(rows):
        raise ValueError("duplicate request IDs")
    return rows


def generate(args):
    import torch
    from transformers import AutoTokenizer

    rows = read_requests(args.prompts)
    started = time.monotonic()
    model, tokenizer = load_model_and_tokenizer(
        args.model, args.base_model, getattr(torch, args.dtype), args.device_map, args.attn_implementation
    )
    signature = tokenizer_signature(tokenizer)
    if signature != tokenizer_signature(AutoTokenizer.from_pretrained(args.base_model, use_fast=True)):
        raise ValueError("model tokenizer differs from the base tokenizer")
    device = model.get_input_embeddings().weight.device
    metadata = {
        "model": args.model, "base_model": args.base_model,
        "prompts_sha256": digest(args.prompts), "tokenizer_sha256": signature,
        "system_prompt": args.system_prompt, "do_sample": False,
        "max_new_tokens": args.max_new_tokens, "batch_size": args.batch_size,
        "dtype": args.dtype, "attn_implementation": args.attn_implementation,
        "expected_examples": len(rows), "completed_examples": 0, "complete": False,
    }
    sidecar = args.output.with_suffix(".meta.json")
    write_json(sidecar, metadata)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", buffering=1) as target:
        for start in range(0, len(rows), args.batch_size):
            batch = rows[start:start + args.batch_size]
            conversations = []
            for row in batch:
                messages = [{"role": "system", "content": args.system_prompt}] if args.system_prompt else []
                conversations.append(messages + [{"role": "user", "content": str(row["prompt"])}])
            inputs = tokenizer.apply_chat_template(conversations, tokenize=True, add_generation_prompt=True,
                                                   padding=True, return_tensors="pt", return_dict=True)
            inputs = {key: value.to(device) for key, value in inputs.items()}
            with torch.inference_mode():
                outputs = model.generate(**inputs, max_new_tokens=args.max_new_tokens, do_sample=False,
                                         pad_token_id=tokenizer.pad_token_id, eos_token_id=tokenizer.eos_token_id)
            responses = tokenizer.batch_decode(outputs[:, inputs["input_ids"].shape[1]:], skip_special_tokens=True)
            for row, response in zip(batch, responses, strict=True):
                target.write(json.dumps({"instance_id": row["instance_id"], "prompt": row["prompt"],
                                         "response": response}, ensure_ascii=False) + "\n")
            metadata.update(completed_examples=start + len(batch), elapsed_seconds=time.monotonic() - started)
            write_json(sidecar, metadata)
            print(f"{args.output.name}: {start + len(batch)}/{len(rows)}", flush=True)
    metadata["complete"] = True
    metadata["predictions_sha256"] = digest(args.output)
    write_json(sidecar, metadata)


def score_response(row, response, compiler):
    source = extract_program(response)
    compile_status = error_line = value = None
    compile_success = execution_success = False
    error = "no_complete_asm_call"
    if source is not None:
        compiled = compiler.compile(source)
        compile_status, error_line = compiled.status, compiled.error_line
        compile_success = compiled.status == ASM1_OK and compiled.complete
        if compile_success:
            executed = execute(source)
            execution_success, value, error = executed.success, executed.value, executed.error
        else:
            error = "compile_error"
    return {
        "instance_id": row["instance_id"], "category": row["category"], "prompt": row["prompt"],
        "expected": int(row["expected"]), "response": response, "program": source,
        "compile_status": compile_status, "compile_error_line": error_line, "compile_success": compile_success,
        "execution_success": execution_success, "execution_error": error, "value": value,
        "correct_result": execution_success and value == int(row["expected"]),
    }


def score_predictions(rows, path, compiler):
    predictions = read_requests(path)
    by_id = {str(row["instance_id"]): row for row in predictions}
    if set(by_id) != {str(row["instance_id"]) for row in rows}:
        raise ValueError("prediction IDs must match the entire evaluation split")
    result = []
    for row in rows:
        prediction = by_id[str(row["instance_id"])]
        if prediction["prompt"] != row["prompt"]:
            raise ValueError(f"prediction prompt differs: {row['instance_id']}")
        result.append(score_response(row, prediction["response"], compiler))
    return result


def score(args):
    rows = read_jsonl(args.eval_file)
    train = read_jsonl(args.train_file)
    eval_ids = [str(row["instance_id"]) for row in rows]
    if len(set(eval_ids)) != len(rows) or set(eval_ids) & {str(row["instance_id"]) for row in train}:
        raise ValueError("duplicate evaluation IDs or train/eval overlap")
    compiler = Asm1Compiler()
    for row in rows:
        source = extract_program(row["completion"])
        if source is None:
            raise ValueError("incomplete reference program")
        verify_program(compiler, source, int(row["expected"]))
    report = {"eval_sha256": digest(args.eval_file), "train_sha256": digest(args.train_file),
              "executor": "AIOS asm1 C compiler and portable reference execution; not a ring-3 runtime test",
              "metrics": {}, "categories": {}, "metadata": {}, "predictions": {}}
    for label, path in (("base", args.base_predictions), ("fine_tuned", args.fine_predictions)):
        meta = json.loads(path.with_suffix(".meta.json").read_text(encoding="utf-8"))
        if not meta["complete"] or meta["prompts_sha256"] != report["eval_sha256"] or meta["predictions_sha256"] != digest(path):
            raise ValueError(f"incomplete or mismatched predictions: {path}")
        results = score_predictions(rows, path, compiler)
        report["predictions"][label] = results
        report["metadata"][label] = meta
        report["metrics"][label] = metrics(results)
        report["categories"][label] = {category: metrics([row for row in results if row["category"] == category])
                                             for category in sorted(Counter(row["category"] for row in rows))}
    for name in ("prompts_sha256", "tokenizer_sha256", "system_prompt", "do_sample", "max_new_tokens", "dtype", "attn_implementation"):
        if report["metadata"]["base"][name] != report["metadata"]["fine_tuned"][name]:
            raise ValueError(f"base/fine-tuned evaluation setting mismatch: {name}")
    report["fine_tuned_minus_base_percentage_points"] = {
        key: report["metrics"]["fine_tuned"][key] - report["metrics"]["base"][key]
        for key in ("compile_success_pct", "execution_success_pct", "correct_result_pct")}
    write_json(args.report, report)
    print(json.dumps(report["metrics"], indent=2), flush=True)
    print(f"Full report: {args.report}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    gpu = commands.add_parser("generate")
    gpu.add_argument("--model", required=True)
    gpu.add_argument("--base-model", default=MODEL_ID)
    gpu.add_argument("--prompts", type=Path, required=True)
    gpu.add_argument("--output", type=Path, required=True)
    gpu.add_argument("--system-prompt", default=SHORT_SYSTEM_PROMPT)
    gpu.add_argument("--dtype", choices=("float16", "bfloat16", "float32"), default="float16")
    gpu.add_argument("--attn-implementation", default="sdpa")
    gpu.add_argument("--device-map", default="cuda:0")
    gpu.add_argument("--batch-size", type=int, default=4)
    gpu.add_argument("--max-new-tokens", type=int, default=256)
    cpu = commands.add_parser("score")
    cpu.add_argument("--eval-file", type=Path, required=True)
    cpu.add_argument("--train-file", type=Path, required=True)
    cpu.add_argument("--base-predictions", type=Path, required=True)
    cpu.add_argument("--fine-predictions", type=Path, required=True)
    cpu.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "generate":
        if args.batch_size < 1 or args.max_new_tokens < 1:
            parser.error("batch size and token limit must be positive")
        generate(args)
    else:
        score(args)


if __name__ == "__main__":
    main()
