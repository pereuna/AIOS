#!/usr/bin/env python3
"""Run a recorded training/evaluation job with logs, status and checkpoint resume."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


def now():
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def save(path, data):
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def last_checkpoint(directory):
    candidates = []
    for path in Path(directory).glob("checkpoint-*"):
        try:
            step = int(path.name.removeprefix("checkpoint-"))
        except ValueError:
            continue
        if all((path / name).is_file() for name in (
            "trainer_state.json", "adapter_model.safetensors", "optimizer.pt", "scheduler.pt", "rng_state.pth"
        )):
            candidates.append((step, path))
    return str(max(candidates)[1]) if candidates else None


def lock_job(file):
    file.seek(0)
    if os.name == "nt":
        import msvcrt
        msvcrt.locking(file.fileno(), msvcrt.LK_NBLCK, 1)
    else:
        import fcntl
        fcntl.flock(file, fcntl.LOCK_EX | fcntl.LOCK_NB)


def run(job_path, resume):
    config = json.loads(job_path.read_text(encoding="utf-8"))
    state_path = job_path.with_name("status.json")
    fingerprint = hashlib.sha256(job_path.read_bytes()).hexdigest()
    state = {"status": "running", "job_sha256": fingerprint, "started_at": now(), "completed_stages": []}
    if state_path.exists():
        if not resume:
            raise RuntimeError("job already has status; use --resume after inspecting it")
        state = json.loads(state_path.read_text(encoding="utf-8"))
        if state["job_sha256"] != fingerprint:
            raise RuntimeError("job configuration changed; refusing to resume unrelated work")
        if state["status"] == "complete":
            print("Job already complete", flush=True)
            return
    state.update(status="running", pid=os.getpid(), updated_at=now(), resumed_at=now() if resume else None)
    save(state_path, state)
    environment = os.environ.copy()
    environment.update(config.get("environment", {}))
    try:
        for stage in config["stages"]:
            if stage["name"] in state["completed_stages"]:
                continue
            command = list(stage["command"])
            checkpoint_dir = stage.get("resume_checkpoint_dir")
            if resume and checkpoint_dir:
                checkpoint = last_checkpoint(checkpoint_dir)
                if checkpoint:
                    command.extend(["--resume-from-checkpoint", checkpoint])
            state.update(stage=stage["name"], stage_started_at=now(), command=command, updated_at=now())
            save(state_path, state)
            print(f"\n{now()} Starting {stage['name']}: {subprocess.list2cmdline(command)}", flush=True)
            log_path = job_path.with_name(stage["name"] + ".log")
            with log_path.open("a", encoding="utf-8", buffering=1) as log:
                log.write(f"\n{now()} {subprocess.list2cmdline(command)}\n")
                with subprocess.Popen(command, cwd=stage.get("cwd", config.get("cwd", str(job_path.parent))),
                                      env=environment, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT) as child:
                    state.update(child_pid=child.pid, updated_at=now())
                    save(state_path, state)
                    result = child.wait()
            if result:
                raise RuntimeError(f"stage {stage['name']} exited {result}; see {log_path}")
            state["completed_stages"].append(stage["name"])
            state.update(child_pid=None, updated_at=now())
            save(state_path, state)
        state.update(status="complete", finished_at=now(), updated_at=now())
        save(state_path, state)
        print(f"{now()} Job complete", flush=True)
    except BaseException as exc:
        state.update(status="failed", error=str(exc), updated_at=now())
        save(state_path, state)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("job", type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--detach", action="store_true")
    mode.add_argument("--status", action="store_true")
    parser.add_argument("--resume", action="store_true")
    args = parser.parse_args()
    job = args.job.resolve()
    if args.status:
        print(job.with_name("status.json").read_text(encoding="utf-8"))
        config = json.loads(job.read_text(encoding="utf-8"))
        if config.get("progress_file") and Path(config["progress_file"]).is_file():
            print(Path(config["progress_file"]).read_text(encoding="utf-8"))
        return
    if args.detach:
        flags = {"creationflags": subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP} if os.name == "nt" else {"start_new_session": True}
        with job.with_name("job.log").open("a", encoding="utf-8") as log:
            child = subprocess.Popen([sys.executable, "-u", str(Path(__file__).resolve()), str(job)] + (["--resume"] if args.resume else []),
                                     cwd=job.parent, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT, **flags)
        print(f"Started supervisor PID {child.pid}; status: {job.with_name('status.json')}", flush=True)
        return
    with job.with_name("job.lock").open("a+b") as file:
        if file.tell() == 0:
            file.write(b"\0")
            file.flush()
        try:
            lock_job(file)
        except OSError as exc:
            raise SystemExit("another supervisor holds this job lock") from exc
        run(job, args.resume)


if __name__ == "__main__":
    main()
