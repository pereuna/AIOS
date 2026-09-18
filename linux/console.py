#!/usr/bin/env python3
"""Interactive Linux development console for the AIOS bare-metal model and asm1."""
from __future__ import annotations

import argparse
import codecs
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "training"))
from asm1_runtime import ASM1_OK, ASM1_MAX_SOURCE, Asm1Compiler, execute, extract_program

HELP = """Text                    Chat with the Q4 model; conversation stays in memory.
/asm asm1; ...; end      Compile and execute asm1 with reference semantics.
/asm                    Enter a multiline program, ending with a line 'end'.
/load PATH              Compile and execute an asm1 UTF-8 file.
/last                   Show the first complete asm1 program in the last reply.
/run                    Explicitly execute that program.
/feedback               Send the last asm1 result to the model for discussion/repair.
/assert UINT32          Check the last execution result (useful in scripts).
/reset                  Clear conversation, last reply and tool result.
/stats                  Show model path, context and settings.
/tokens 1..8192          Set answer token limit.
/threads 1..4           Set CPU worker count.
/simd auto|sse2          Select the shared inference kernel.
/help                   Show this help.
/quit                   Exit. Ctrl-C stops the current model and clears its context.

asm1 uses the real C compiler and a bounded source interpreter. Hardware, UEFI,
ring-3 isolation and emitted x86 execution still need QEMU or the target machine.
"""


def visible(text: str) -> str:
    """Model text must not inject terminal control sequences."""
    return "".join(c if c in "\n\t" or ord(c) >= 32 and not 127 <= ord(c) <= 159
                   else f"\\x{ord(c):02x}" for c in text)


class Log:
    def __init__(self, directory: Path):
        directory.mkdir(parents=True, exist_ok=True)
        self.session = uuid.uuid4().hex
        self.text = (directory / "console.log").open("a", encoding="utf-8", buffering=1)
        self.events = (directory / "console.jsonl").open("a", encoding="utf-8", buffering=1)
        self.event("start")
        self.text.write(f"\n=== {datetime.now().astimezone().isoformat()} {self.session} ===\n")

    def event(self, kind: str, **data):
        self.events.write(json.dumps(dict(time=datetime.now(timezone.utc).isoformat(),
                                         session=self.session, event=kind, **data),
                                     ensure_ascii=False) + "\n")

    def write(self, text: str):
        text = visible(text)
        print(text, end="", flush=True)
        self.text.write(text)
        self.text.flush()

    def input(self, text: str):
        self.event("input", text=text)
        self.text.write(f"YOU> {visible(text)}\n")

    def close(self, code: int):
        self.event("exit", code=code)
        self.text.write(f"\n=== exit {code} ===\n")
        self.text.close()
        self.events.close()


class ModelWorker:
    def __init__(self, args):
        self.errors = tempfile.TemporaryFile()
        self.process = None
        try:
            self.process = subprocess.Popen(
                [str(args.worker), str(args.model), str(args.context), str(args.tokens),
                 str(args.threads), args.simd], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=self.errors, start_new_session=True)
            if self.line() != "READY QWENQ4":
                raise RuntimeError("Unexpected model worker greeting")
        except BaseException:
            self.close()
            raise

    def line(self):
        line = self.process.stdout.readline()
        if not line:
            self.process.wait()
            self.errors.seek(0)
            detail = self.errors.read().decode("utf-8", errors="replace").strip()
            raise RuntimeError(detail or f"Model worker exited ({self.process.returncode})")
        return line.decode("ascii").rstrip("\n")

    def send(self, command: bytes):
        self.process.stdin.write(command)
        self.process.stdin.flush()

    def command(self, command: str):
        self.send(command.encode("ascii") + b"\n")
        result = self.line()
        if not result.startswith("STATE "):
            raise RuntimeError(result)
        _, pos, ctx, threads, tokens, simd = result.split()
        return dict(position=int(pos), context=int(ctx), threads=int(threads),
                    tokens=int(tokens), simd=simd)

    def chat(self, prompt, log):
        encoded = prompt.encode("utf-8")
        if not 1 <= len(encoded) <= 4095 or b"\0" in encoded:
            raise ValueError("Question must be 1..4095 UTF-8 bytes without NUL")
        self.send(f"CHAT {len(encoded)}\n".encode("ascii") + encoded)
        decoder = codecs.getincrementaldecoder("utf-8")("replace")
        reply = []
        prompt_tokens = 0
        while True:
            frame = self.line().split()
            if frame[0] == "ERROR":
                raise ValueError(" ".join(frame[1:]))
            if frame[0] == "BEGIN":
                prompt_tokens = int(frame[1])
                if int(frame[2]):
                    log.write("[context full; starting a new conversation]\n")
                log.write(f"[prefill {prompt_tokens} tokens]\n")
            elif frame[0] == "PREFILL":
                done, total = map(int, frame[1:])
                log.event("prefill", done=done, total=total)
                log.write(f"[prefill {done}/{total}]\n")
                if done == total:
                    log.write("AI> ")
            elif frame[0] == "TEXT":
                size = int(frame[1])
                if not 1 <= size <= 16384:
                    raise RuntimeError("Invalid worker text frame")
                chunk = self.process.stdout.read(size)
                if len(chunk) != size:
                    raise RuntimeError("Incomplete model output frame")
                text = decoder.decode(chunk)
                reply.append(text)
                log.write(text)
                log.event("assistant_chunk", text=text)
            elif frame[0] == "DONE":
                tail = decoder.decode(b"", final=True)
                reply.append(tail)
                log.write(tail + "\n")
                _, pos, count, stop, prefill, generation = frame
                stats = dict(position=int(pos), output_tokens=int(count), stop=stop,
                             prompt_tokens=prompt_tokens, prefill_seconds=float(prefill),
                             generation_seconds=float(generation))
                text = "".join(reply)
                log.event("assistant", text=text, **stats)
                log.write(f"[context {pos}; output {count} tokens; prefill {float(prefill):.2f}s; "
                          f"generation {float(generation):.2f}s; {stop}]\n")
                if stop == "invalid_token":
                    raise RuntimeError("Model produced an invalid control token")
                return text
            else:
                raise RuntimeError("Unknown model worker frame")

    def close(self):
        if self.process is not None:
            if self.process.poll() is None:
                self.process.terminate()
                try:
                    self.process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait()
            self.process.stdin.close()
            self.process.stdout.close()
        self.errors.close()


class Console:
    def __init__(self, args, log):
        self.args, self.log = args, log
        self.worker = None
        self.compiler = None
        self.reply = ""
        self.result = None
        self.source = ""
        self.expected = None
        self.failed = False

    def chat(self, prompt):
        if self.args.asm_only:
            raise ValueError("Model disabled by --asm-only")
        if self.worker is None:
            self.log.write(f"Loading {self.args.model} (CPU QWENQ4)...\n")
            self.worker = ModelWorker(self.args)
            self.log.event("model_loaded", path=str(self.args.model),
                           bytes=self.args.model.stat().st_size)
        self.reply = ""  # Never run an older answer after an interrupted request.
        self.reply = self.worker.chat(prompt, self.log)

    def run_asm(self, source):
        self.result = None
        self.source = source
        self.expected = None
        if source.startswith("/asm") and (len(source) == 4 or source[4].isspace()):
            source = source[4:].lstrip()
        if "\0" in source or len(source.encode("utf-8")) >= ASM1_MAX_SOURCE:
            raise ValueError("asm1 source must be at most 4095 UTF-8 bytes without NUL")
        if self.compiler is None:
            self.compiler = Asm1Compiler()
        compiled = self.compiler.compile(source)
        self.log.event("compile", source=source, **compiled._asdict())
        if compiled.status != ASM1_OK or not compiled.complete:
            self.result = dict(success=False, error="compile_error", status=compiled.status,
                               line=compiled.error_line)
            self.log.event("execution", **self.result)
            raise ValueError(f"asm1 compile error: status={compiled.status}, line={compiled.error_line}, "
                             f"complete={compiled.complete}")
        started = time.monotonic()
        result = execute(source, step_limit=self.args.step_limit)
        self.result = result._asdict()
        self.log.event("execution", **self.result, seconds=time.monotonic() - started)
        if not result.success:
            raise ValueError(f"asm1 runtime error: {result.error}; steps={result.steps}")
        self.log.write(f"asm1: ok; value={result.value}; steps={result.steps}; "
                       f"code={compiled.code_size} bytes; reference interpreter\n")

    def command(self, line):
        command, _, arg = line.partition(" ")
        arg = arg.strip()
        if not line:
            return True
        if command == "/quit":
            return False
        if command == "/help":
            self.log.write(HELP)
        elif command == "/asm":
            self.run_asm(arg)
        elif command == "/load":
            self.result = None
            self.expected = None
            if not arg:
                raise ValueError("Use /load PATH")
            with Path(arg).expanduser().open(encoding="utf-8") as file:
                source = file.read(ASM1_MAX_SOURCE)
            self.run_asm(source)
        elif command in {"/last", "/run"}:
            if command == "/run":
                self.result = None
                self.expected = None
            if arg:
                raise ValueError(f"Use {command} without arguments")
            program = extract_program(self.reply)
            if program is None:
                raise ValueError("Last reply contains no complete /asm ... end program")
            if command == "/last":
                self.log.write(f"/asm {program}\n")
            else:
                self.run_asm(program)
        elif command == "/feedback":
            if self.result is None:
                raise ValueError("Run an asm1 program first")
            prompt = ("Program tested:\n" + self.source +
                      "\nTool result (asm1, Linux reference interpreter): " +
                      json.dumps(self.result) +
                      ". Explain the result, or return a corrected /asm program on error.")
            if self.expected is not None:
                prompt += (f" The required result is {self.expected}. If the actual result differs, "
                           "return only a corrected runnable /asm program.")
            self.log.event("feedback", text=prompt)
            self.chat(prompt)
        elif command == "/assert":
            expected = int(arg)
            if not 0 <= expected <= 0xffffffff:
                raise ValueError("Expected value must be uint32")
            self.expected = expected
            passed = bool(self.result and self.result.get("success") and self.result.get("value") == expected)
            self.log.event("assertion", expected=expected, success=passed, actual=self.result)
            if not passed:
                raise ValueError(f"Assertion failed: expected {expected}, got {self.result}")
            self.log.write(f"assert: PASS ({expected})\n")
        elif command == "/reset":
            if self.worker:
                self.worker.command("RESET")
            self.reply, self.result, self.source = "", None, ""
            self.expected = None
            self.log.write("Conversation cleared.\n")
        elif command == "/stats":
            state = (self.worker.command("STATS") if self.worker else
                     dict(position=0, context=self.args.context, threads=self.args.threads,
                          tokens=self.args.tokens, simd=self.args.simd))
            self.log.write(f"Model: {self.args.model}; loaded={self.worker is not None}\n{state}\n")
            self.log.event("stats", **state)
        elif command in {"/tokens", "/threads", "/simd"}:
            key = command[1:]
            if key == "simd":
                if arg not in {"auto", "sse2"}:
                    raise ValueError("Use /simd auto|sse2")
                value = arg
            else:
                value = int(arg)
                if not 1 <= value <= (4 if key == "threads" else 8192):
                    raise ValueError(f"Invalid {key} value")
            if self.worker:
                self.worker.command(f"{key.upper()} {value}")
            setattr(self.args, key, value)
            self.log.write(f"{key}={value}\n")
        elif command.startswith("/"):
            raise ValueError("Unknown command. Use /help. /exec and hardware demos require AIOS/QEMU.")
        else:
            self.chat(line)
        return True

    def close(self):
        if self.worker:
            self.worker.close()
            self.worker = None


def bounded(lo, hi):
    def parse(text):
        n = int(text)
        if not lo <= n <= hi:
            raise argparse.ArgumentTypeError(f"must be {lo}..{hi}")
        return n
    return parse


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    default_model = ROOT / ".build/console-model.bin"
    parser.add_argument("--model", type=Path, default=default_model if default_model.exists() else ROOT / "model.bin")
    parser.add_argument("--worker", type=Path, default=ROOT / ".build/aios-model")
    parser.add_argument("--context", type=bounded(8, 8192), default=2048)
    parser.add_argument("--tokens", type=bounded(1, 8192), default=512)
    parser.add_argument("--threads", type=bounded(1, 4), default=4)
    parser.add_argument("--simd", choices=("auto", "sse2"), default="auto")
    parser.add_argument("--step-limit", type=bounded(1, 10_000_000), default=100_000)
    parser.add_argument("--log-dir", type=Path, default=ROOT / ".build/console/logs")
    parser.add_argument("--script", type=Path, help="Read commands from a UTF-8 file; errors give exit status 1")
    parser.add_argument("--asm-only", action="store_true", help="Test asm1 without loading a model")
    args = parser.parse_args()
    args.model, args.worker = args.model.expanduser().resolve(), args.worker.expanduser().resolve()
    log = Log(args.log_dir)
    console = Console(args, log)
    source = None
    code = 0
    try:
        source = args.script.open(encoding="utf-8") if args.script else sys.stdin
        interactive = not args.script and sys.stdin.isatty()
        log.event("settings", model=str(args.model), context=args.context, tokens=args.tokens,
                  threads=args.threads, simd=args.simd, asm_only=args.asm_only)
        log.write("AIOS Linux console — /help for commands\n")
        log.write(f"Log: {args.log_dir.resolve() / 'console.log'}\n")
        log.write("asm1 execution: reference interpreter; model output runs only with /run.\n")
        while True:
            try:
                if interactive:
                    print("YOU> ", end="", flush=True)
                raw = source.readline()
                if not raw:
                    break
                line = raw.strip()
                if line == "/asm":
                    lines = []
                    while True:
                        if interactive:
                            print("asm> ", end="", flush=True)
                        raw = source.readline()
                        if not raw:
                            raise ValueError("Incomplete multiline asm1 program (missing end)")
                        lines.append(raw.rstrip("\r\n"))
                        if raw.strip() == "end":
                            break
                        if sum(len(part.encode("utf-8")) + 1 for part in lines) >= ASM1_MAX_SOURCE:
                            raise ValueError("Multiline asm1 program too long")
                    line = "/asm " + "\n".join(lines)
                log.input(line)
                if not interactive:
                    print(visible(f"YOU> {line}"), flush=True)
                if not console.command(line):
                    break
            except KeyboardInterrupt:
                console.close()
                console.reply, console.result, console.source = "", None, ""
                console.expected = None
                log.event("interrupted")
                log.write("\nInterrupted; model context cleared.\n")
                if not interactive:
                    code = 130
                    break
            except (OSError, ValueError, RuntimeError) as exc:
                log.event("error", message=str(exc))
                log.write(f"ERROR: {exc}\n")
                console.failed = True
                if isinstance(exc, (OSError, RuntimeError)):
                    console.close()
                    console.reply = ""
        if console.failed and not interactive and code == 0:
            code = 1
    except OSError as exc:
        log.event("error", message=str(exc))
        log.write(f"ERROR: {exc}\n")
        code = 1
    finally:
        console.close()
        if source is not None and source is not sys.stdin:
            source.close()
        log.close(code)
    return code


if __name__ == "__main__":
    sys.exit(main())
