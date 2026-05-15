# Secure Dual-Bank Bootloader with OTA Updates

Secure boot + OTA update system for Cortex-M MCUs (STM32F4-class design), built to show real
embedded security and reliability engineering: signed images, dual-slot rollback logic, power-loss
resilience, and bounded recovery behavior.

## What this project proves

- You can implement verified boot with practical embedded constraints (`CRC32 -> SHA-256 -> ECDSA`).
- You can design safe OTA flows using inactive-slot writes + post-swap validation + rollback logic.
- You can reason about failure paths (torn config writes, repeated boot failures, recovery mode).
- You can build a testable update pipeline with host tools and target-side protocol handling.

## Architecture at a glance

- **Partitions**: bootloader + boot config + Slot A + Slot B.
- **Verification model**: each candidate slot is verified before control transfer.
- **Update model**: OTA writes inactive slot, verifies, marks pending, resets, and commits swap.
- **Confirmation model**: application confirms boot via shared SRAM flag; missing confirmation
  contributes to rollback.

Primary references:

- [`docs/architecture.md`](docs/architecture.md)
- [`common/memory_map.h`](common/memory_map.h)
- [`common/firmware_format.h`](common/firmware_format.h)

## Quick verification (5-10 minutes)

### Prerequisites

```bash
pip install -r tools/requirements.txt
make preflight    # arm-none-eabi-gcc, qemu-system-arm, python, hdiffi
```

`hdiffi` (from [HPatchLite](https://github.com/sisong/HPatchLite)) is required for delta patches
and for `make delta` / delta integration tests.

### 1) Build everything

```bash
make keys
make all
```

### 2) Run QEMU profile and push OTA image

```bash
# Terminal A
make qemu

# Terminal B
make ota_server
```

Expected: OTA transfer completes, bootloader reboots, and logs show a verified jump decision.

### 3) Optional: delta OTA

```bash
make delta
# With QEMU still running (recovery path: corrupt Slot A first — see docs/build_and_run.md)
python tools/ota_server.py --delta build/app_v1_to_v2.patch \
    --old-signed build/app_v1_signed.bin \
    --new-signed build/app_v2_signed.bin --tcp 127.0.0.1:4444
```

### 4) Run tests

```bash
make test              # skips missing tools
make test STRICT=1     # fail if QEMU/gcc/build artifacts missing
```

`make test` runs: signature (Python + host C), delta helpers, power-loss, recovery OTA,
delta OTA, and rollback tests when the toolchain is available.

## Build profiles

- **QEMU profile** (`BUILD=qemu`, default):
  - Cortex-M3 flags + `QEMU_FLASH_SIM=1`
  - best for cross-platform smoke tests and protocol iteration
- **Hardware-fidelity profile** (`BUILD=hw`):
  - Cortex-M4 hard-float flags
  - intended for Renode or physical STM32F407 validation

Profile behavior and commands are in [`docs/build_and_run.md`](docs/build_and_run.md).

## Module map

- `bootloader/src/main.c` - top-level boot state machine, slot selection, rollback path.
- `bootloader/src/boot_config.c` - persistent config load/save/default/validate logic.
- `bootloader/src/crypto.c` - image verification pipeline (CRC, SHA-256, ECDSA).
- `bootloader/src/flash_driver.c` - flash abstraction (QEMU shim + HW path).
- `bootloader/src/ota_client.c` - framed OTA receiver and inactive-slot write flow.
- `bootloader/src/delta_patch.c` - HPatchLite patch apply (uncompressed or tinyuz-compressed).
- `bootloader/src/recovery.c` - UART recovery command loop for invalid-slot scenarios.
- `application/src/app_main.c` - demo app and boot confirmation handoff behavior.
- `tools/` - key generation, signing, image packaging, flash layout, OTA sender, delta helpers.
- `tests/` - automation for crypto, power-loss, recovery, delta, and rollback scenarios.

## Engineering decisions and tradeoffs

- **Slot safety**: active slot is never modified in place during OTA.
- **Config integrity**: `BootConfig` CRC is written last to detect interrupted writes.
- **Anti-downgrade**: `rollback_counter` and slot version fields enforce a monotonic floor.
- **Compile-time guarantees**: partition overlaps and VTOR alignment are guarded with
  `_Static_assert`.
- **Crypto footprint**: micro-ecc selected for small bootloader size; backend can be replaced
  (see security analysis doc).

## Evidence and testability

- `tests/run_all.py` orchestrates host + QEMU validation (`--strict` for CI).
- `tests/test_signature_python.py` validates signed/modified/wrong-key image behavior.
- `tests/test_power_loss.py` repeatedly kills OTA mid-transfer and checks recovery.
- `tests/test_recovery_ota.py` validates recovery path and post-update state progression.
- `tests/test_delta_ota.py` validates `START_DELTA` apply + verify on device (QEMU).
- `tests/test_rollback.py` validates boot-attempt rollback to the good slot.

## Limits and boundaries

- QEMU `netduino2` models flash as ROM; this repo uses a QEMU-only SRAM shadow strategy for
  writable regions so OTA flows can be exercised.
- The final "execute newly swapped slot" behavior is fully representative on Renode/hardware,
  while QEMU remains a constrained approximation.
- This is a portfolio-grade secure boot project, not a certified production security stack.
- Production TLS/HTTP is documented but not implemented in firmware; see
  [`docs/ota_production_transport.md`](docs/ota_production_transport.md).

Detailed caveats and threat boundaries:

- [`docs/architecture.md`](docs/architecture.md)
- [`docs/security_analysis.md`](docs/security_analysis.md)

## Deep-dive docs

- [`docs/build_and_run.md`](docs/build_and_run.md) - full build/test/run workflows.
- [`docs/architecture.md`](docs/architecture.md) - memory map and state-machine rationale.
- [`docs/update_protocol.md`](docs/update_protocol.md) - OTA transport framing and semantics.
- [`docs/security_analysis.md`](docs/security_analysis.md) - threat model and residual risk.
- [`docs/metrics_and_claims.md`](docs/metrics_and_claims.md) - delta OTA sizing and evidence boundaries.
- [`docs/ota_production_transport.md`](docs/ota_production_transport.md) - TLS/lwIP integration guide.
- [`docs/interview_guide.md`](docs/interview_guide.md) - repo-aligned interview talking points.
- [`ENGINEERING_RETROSPECTIVE.txt`](ENGINEERING_RETROSPECTIVE.txt) - decisions and lessons.

## License

MIT, see [`LICENSE`](LICENSE). Vendored micro-ecc is BSD-2-Clause; HPatchLite and tinyuz are MIT.
