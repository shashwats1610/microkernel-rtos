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

### 1) Build everything

```bash
make preflight
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

### 3) Run tests

```bash
make test
```

Test runner covers signature checks, power-loss resilience, and recovery-mode OTA flow.

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
- `bootloader/src/recovery.c` - UART recovery command loop for invalid-slot scenarios.
- `application/src/app_main.c` - demo app and boot confirmation handoff behavior.
- `tools/` - key generation, signing, image packaging, flash layout, OTA sender.
- `tests/` - automation for crypto correctness, power-loss, and recovery OTA scenarios.

## Engineering decisions and tradeoffs

- **Slot safety**: active slot is never modified in place during OTA.
- **Config integrity**: `BootConfig` CRC is written last to detect interrupted writes.
- **Compile-time guarantees**: partition overlaps and VTOR alignment are guarded with
  `_Static_assert`.
- **Crypto footprint**: micro-ecc selected for small bootloader size; backend can be replaced
  (see security analysis doc).

## Evidence and testability

- `tests/run_all.py` orchestrates host + target validation passes.
- `tests/test_signature_python.py` validates signed/modified/wrong-key image behavior.
- `tests/test_power_loss.py` repeatedly kills OTA mid-transfer and checks recovery.
- `tests/test_recovery_ota.py` validates recovery path and post-update state progression.

## Limits and boundaries

- QEMU `netduino2` models flash as ROM; this repo uses a QEMU-only SRAM shadow strategy for
  writable regions so OTA flows can be exercised.
- The final "execute newly swapped slot" behavior is fully representative on Renode/hardware,
  while QEMU remains a constrained approximation.
- This is a portfolio-grade secure boot project, not a certified production security stack.

Detailed caveats and threat boundaries:

- [`docs/architecture.md`](docs/architecture.md)
- [`docs/security_analysis.md`](docs/security_analysis.md)

## Deep-dive docs

- [`docs/build_and_run.md`](docs/build_and_run.md) - full build/test/run workflows.
- [`docs/architecture.md`](docs/architecture.md) - memory map and state-machine rationale.
- [`docs/update_protocol.md`](docs/update_protocol.md) - OTA transport framing and semantics.
- [`docs/security_analysis.md`](docs/security_analysis.md) - threat model and residual risk.
- [`ENGINEERING_RETROSPECTIVE.txt`](ENGINEERING_RETROSPECTIVE.txt) - decisions and lessons.

## License

MIT, see [`LICENSE`](LICENSE). Vendored micro-ecc is BSD-2-Clause.
