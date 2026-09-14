#!/usr/bin/env python3
"""Use AIOS's asm1 compiler and execute asm1 source with reference semantics.

The compiler binding deliberately builds ``baremetal/asm1.c`` instead of
reimplementing its acceptance rules.  The small interpreter is used for
portable dataset verification and model evaluation; executing the generated
x86 directly is only safe inside AIOS's ring-3 process environment.
"""

from __future__ import annotations

import ctypes
import hashlib
import os
from pathlib import Path
import re
import subprocess
import tempfile
from typing import NamedTuple


ROOT = Path(__file__).resolve().parents[1]
UINT32_MASK = (1 << 32) - 1
ASM1_OK = 0
ASM1_MAX_SOURCE = 4096
ASM1_MAX_INPUT = 64
ASM1_MAX_CODE = 4096
DEFAULT_STEP_LIMIT = 100_000


class CompileResult(NamedTuple):
    status: int
    error_line: int
    complete: bool
    code_size: int
    input_count: int


class ExecutionResult(NamedTuple):
    success: bool
    value: int | None
    steps: int
    error: str | None


class _Asm1Program(ctypes.Structure):
    _fields_ = [
        ("code", ctypes.c_ubyte * ASM1_MAX_CODE),
        ("code_size", ctypes.c_size_t),
        ("input", ctypes.c_uint32 * ASM1_MAX_INPUT),
        ("input_count", ctypes.c_size_t),
        ("error", ctypes.c_int),
        ("error_line", ctypes.c_uint),
        ("complete", ctypes.c_int),
    ]


class Asm1Compiler:
    """ctypes binding to the repository's actual ``asm1_compile`` function."""

    def __init__(self, cc: str | None = None) -> None:
        source = ROOT / "baremetal" / "asm1.c"
        headers = [
            ROOT / "baremetal" / "asm1.h",
            ROOT / "baremetal" / "process.h",
            ROOT / "baremetal" / "runtime.h",
        ]
        digest = hashlib.sha256()
        for path in [source, *headers]:
            digest.update(path.read_bytes())
        key = digest.hexdigest()[:16]
        library = Path(tempfile.gettempdir()) / f"aios-asm1-{key}.so"
        if not library.exists():
            temporary = library.with_suffix(f".{os.getpid()}.tmp.so")
            command = [
                cc or os.environ.get("CC", "cc"),
                "-O2",
                "-std=c11",
                "-shared",
                "-fPIC",
                "-I",
                str(ROOT / "baremetal"),
                str(source),
                "-o",
                str(temporary),
            ]
            try:
                subprocess.run(command, check=True, capture_output=True, text=True)
                temporary.replace(library)
            except (OSError, subprocess.CalledProcessError) as exc:
                temporary.unlink(missing_ok=True)
                detail = getattr(exc, "stderr", None) or str(exc)
                raise RuntimeError(f"could not build the AIOS asm1 compiler: {detail}") from exc
        self._library = ctypes.CDLL(str(library))
        self._compile = self._library.asm1_compile
        self._compile.argtypes = [ctypes.c_char_p, ctypes.POINTER(_Asm1Program)]
        self._compile.restype = ctypes.c_int

    def compile(self, source: str) -> CompileResult:
        encoded = source.encode("utf-8")
        program = _Asm1Program()
        status = int(self._compile(encoded, ctypes.byref(program)))
        return CompileResult(
            status=status,
            error_line=int(program.error_line),
            complete=bool(program.complete),
            code_size=int(program.code_size),
            input_count=int(program.input_count),
        )


def _source_statements(source: str) -> list[list[str]]:
    if source.startswith("/asm") and (len(source) == 4 or source[4].isspace()):
        source = source[4:].lstrip()
    statements: list[list[str]] = []
    for raw in re.split(r"[;\n]", source):
        raw = raw.partition("#")[0].strip()
        if not raw:
            continue
        words = raw.split()
        if words[0] == "asm1":
            if len(words) != 1:
                raise ValueError("invalid asm1 directive")
            continue
        if words[0] == "end":
            if len(words) != 1:
                raise ValueError("invalid end directive")
            break
        statements.append(words)
    return statements


def execute(source: str, step_limit: int = DEFAULT_STEP_LIMIT) -> ExecutionResult:
    """Execute compiler-accepted asm1 using the documented uint32 semantics."""

    try:
        statements = _source_statements(source)
        registers = [0] * 10
        memory = [0] * 256
        labels: dict[str, int] = {}
        inputs: list[int] = []
        for pc, words in enumerate(statements):
            if words[0] == "label":
                labels[words[1]] = pc
            elif words[0] == "input":
                inputs.extend(int(value) for value in words[1:])
        if len(inputs) > ASM1_MAX_INPUT:
            raise ValueError("too many inputs")
        for index, value in enumerate(inputs):
            memory[index] = value & UINT32_MASK
        registers[0] = len(inputs)

        def reg(name: str) -> int:
            if len(name) != 2 or name[0] != "r" or name[1] not in "0123456789":
                raise ValueError(f"invalid register {name}")
            return ord(name[1]) - ord("0")

        pc = 0
        steps = 0
        while pc < len(statements):
            if steps >= step_limit:
                return ExecutionResult(False, None, steps, "step_limit")
            steps += 1
            words = statements[pc]
            op = words[0]
            pc += 1
            if op in {"input", "label"}:
                continue
            if op == "li":
                registers[reg(words[1])] = int(words[2]) & UINT32_MASK
            elif op == "mov":
                registers[reg(words[1])] = registers[reg(words[2])]
            elif op in {"add", "sub", "mul", "udiv", "umod", "and", "or", "xor", "shl", "shr", "eq", "lt"}:
                destination, source_register = reg(words[1]), reg(words[2])
                left, right = registers[destination], registers[source_register]
                if op == "add":
                    value = left + right
                elif op == "sub":
                    value = left - right
                elif op == "mul":
                    value = left * right
                elif op == "udiv":
                    if right == 0:
                        return ExecutionResult(False, None, steps, "division_by_zero")
                    value = left // right
                elif op == "umod":
                    if right == 0:
                        return ExecutionResult(False, None, steps, "division_by_zero")
                    value = left % right
                elif op == "and":
                    value = left & right
                elif op == "or":
                    value = left | right
                elif op == "xor":
                    value = left ^ right
                elif op == "shl":
                    value = left << (right & 31)
                elif op == "shr":
                    value = left >> (right & 31)
                elif op == "eq":
                    value = int(left == right)
                else:
                    value = int(left < right)
                registers[destination] = value & UINT32_MASK
            elif op == "ld":
                registers[reg(words[1])] = memory[int(words[2])]
            elif op == "st":
                memory[int(words[1])] = registers[reg(words[2])]
            elif op == "jmp":
                pc = labels[words[1]]
            elif op == "jz":
                if registers[reg(words[1])] == 0:
                    pc = labels[words[2]]
            elif op == "exit":
                return ExecutionResult(True, registers[reg(words[1])], steps, None)
            else:
                raise ValueError(f"unsupported instruction {op}")
        return ExecutionResult(False, None, steps, "no_exit")
    except (IndexError, KeyError, ValueError) as exc:
        return ExecutionResult(False, None, 0, f"invalid_source: {exc}")


def extract_program(text: str) -> str | None:
    """Extract the first complete ``/asm ... end`` call from model output."""

    match = re.search(r"(?<!\S)/asm(?:\s+|$)", text)
    if not match:
        return None
    tail = text[match.end() :]
    end = re.search(r"(?:^|;)\s*end\s*(?:;|$)", tail, flags=re.MULTILINE)
    if not end:
        return None
    program = tail[: end.end()].strip()
    program = re.sub(r"\s*```.*$", "", program, flags=re.DOTALL).strip()
    return program


def verify_program(compiler: Asm1Compiler, source: str, expected: int) -> None:
    compiled = compiler.compile(source)
    if compiled.status != ASM1_OK or not compiled.complete:
        raise ValueError(
            f"AIOS asm1 compiler rejected generated source: status={compiled.status}, "
            f"line={compiled.error_line}, complete={compiled.complete}"
        )
    executed = execute(source)
    if not executed.success:
        raise ValueError(f"generated source did not execute: {executed.error}")
    if executed.value != (expected & UINT32_MASK):
        raise ValueError(
            f"generated source returned {executed.value}, expected {expected & UINT32_MASK}"
        )
