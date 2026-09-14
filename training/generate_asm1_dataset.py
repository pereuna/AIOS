#!/usr/bin/env python3
"""Generate disjoint, compiler-verified natural-language -> asm1 datasets."""

from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import random
import tempfile
from typing import Callable

from asm1_runtime import Asm1Compiler, UINT32_MASK, verify_program


DEFAULT_SEED = 20260914
TRAIN_PHRASES = (
    "Write a complete AIOS asm1 program to {request} Return only one runnable /asm line.",
    "Using AIOS asm1, {request} Give only the executable /asm command.",
    "Produce a one-line asm1 program that will {request} Do not add prose.",
    "Create runnable /asm asm1 code to {request} The response must contain only the program.",
    "Solve this with the asm1 integer language: {request} Output one complete /asm command.",
    "Generate an AIOS assembler example that can {request} Return the single asm1 command only.",
)
EVAL_PHRASES = (
    "Create executable AIOS asm1 for this held-out task: {request} Respond with just the /asm command.",
    "How would a standalone asm1 program {request} Supply only one runnable /asm line.",
)


@dataclass(frozen=True)
class Candidate:
    category: str
    problem_key: str
    request: str
    source: str
    expected: int


def u32(value: int) -> int:
    return value & UINT32_MASK


def program(*instructions: str) -> str:
    return "; ".join(("asm1", *instructions, "end"))


def arithmetic(rng: random.Random) -> Candidate:
    operation = rng.choice(("add", "sub", "mul", "udiv", "and", "or", "xor", "shl", "shr"))
    a = rng.randrange(0, 1 << 32)
    b = rng.randrange(1, 1 << 20) if operation == "udiv" else rng.randrange(0, 32) if operation in {"shl", "shr"} else rng.randrange(0, 1 << 32)
    functions: dict[str, Callable[[int, int], int]] = {
        "add": lambda x, y: u32(x + y),
        "sub": lambda x, y: u32(x - y),
        "mul": lambda x, y: u32(x * y),
        "udiv": lambda x, y: x // y,
        "and": lambda x, y: x & y,
        "or": lambda x, y: x | y,
        "xor": lambda x, y: x ^ y,
        "shl": lambda x, y: u32(x << (y & 31)),
        "shr": lambda x, y: x >> (y & 31),
    }
    descriptions = {
        "add": f"compute {a} plus {b} with 32-bit unsigned wraparound.",
        "sub": f"subtract {b} from {a} with 32-bit unsigned wraparound.",
        "mul": f"multiply {a} by {b} with 32-bit unsigned wraparound.",
        "udiv": f"calculate the unsigned integer quotient {a} divided by {b}.",
        "and": f"return the bitwise AND of {a} and {b}.",
        "or": f"return the bitwise OR of {a} and {b}.",
        "xor": f"return the bitwise XOR of {a} and {b}.",
        "shl": f"shift {a} left by {b} bits in a 32-bit unsigned register.",
        "shr": f"shift {a} right by {b} bits as an unsigned value.",
    }
    if rng.randrange(2):
        source = program(f"li r0 {a}", f"li r1 {b}", f"{operation} r0 r1", "exit r0")
    else:
        source = program(f"input {a} {b}", "ld r0 0", "ld r1 1", f"{operation} r0 r1", "exit r0")
    return Candidate("arithmetic", f"{operation}:{a}:{b}", descriptions[operation], source, functions[operation](a, b))


def conditionals(rng: random.Random) -> Candidate:
    kind = rng.choice(("maximum", "minimum", "equal", "select_equal"))
    a, b = rng.randrange(0, 1 << 32), rng.randrange(0, 1 << 32)
    if rng.randrange(8) == 0:
        b = a
    if kind == "maximum":
        source = program(
            f"li r0 {a}", f"li r1 {b}", "mov r2 r0", "lt r2 r1", "jz r2 l0",
            "mov r0 r1", "label l0", "exit r0",
        )
        request, expected = f"return the larger of unsigned values {a} and {b}.", max(a, b)
        key = f"maximum:{a}:{b}"
    elif kind == "minimum":
        source = program(
            f"li r0 {a}", f"li r1 {b}", "mov r2 r0", "lt r2 r1", "jz r2 l0",
            "mov r1 r0", "label l0", "exit r1",
        )
        request, expected = f"return the smaller of unsigned values {a} and {b}.", min(a, b)
        key = f"minimum:{a}:{b}"
    elif kind == "equal":
        source = program(f"li r0 {a}", f"li r1 {b}", "eq r0 r1", "exit r0")
        request, expected = f"return 1 when {a} equals {b}, otherwise return 0.", int(a == b)
        key = f"equal:{a}:{b}"
    else:
        yes, no = rng.randrange(0, 1 << 32), rng.randrange(0, 1 << 32)
        source = program(
            f"li r0 {a}", f"li r1 {b}", "eq r0 r1", "jz r0 l0", f"li r2 {yes}",
            "exit r2", "label l0", f"li r2 {no}", "exit r2",
        )
        request = f"return {yes} if {a} equals {b}, and return {no} otherwise."
        expected, key = (yes if a == b else no), f"select_equal:{a}:{b}:{yes}:{no}"
    return Candidate("conditionals", key, request, source, expected)


def loops(rng: random.Random) -> Candidate:
    kind = rng.choice(("triangular", "power"))
    if kind == "triangular":
        n = rng.randrange(0, 5001)
        source = program(
            "li r0 0", f"li r1 {n}", "li r2 1", "label l0", "jz r1 l1",
            "add r0 r1", "sub r1 r2", "jmp l0", "label l1", "exit r0",
        )
        request = f"use a loop to sum every integer from 1 through {n} with uint32 wraparound."
        expected, key = u32(n * (n + 1) // 2), f"triangular:{n}"
    else:
        base, exponent = rng.randrange(0, 1 << 16), rng.randrange(0, 33)
        source = program(
            "li r0 1", f"li r1 {base}", f"li r2 {exponent}", "li r3 1", "label l0",
            "jz r2 l1", "mul r0 r1", "sub r2 r3", "jmp l0", "label l1", "exit r0",
        )
        request = f"use a loop to compute {base} to the power {exponent}, wrapping at 32 bits."
        expected, key = pow(base, exponent, 1 << 32), f"power:{base}:{exponent}"
    return Candidate("loops", key, request, source, expected)


def factorial(rng: random.Random) -> Candidate:
    n = rng.randrange(0, 10_001)
    source = program(
        "li r0 1", f"li r1 {n}", "li r2 1", "label l0", "jz r1 l1", "mul r0 r1",
        "sub r1 r2", "jmp l0", "label l1", "exit r0",
    )
    value = 1
    for factor in range(2, n + 1):
        value = u32(value * factor)
        if value == 0:
            break
    return Candidate(
        "factorial", f"factorial:{n}",
        f"compute {n} factorial using an asm1 loop and return the uint32 result.", source, value,
    )


def gcd(rng: random.Random) -> Candidate:
    a, b = rng.randrange(1, 1 << 32), rng.randrange(1, 1 << 32)
    source = program(
        f"input {a} {b}", "ld r0 0", "ld r1 1", "label l0", "jz r1 l1", "mov r2 r0",
        "umod r2 r1", "mov r0 r1", "mov r1 r2", "jmp l0", "label l1", "exit r0",
    )
    import math
    return Candidate("gcd", f"gcd:{a}:{b}", f"find the greatest common divisor of {a} and {b} with Euclid's loop.", source, math.gcd(a, b))


def modulo(rng: random.Random) -> Candidate:
    dividend, divisor = rng.randrange(0, 1 << 32), rng.randrange(1, 1 << 32)
    source = program(f"li r0 {dividend}", f"li r1 {divisor}", "umod r0 r1", "exit r0")
    return Candidate(
        "modulo", f"modulo:{dividend}:{divisor}",
        f"calculate the unsigned remainder when {dividend} is divided by {divisor}.",
        source, dividend % divisor,
    )


def repeated_addition(rng: random.Random) -> Candidate:
    multiplicand, count = rng.randrange(0, 1 << 24), rng.randrange(0, 2001)
    source = program(
        "li r0 0", f"li r1 {multiplicand}", f"li r2 {count}", "li r3 1", "label l0",
        "jz r2 l1", "add r0 r1", "sub r2 r3", "jmp l0", "label l1", "exit r0",
    )
    return Candidate(
        "repeated_addition", f"repeated_addition:{multiplicand}:{count}",
        f"multiply {multiplicand} by {count} through repeated addition in a loop, without mul.",
        source, u32(multiplicand * count),
    )


def input_memory(rng: random.Random) -> Candidate:
    kind = rng.choice(("sum_inputs", "store_expression"))
    if kind == "sum_inputs":
        values = [rng.randrange(0, 1 << 32) for _ in range(rng.randrange(3, 9))]
        instructions = [f"input {' '.join(map(str, values))}", "li r0 0"]
        for index in range(len(values)):
            instructions.extend((f"ld r1 {index}", "add r0 r1"))
        instructions.append("exit r0")
        source = program(*instructions)
        rendered = ", ".join(map(str, values))
        request = f"load the input memory cells containing {rendered} and return their wrapped uint32 sum."
        expected, key = u32(sum(values)), "sum_inputs:" + ":".join(map(str, values))
    else:
        a, b = rng.randrange(0, 1 << 32), rng.randrange(0, 1 << 32)
        cell = rng.randrange(64, 256)
        source = program(
            f"input {a} {b}", "ld r0 0", "ld r1 1", "add r0 r1", f"st {cell} r0",
            "li r0 0", f"ld r0 {cell}", "exit r0",
        )
        request = f"add input values {a} and {b}, store the result in memory cell {cell}, load it back, and return it."
        expected, key = u32(a + b), f"store_expression:{a}:{b}:{cell}"
    return Candidate("input_memory", key, request, source, expected)


def combinations(rng: random.Random) -> Candidate:
    kind = rng.choice(("affine_mod", "gcd_plus", "max_times"))
    if kind == "affine_mod":
        a, b, c = (rng.randrange(0, 1 << 32) for _ in range(3))
        modulus = rng.randrange(1, 1 << 32)
        source = program(
            f"input {a} {b} {c} {modulus}", "ld r0 0", "ld r1 1", "add r0 r1",
            "ld r2 2", "mul r0 r2", "ld r3 3", "umod r0 r3", "exit r0",
        )
        expected = u32(u32(a + b) * c) % modulus
        request = f"evaluate (({a} + {b}) * {c}) modulo {modulus}, applying uint32 wraparound before modulo."
        key = f"affine_mod:{a}:{b}:{c}:{modulus}"
    elif kind == "gcd_plus":
        a, b, c = rng.randrange(1, 1 << 32), rng.randrange(1, 1 << 32), rng.randrange(0, 1 << 32)
        source = program(
            f"input {a} {b} {c}", "ld r0 0", "ld r1 1", "label l0", "jz r1 l1",
            "mov r2 r0", "umod r2 r1", "mov r0 r1", "mov r1 r2", "jmp l0", "label l1",
            "ld r3 2", "add r0 r3", "exit r0",
        )
        import math
        expected = u32(math.gcd(a, b) + c)
        request = f"find gcd({a}, {b}) with a loop, then add {c} using uint32 arithmetic."
        key = f"gcd_plus:{a}:{b}:{c}"
    else:
        a, b, c = (rng.randrange(0, 1 << 32) for _ in range(3))
        source = program(
            f"input {a} {b} {c}", "ld r0 0", "ld r1 1", "mov r2 r0", "lt r2 r1",
            "jz r2 l0", "mov r0 r1", "label l0", "ld r3 2", "mul r0 r3", "exit r0",
        )
        expected = u32(max(a, b) * c)
        request = f"select the larger of {a} and {b}, multiply it by {c}, and return the wrapped uint32 result."
        key = f"max_times:{a}:{b}:{c}"
    return Candidate("combinations", key, request, source, expected)


GENERATORS: dict[str, Callable[[random.Random], Candidate]] = {
    "arithmetic": arithmetic,
    "conditionals": conditionals,
    "loops": loops,
    "factorial": factorial,
    "gcd": gcd,
    "modulo": modulo,
    "repeated_addition": repeated_addition,
    "input_memory": input_memory,
    "combinations": combinations,
}
# A forty-example cycle makes the 20k/1k defaults exact and keeps rare
# factorial examples useful instead of flooding the data with n! == 0 mod 2^32.
CATEGORY_CYCLE = (
    ["arithmetic"] * 12
    + ["conditionals"] * 4
    + ["loops"] * 4
    + ["factorial"]
    + ["gcd"] * 4
    + ["modulo"] * 4
    + ["repeated_addition"] * 4
    + ["input_memory"] * 4
    + ["combinations"] * 3
)


def stable_id(candidate: Candidate) -> str:
    value = f"{candidate.category}\0{candidate.problem_key}".encode("utf-8")
    return hashlib.sha256(value).hexdigest()[:24]


def make_split(
    name: str,
    count: int,
    rng: random.Random,
    compiler: Asm1Compiler,
    used_instances: set[str],
) -> list[dict[str, object]]:
    templates = TRAIN_PHRASES if name == "train" else EVAL_PHRASES
    rows: list[dict[str, object]] = []
    attempts = 0
    while len(rows) < count:
        category = CATEGORY_CYCLE[len(rows) % len(CATEGORY_CYCLE)]
        candidate = GENERATORS[category](rng)
        instance_id = stable_id(candidate)
        attempts += 1
        if instance_id in used_instances:
            if attempts > count * 100 + 1000:
                raise RuntimeError("could not generate enough unique problem instances")
            continue
        verify_program(compiler, candidate.source, candidate.expected)
        used_instances.add(instance_id)
        template_index = rng.randrange(len(templates))
        prompt = templates[template_index].format(request=candidate.request)
        rows.append(
            {
                "prompt": prompt,
                "completion": f"/asm {candidate.source}",
                "category": candidate.category,
                "instance_id": instance_id,
                "template_id": f"{name}-{template_index}",
                "expected": candidate.expected & UINT32_MASK,
            }
        )
    return rows


def write_jsonl(path: Path, rows: list[dict[str, object]]) -> str:
    digest = hashlib.sha256()
    with path.open("wb") as output:
        for row in rows:
            encoded = (json.dumps(row, sort_keys=True, separators=(",", ":")) + "\n").encode("utf-8")
            output.write(encoded)
            digest.update(encoded)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=Path(__file__).with_name("data"))
    parser.add_argument("--train-count", type=int, default=20_000)
    parser.add_argument("--eval-count", type=int, default=1_000)
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    parser.add_argument("--cc", help="C compiler used to build the asm1 binding (default: CC or cc)")
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args()
    if args.train_count < 1 or args.eval_count < 1:
        parser.error("--train-count and --eval-count must both be positive")

    output_dir = args.output_dir.resolve()
    train_path, eval_path = output_dir / "train.jsonl", output_dir / "eval.jsonl"
    if not args.overwrite and (train_path.exists() or eval_path.exists()):
        parser.error(f"dataset already exists in {output_dir}; pass --overwrite to replace it")

    compiler = Asm1Compiler(args.cc)
    rng = random.Random(args.seed)
    used_instances: set[str] = set()
    train_rows = make_split("train", args.train_count, rng, compiler, used_instances)
    train_ids = {str(row["instance_id"]) for row in train_rows}
    eval_rows = make_split("eval", args.eval_count, rng, compiler, used_instances)
    eval_ids = {str(row["instance_id"]) for row in eval_rows}
    if train_ids & eval_ids:
        raise AssertionError("train/eval problem-instance leakage")
    if {str(row["template_id"]) for row in train_rows} & {str(row["template_id"]) for row in eval_rows}:
        raise AssertionError("train/eval phrasing-template leakage")

    output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output_dir) as temporary_directory:
        temporary = Path(temporary_directory)
        train_sha = write_jsonl(temporary / "train.jsonl", train_rows)
        eval_sha = write_jsonl(temporary / "eval.jsonl", eval_rows)
        manifest = {
            "schema_version": 1,
            "seed": args.seed,
            "compiler": "baremetal/asm1.c",
            "train": {
                "file": "train.jsonl",
                "examples": len(train_rows),
                "sha256": train_sha,
                "categories": dict(sorted(Counter(str(row["category"]) for row in train_rows).items())),
            },
            "eval": {
                "file": "eval.jsonl",
                "examples": len(eval_rows),
                "sha256": eval_sha,
                "categories": dict(sorted(Counter(str(row["category"]) for row in eval_rows).items())),
            },
            "shared_instance_ids": 0,
            "shared_template_ids": 0,
        }
        (temporary / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        for name in ("train.jsonl", "eval.jsonl", "manifest.json"):
            (temporary / name).replace(output_dir / name)

    print(f"wrote {len(train_rows):,} verified training examples to {train_path}")
    print(f"wrote {len(eval_rows):,} verified held-out examples to {eval_path}")
    print("train/eval shared problem instances: 0; shared phrasing templates: 0")


if __name__ == "__main__":
    main()
