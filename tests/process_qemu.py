"""Boot the real process implementation; timeout/triple-fault is a failure."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--firmware", default=os.environ.get("OVMF_CODE", "/usr/share/OVMF/OVMF_CODE_4M.fd"))
    parser.add_argument("--accel", default="tcg")
    parser.add_argument("--cpus", type=int, default=1)
    parser.add_argument("--image", default=".build/process-test.efi")
    parser.add_argument("--model", type=Path, help="optional model.bin for the live agent test")
    parser.add_argument("--memory", type=int, default=256, help="VM RAM in MiB")
    parser.add_argument("--timeout", type=int, default=60, help="host timeout in seconds")
    args = parser.parse_args()
    if not Path(args.firmware).is_file():
        parser.error("set OVMF_CODE or --firmware to an OVMF code image")
    with tempfile.TemporaryDirectory(prefix="aios-process-") as directory:
        esp = Path(directory) / "esp"
        boot = esp / "EFI" / "BOOT"
        boot.mkdir(parents=True)
        shutil.copyfile(args.image, boot / "BOOTX64.EFI")
        drive = f"format=raw,file=fat:rw:{esp}"
        if args.model:
            # QEMU's directory-backed FAT is only ~516 MB. Use a private FAT32
            # image for the 919 MiB model; no mount or root privileges needed.
            disk = Path(directory) / "esp.img"
            with disk.open("wb") as image:
                image.truncate(2 * 1024**3)
            subprocess.run(["mformat", "-i", str(disk), "-F", "::"], check=True)
            subprocess.run(["mcopy", "-i", str(disk), "-s", str(esp / "EFI"), "::/"], check=True)
            subprocess.run(["mcopy", "-i", str(disk), str(args.model), "::/model.bin"], check=True)
            drive = f"format=raw,snapshot=on,file={disk}"
        variables = Path(directory) / "vars.fd"
        shutil.copyfile(Path(args.firmware).with_name("OVMF_VARS_4M.fd"), variables)
        command = ["qemu-system-x86_64", "-machine", "q35", "-accel", args.accel,
                   "-cpu", "max" if args.accel == "tcg" else "host",
                   "-smp", str(args.cpus), "-m", str(args.memory),
                   "-drive", f"if=pflash,format=raw,readonly=on,file={args.firmware}",
                   "-drive", f"if=pflash,format=raw,file={variables}",
                   "-drive", drive, "-display", "none",
                   "-serial", "none", "-monitor", "none", "-net", "none",
                   "-debugcon", "stdio", "-device", "isa-debug-exit,iobase=0xf4,iosize=4",
                   "-no-reboot"]
        try:
            result = subprocess.run(command, capture_output=True, timeout=args.timeout)
        except subprocess.TimeoutExpired as error:
            print((error.stdout or b"").decode(errors="replace"), end="")
            raise SystemExit(f"FAIL: ring3/firmware test hung ({args.timeout}s timeout)") from error
        print(result.stdout.decode(errors="replace"), end="")
        if result.returncode != 33 or b"PROCESS PASS" not in result.stdout:
            print(result.stderr.decode(errors="replace"))
            raise SystemExit(f"FAIL: QEMU exit {result.returncode}")


if __name__ == "__main__":
    main()
