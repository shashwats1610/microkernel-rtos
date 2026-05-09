# Secure Dual-Bank Bootloader with OTA Updates

A production-grade, ECDSA-P256-verified, dual-bank Secure Bootloader with
UART-based OTA updates, automatic rollback, watchdog, and power-loss
safety for ARM Cortex-M4 microcontrollers (STM32F407VGT6).

The same source builds for two profiles:

- **QEMU profile** (`-machine netduino2`): Cortex-M3 + soft-float +
  `QEMU_FLASH_SIM=1`. Runs in QEMU 7.0+ on Linux / macOS / Windows.
- **HW profile**: Cortex-M4 + hard-float + FPU. Runs in Renode
  (`stm32f4_discovery`) and on physical STM32F407 silicon.

> QEMU's `netduino2` machine (STM32F2 family) is used for simulation. For
> full STM32F407 peripheral fidelity, use Renode with the
> `stm32f4_discovery` platform - see [`docs/build_and_run.md`](docs/build_and_run.md).

## Memory layout

```
0x08000000  Bootloader (64 KB)              sectors 0-3
0x08010000  BootConfig (4 KB used in sector 4)
0x08011000  Slot A (460 KB)                 [header 512B][app payload]
0x08084000  Slot B (460 KB)                 [header 512B][app payload]
0x080F7000  end of used flash

0x20000000  SRAM start (128 KB contiguous; CCM 64 KB at 0x10000000)
0x2001FFF0  SharedBootBlock (16 B, NOLOAD, not zeroed by startup)
0x2001FFFC  RAM end
```

Authoritative source: [`common/memory_map.h`](common/memory_map.h).
Compile-time `_Static_assert` proves all four partitions don't overlap
and that both application vector tables are 512-byte aligned (the
Cortex-M4 VTOR requirement for the F407's 98-vector table).

## Prerequisites

- ARM GNU Toolchain (`arm-none-eabi-gcc` 10+)
- QEMU 7.0+ (`qemu-system-arm`)
- GNU Make 4+
- Python 3.10+ with `pip install -r tools/requirements.txt`
  (`ecdsa`, `pyserial`)
- Optional: Renode 1.14+ for F407 fidelity

Run `make preflight` to verify everything is on `PATH`.

## Quickstart

```bash
make keys                      # one-time: generate ECDSA P-256 key pair
make all                       # bootloader + app + signed image + flash.bin

# Terminal A
make qemu                      # bootloader logs on stdio,
                               # OTA UART exposed on TCP 127.0.0.1:4444

# Terminal B
make ota_server                # pushes build/app_signed.bin via TCP 4444
make test                      # runs all tests that have their tooling
```

QEMU exposes the bootloader's OTA UART on TCP port 4444 for
cross-platform compatibility (no COM-port wrangling on Windows).
`tools/ota_server.py` connects via a plain TCP socket and speaks the
same framed protocol regardless of platform; pass `--serial COMx` /
`--serial /dev/ttyUSB0` to use a real serial port instead.

## Security model

- **ECDSA-P256 signature** (NIST curve secp256r1) verifies every slot
  before the bootloader transfers control. The signing input is
  `[FirmwareHeader_t with .signature zeroed || payload]`, so signer and
  verifier hash byte-identical bytes.
- **Raw `r||s` signature format** (64 B, big-endian, fixed-length).
  Identical to micro-ecc's `uECC_verify` API; no DER / ASN.1 surface.
- **CRC32 fast pre-check** rejects obviously corrupt or wrong-key
  images before SHA-256 + ECDSA, saving ~120 ms on the failure path.
- **Three-strike rollback**: if `boot_attempts > 3` (the application
  failed to call `bootloader_confirm_boot()` three times in a row), the
  bootloader switches active slot, resets attempts, and tries again.
- **Atomic config updates**: `BootConfig_t.crc32` is programmed last,
  so a torn write at any point before that leaves the on-flash record
  CRC-invalid and the next boot falls back to defaults.
- **IWDG (10 s)** kicks in if the bootloader ever hangs.

See [`docs/security_analysis.md`](docs/security_analysis.md) for the
threat model, residual risks, and the mbedTLS swap path for production.

## Boot flow

```mermaid
flowchart TD
    Reset[Reset] --> Init[Init clock+UART+IWDG] --> Cfg[boot_config_load]
    Cfg --> Conf{boot_confirmed?}
    Conf -->|yes| Reset0[attempts=0, save] --> Pend{ota_pending?}
    Conf -->|no| Pend
    Pend -->|yes| VInact[Verify inactive]
    Pend -->|no|  VAct[Verify active]
    VInact -->|ok|   Swap[active=inactive, save] --> VNew[Verify NEW]
    VInact -->|fail| ClrP[Clear pending] --> VAct
    VNew -->|ok|   Att{attempts > 3?}
    VNew -->|fail| Revert[active=old, save] --> VAct
    VAct  -->|ok|   Att
    VAct  -->|fail| Other[Verify other]
    Att   -->|no|  Bump[attempts++, save, jump]
    Att   -->|yes| Switch[switch active, attempts=0, save] --> VAct
    Other -->|ok|   Bump
    Other -->|fail| Rec[Recovery menu - infinite UART loop]
```

See [`docs/architecture.md`](docs/architecture.md) for the full diagram
and rationale for each correctness fix (post-swap re-verify,
boot-attempts reset, app-confirm check, recovery-loop expansion).

### QEMU caveat: flash is ROM, with a SRAM shadow

QEMU 11's `netduino2` (and every STM32F2/F4 SoC model in mainline QEMU)
implements the on-chip flash as a true ROM region
(`memory_region_init_rom`): CPU loads return the image loaded via
`-kernel`, but **CPU stores are silently dropped at the bus**. A
faithful bootloader test can't run if the bootloader can never persist
its config or the OTA payload, so the QEMU build (`-DQEMU_FLASH_SIM=1`)
keeps a 70 KB SRAM-backed shadow of the writable flash regions
(boot config + first 32 KB of each slot) at `0x2000E000` in a NOLOAD
linker section. `flash_program_*()` and `flash_erase_*()` write to the
shadow only; `flash_read()` and `flash_get_ptr()` (used by `crypto.c`)
serve from the shadow when the address falls in a shadowed window. A
magic cookie distinguishes a cold boot (init shadow from ROM) from a
soft reset via `bootloader_system_reset()`/AIRCR (keep shadow), which
is what makes the post-OTA re-verify-after-reset path testable on QEMU.

The shadow is **only** present in the QEMU build (`BUILD=qemu`). The HW
build (`BUILD=hw`) excludes the entire shadow code path and the
`.flash_shadow` linker section is empty. There is one observable QEMU-
only artefact: after a successful OTA + soft reset, the bootloader
verifies and announces `INFO: jumping to slot B`, but the actual
instruction fetch at `0x08084200` still returns the original ROM bytes
(0xFF, since `flash_layout.py` left slot B blank). On real silicon or
on Renode the swap-and-execute path completes end-to-end. The
recovery-OTA test (`tests/test_recovery_ota.py`) asserts up to and
including the `INFO: jumping to slot B` message and is explicit about
this boundary.

## OTA protocol

```
Frame:    [SOH=0x01][OP][SEQ(2)][LEN(2)][DATA(LEN)][CRC32(4)]   (LE)
Response: [ACK=0x06 | NAK=0x15][SEQ(2)]
OP: START(0x21) | DATA(0x22) | END(0x23) | ABORT(0x2F)
```

Full state machine and HTTP-equivalent table:
[`docs/update_protocol.md`](docs/update_protocol.md).

## Performance & test metrics

These are the targets the project was designed to hit. Real measurements
come from `arm-none-eabi-size` and timing measurements on the smoke run
(see "Smoke run" below); the table is updated post-build.

- **Bootloader code size:** target 40-45 KB / hard ceiling 50 KB out of
  the 64 KB partition. **Measured (QEMU build, M3 soft-float, `-O2`):
  14,024 B flash footprint** = 392 B `.isr_vector` + 11,020 B `.text`
  + 2,612 B `.rodata`, 0 B `.data`. That's 13.7 KB, leaving 50.3 KB
  headroom (~78% under the ceiling). From
  `arm-none-eabi-objdump -h build/bootloader/bootloader.elf`.
- **Boot time (power-on -> app entry):** target < 500 ms on real
  Cortex-M4 @ 168 MHz, broken down as:
  - ECDSA verify (micro-ecc, SECP256R1): ~120 ms
  - SHA-256 over 180 KB payload: ~80 ms
  - CRC32 fast pre-check (180 KB): ~25 ms
  - Flash reads: ~30 ms
  - Clock + UART + IWDG init + config load + jump: ~50 ms
  - Headroom: ~195 ms
  - **In QEMU (M3, no FPU)** boot-to-banner-to-app is effectively
    instantaneous (sub-second wall clock) because QEMU runs without
    cycle-accurate timing; we report the breakdown as estimated targets
    and validate proportions via Renode's cycle counter.
- **OTA transfer (115200 baud, 180 KB):** ~14.2 s pure throughput +
  ACK round-trips ≈ 15-17 s estimated for a real-hardware UART link.
  **Measured (TCP loopback to QEMU, 2,244 B image, 256 B chunks):**
  ~30-80 ms wall-clock for the full DATA loop end-to-end (verify
  included), bound by the bootloader's flash-program-and-verify
  loop and the Python frame builder, NOT by the link.
- **RAM overhead (real STM32F4 build, excluding stack/heap reserve):**
  target < 2 KB. **Measured: 20 B `.bss` + 0 B `.data`**, primarily the
  OTA receiver's per-transfer `g_state`. The `.bss = 78,876 B` reported
  by `size` for the QEMU build also counts the 9,220 B linker-reserved
  stack/heap and the 69,636 B QEMU-only `.flash_shadow` NOLOAD region
  (see "QEMU caveat" in the boot flow section). The HW build (`BUILD=hw`)
  drops `.flash_shadow` entirely.

### Test coverage checklist

Status after `python tests/run_all.py` on the QEMU build:

- [x] Signature verification: valid accepted, single-byte flip rejected,
  full-image flip rejected, wrong-key rejected
  (`tests/test_signature_python.py`, 4/4 pass; the same logic is also
  exercised by `tests/test_signature.c` on hosts with a C compiler).
- [x] Power-loss: 8 randomised mid-OTA kill iterations, device always
  recovers to a verified active slot (`tests/test_power_loss.py`, 8/8 pass;
  configurable via `POWERLOSS_ITERS`).
- [x] Both slots corrupt -> bootloader enters UART recovery mode and
  prints the U/V/R menu (covered by `tests/test_recovery_ota.py`).
- [x] OTA upgrade end-to-end via TCP 4444 in recovery mode: START->DATA*->
  END accepted, NVIC soft reset, post-reset shadow preserved, post-reset
  re-verify of new active slot succeeds, bootloader announces
  `INFO: jumping to slot B` (`tests/test_recovery_ota.py`, pass).
- [ ] Rollback on 3 consecutive un-confirmed boots triggers slot switch -
  the state-machine path is implemented and unit-traceable in `main.c`,
  but the QEMU process restart drops the shadow so a multi-process boot-
  attempt counter test is **Renode/HW only**. Documented as a QEMU
  limitation, not a bootloader bug.
- [ ] Boot confirmation: app must call `bootloader_confirm_boot()`
  within 60 s; otherwise next boot rolls back. Same Renode/HW-only
  caveat: requires multiple full reboots with persistent flash, which
  QEMU's ROM model does not provide.

## Memory budget (64 KB bootloader limit)

Estimated breakdown vs the actual numbers from `arm-none-eabi-size`
after the smoke run:

- **Startup + vector table (`startup.s` + `.isr_vector`):** est. 1 KB
- **Boot state machine (`main.c`):** est. 3 KB
- **Flash driver (`flash_driver.c`, QEMU sim path + ROM-shadow logic):**
  est. 2 KB
- **ECDSA (micro-ecc, secp256r1-only, opt level 2):** est. 6 KB
- **SHA-256 (`sha256.c`):** est. 2 KB
- **CRC32 (`crc32.c`, table-free):** est. 0.5 KB
- **UART driver:** est. 1 KB
- **OTA client (`ota_client.c`):** est. 1 KB
- **Boot config management (`boot_config.c`):** est. 1 KB
- **Crypto verify glue (`crypto.c`):** est. 0.5 KB
- **Recovery menu (`recovery.c`):** est. 0.5 KB
- **IWDG (`iwdg.c`):** est. 0.1 KB
- **Final linked flash footprint (after `-Wl,--gc-sections`):**
  **`.isr_vector` 392 B + `.text` 11,020 B + `.rodata` 2,612 B
  = 14,024 B (~13.7 KB)** with no `.data`. From
  `arm-none-eabi-objdump -h build/bootloader/bootloader.elf`.
- **Safety margin (vs 64 KB partition):** **50.3 KB** (~78% headroom).

## Smoke run

The full pipeline (run from a Developer PowerShell or any shell with the
ARM toolchain + QEMU + Python 3 on PATH):

```bash
make distclean                                       # start clean
make keys                                            # ECDSA-P256 key pair
make all                                             # build everything
arm-none-eabi-objdump -h build/bootloader/bootloader.elf | findstr "text rodata"
python tests/run_all.py                              # signature + power-loss + recovery-OTA
```

The expected, verified output (current build):

- `arm-none-eabi-objdump -h` reports `.text 0x2b0c (11,020 B)` and
  `.rodata 0xa34 (2,612 B)`, so the bootloader fills 13.7 KB of its
  64 KB partition.
- `python tests/run_all.py` ends with
  `ALL TESTS PASSED (1 skipped)` - the one skip is the host C
  signature test, which only runs when a non-cross C compiler
  (`gcc`/`cc`/`clang`) is present on the host PATH.

The interactive demo flow (in two terminals):

```bash
make qemu                       # terminal A: bootloader + Slot A demo app
make ota_server                 # terminal B: pushes v1.0.0 image
                                # (see "QEMU caveat" below)
```

## Troubleshooting

- **"QEMU hangs at boot / no output on stdio"** - verify `-machine
  netduino2`, check that `build/flash.bin` is exactly 1 MB
  (`Get-Item build\flash.bin` on Windows, `ls -l build/flash.bin` on
  Unix), confirm the linker script vector table is at `0x08000000`
  (`arm-none-eabi-objdump -d build/bootloader/bootloader.elf | head`
  should show `Reset_Handler` reachable from offset 4 of the vector
  table).
- **"Signature verification fails for a freshly built image"** -
  usually a key mismatch: re-run `make keys` then `make clean all` so
  `embed_pubkey.py` re-emits `public_key.h` and the bootloader is
  rebuilt against the new key. Second-most-common cause: byte-order
  mismatch between signer and verifier - both must use big-endian
  `r||s`, fixed 32 B each. The signed image's signature lives at
  header offset 20.
- **"OTA times out connecting to TCP 4444"** - confirm the QEMU
  process is up and listening (`netstat -an | findstr 4444` on Windows,
  `lsof -i :4444` on Unix), verify no firewall blocks loopback, ensure
  `make qemu` was launched with `-serial tcp:127.0.0.1:4444,server,nowait`.
- **"OTA serial mode times out on real hardware"** - port permissions
  (`dialout` group on Linux; the COM port not held by another process
  on Windows), baud rate mismatch (must be 115200), 8N1 only, no flow
  control. Pass `--serial COMx` / `--serial /dev/ttyUSB0` to
  `tools/ota_server.py`.
- **"`arm-none-eabi-gcc: command not found`"** - install ARM GNU
  Toolchain from arm.com and add `bin/` to `PATH`. On Windows the
  installer can do this; verify with `arm-none-eabi-gcc --version`.
- **"Bootloader > 64 KB"** - drop `uECC_OPTIMIZATION_LEVEL` from 2 to 1,
  remove the recovery-menu strings (`recovery.c`), or strip the verbose
  log lines in `main.c`. `-ffunction-sections -fdata-sections
  -Wl,--gc-sections` is already on; check the `bootloader.map` to find
  the largest contributors.
- **"`ModuleNotFoundError: No module named 'ecdsa'`"** - run
  `python -m pip install -r tools/requirements.txt`.

## Repository layout

```
bootloader/   64 KB bootloader (this is the secure part)
  src/        main.c boot_config.c flash_driver.c crypto.c ota_client.c
              recovery.c crc32.c sha256.c uart.c iwdg.c
  include/    public headers
  startup.s   vector table + Reset_Handler
  linker_bootloader.ld

application/  demo app v1.0.0
  src/        app_main.c system_app.c
  include/    bootloader_api.h (bootloader_confirm_boot)
  startup_app.s
  linker_app.ld

common/       shared headers (firmware_format, memory_map, shared_memory)
tools/        Python tooling (key generation, signing, OTA push, flash layout)
tests/        signature KAT (host C + Python), power-loss harness, run_all
docs/         architecture, security analysis, build/run, OTA protocol
third_party/  micro-ecc (BSD-2)
```

## License

MIT, see [LICENSE](LICENSE). Vendored micro-ecc is BSD-2-Clause.
