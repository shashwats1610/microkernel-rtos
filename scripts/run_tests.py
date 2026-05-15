#!/usr/bin/env python3
"""Build and run firmware tests under QEMU; read pass globals via GDB."""

from __future__ import annotations

import argparse
import re
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

TESTS = [
    ("test_scheduler", "g_scheduler_test_pass", 1),
    ("test_mutex", "g_mutex_test_pass", 1),
    ("test_priority_inheritance", "g_pi_test_pass", 1),
    ("test_msgqueue", "g_msgqueue_test_pass", 2),
    ("test_semaphore", "g_semaphore_test_pass", 1),
    ("test_timeout", "g_timeout_test_pass", 1),
]


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(cmd))
    subprocess.run(cmd, cwd=cwd or ROOT, check=True)


def symbol_address(elf: Path, name: str) -> int:
    out = subprocess.check_output(
        ["arm-none-eabi-nm", str(elf)], text=True, cwd=ROOT
    )
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[2] == name:
            return int(parts[0], 16)
    raise RuntimeError(f"symbol not found: {name}")


def wait_port(port: int, timeout: float = 10.0) -> None:
    deadline = time.time() + timeout
    while time.time() < deadline:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(0.2)
            try:
                s.connect(("127.0.0.1", port))
                return
            except OSError:
                time.sleep(0.05)
    raise RuntimeError(f"GDB port {port} not ready")


def read_pass_via_gdb(elf: Path, port: int, addr: int, expected: int) -> int:
    gdb_cmds = [
        f"target remote :{port}",
        f"x/wx 0x{addr:08X}",
        "quit",
    ]
    gdb_bin = shutil.which("arm-none-eabi-gdb") or shutil.which("gdb-arm-none-eabi")
    proc = subprocess.run(
        [gdb_bin, "-batch", str(elf)]
        + [item for cmd in gdb_cmds for item in ("-ex", cmd)],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    text = proc.stdout + proc.stderr
    m = re.search(r"0x[0-9a-fA-F]+:\s+0x([0-9a-fA-F]+)", text)
    if not m:
        return -1
    val = int(m.group(1), 16)
    return 0 if val == expected else val


def run_one(app: str, sym: str, expected: int, port: int, run_s: float) -> bool:
    elf = ROOT / "build" / "firmware.elf"
    run(["make", f"APP={app}", "all"], cwd=ROOT)
    addr = symbol_address(elf, sym)

    qemu = subprocess.Popen(
        [
            "qemu-system-arm",
            "-M",
            "netduino2",
            "-kernel",
            str(elf),
            "-nographic",
            "-semihosting-config",
            "enable=on,target=native",
            "-gdb",
            f"tcp::{port}",
            "-S",
        ],
        cwd=ROOT,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    try:
        wait_port(port)
        gdb_bin = shutil.which("arm-none-eabi-gdb") or shutil.which("gdb-arm-none-eabi")
        gdb = subprocess.Popen(
            [gdb_bin, "-q", str(elf)],
            cwd=ROOT,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        cmds = f"target remote :{port}\ncontinue\n"
        gdb.stdin.write(cmds)
        gdb.stdin.flush()
        time.sleep(run_s)
        gdb.stdin.write("interrupt\n")
        gdb.stdin.flush()
        time.sleep(0.3)
        val = read_pass_via_gdb(elf, port, addr, expected)
        gdb.terminate()
        ok = val == 0
        print(f"  {app}: {'PASS' if ok else 'FAIL'} ({sym} expected {expected})")
        return ok
    finally:
        qemu.terminate()
        try:
            qemu.wait(timeout=3)
        except subprocess.TimeoutExpired:
            qemu.kill()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=12345)
    parser.add_argument("--seconds", type=float, default=2.5)
    args = parser.parse_args()

    gdb = shutil.which("arm-none-eabi-gdb") or shutil.which("gdb-arm-none-eabi")
    for tool in ("make", "arm-none-eabi-gcc", "qemu-system-arm"):
        if shutil.which(tool) is None:
            print(f"Missing tool: {tool}", file=sys.stderr)
            return 2
    if gdb is None:
        print("Missing tool: arm-none-eabi-gdb (or gdb-arm-none-eabi)", file=sys.stderr)
        return 2

    port = args.port
    all_ok = True
    print("Running firmware tests...")
    for app, sym, expected in TESTS:
        if not run_one(app, sym, expected, port, args.seconds):
            all_ok = False
        port += 1

    if all_ok:
        print("All tests passed.")
        return 0
    print("Some tests failed.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
