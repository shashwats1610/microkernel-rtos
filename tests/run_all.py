"""Top-level test orchestrator.

Runs everything that can run in the current environment and reports a
single pass/fail summary. Tests that require a missing tool (host C
compiler, qemu-system-arm) are reported as SKIPPED rather than FAILED
so the orchestrator is still useful in CI environments that only have
some of the toolchain.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def header(label: str) -> None:
    print("\n" + "=" * 60)
    print(f"== {label}")
    print("=" * 60)


def run(cmd: list[str], cwd: Path | None = None) -> int:
    print(f"$ {' '.join(cmd)}")
    return subprocess.call(cmd, cwd=cwd)


def host_cc() -> str | None:
    for c in ("gcc", "cc", "clang"):
        if shutil.which(c):
            return c
    return None


def main() -> int:
    failures = 0
    skipped = 0

    # --- Python self-consistency tests ---
    header("test_signature_python.py")
    rc = run([sys.executable, str(ROOT / "tests" / "test_signature_python.py")])
    if rc != 0:
        failures += 1

    # --- Host C signature test (build + run) ---
    header("test_signature.c (host)")
    cc = host_cc()
    signed = ROOT / "build" / "app_signed.bin"
    pubkey = ROOT / "bootloader" / "include" / "public_key.h"
    if cc is None:
        print("SKIP: no host C compiler (gcc/cc/clang) on PATH")
        skipped += 1
    elif not signed.exists():
        print(f"SKIP: {signed} missing; run 'make all' first")
        skipped += 1
    elif not pubkey.exists():
        print(f"SKIP: {pubkey} missing; run 'make keys' first")
        skipped += 1
    else:
        out = ROOT / "build" / "test_signature_host"
        if sys.platform.startswith("win"):
            out = out.with_suffix(".exe")
        cmd = [
            cc,
            "-O2", "-Wall", "-Wextra",
            "-DuECC_PLATFORM=0",
            "-DuECC_SUPPORTS_secp160r1=0",
            "-DuECC_SUPPORTS_secp192r1=0",
            "-DuECC_SUPPORTS_secp224r1=0",
            "-DuECC_SUPPORTS_secp256r1=1",
            "-DuECC_SUPPORTS_secp256k1=0",
            "-DuECC_OPTIMIZATION_LEVEL=2",
            "-DuECC_SQUARE_FUNC=1",
            "-DuECC_VLI_NATIVE_LITTLE_ENDIAN=0",
            "-DuECC_ENABLE_VLI_API=0",
            "-Ibootloader/include", "-Icommon", "-Ithird_party/micro-ecc",
            "tests/test_signature.c",
            "tests/host_test_harness.c",
            "bootloader/src/crypto.c",
            "bootloader/src/sha256.c",
            "bootloader/src/crc32.c",
            "third_party/micro-ecc/uECC.c",
            "-o", str(out),
        ]
        if run(cmd, cwd=ROOT) != 0:
            failures += 1
        else:
            if run([str(out), str(signed)], cwd=ROOT) != 0:
                failures += 1

    # --- Power-loss test (requires QEMU) ---
    header("test_power_loss.py")
    rc = run([sys.executable, str(ROOT / "tests" / "test_power_loss.py")])
    if rc != 0:
        failures += 1

    # --- Recovery-mode OTA test (requires QEMU) ---
    header("test_recovery_ota.py")
    rc = run([sys.executable, str(ROOT / "tests" / "test_recovery_ota.py")])
    if rc != 0:
        failures += 1

    # --- Summary ---
    print("\n" + "=" * 60)
    if failures == 0:
        print(f"ALL TESTS PASSED ({skipped} skipped)")
        return 0
    print(f"{failures} TEST(S) FAILED ({skipped} skipped)")
    return 1


if __name__ == "__main__":
    sys.exit(main())
