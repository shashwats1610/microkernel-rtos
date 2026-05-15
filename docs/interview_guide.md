# Interview guide (repo-aligned talking points)

Use this with [`README.md`](../README.md) and [`docs/metrics_and_claims.md`](metrics_and_claims.md).
Only claim what the code and tests demonstrate.

## Elevator pitch

Secure dual-bank Cortex-M bootloader: verify every boot (CRC → SHA-256 → ECDSA),
OTA into inactive slot, post-swap re-verify with revert, boot-attempt rollback,
recovery UART menu, optional HPatchLite delta OTA (uncompressed or tinyuz-compressed).

## Demo commands (fully provisioned machine)

```bash
pip install -r tools/requirements.txt
make preflight    # needs arm-none-eabi-gcc, qemu-system-arm, hdiffi
make keys && make all
make test STRICT=1
make qemu         # terminal A
make ota_server   # terminal B — full image
make delta        # builds patch; see README for --delta push
```

## Common questions

**Walk through OTA.** Host sends framed `START` or `START_DELTA` → inactive slot
write → `crypto_verify_firmware` → `SharedBootBlock.ota_pending` → reset →
`process_ota_pending_swap` in [`bootloader/src/main.c`](../bootloader/src/main.c)
→ post-swap re-verify → revert on failure.

**Power loss?** Active slot never modified in place; config CRC written last;
[`tests/test_power_loss.py`](../tests/test_power_loss.py).

**Downgrade attack?** `BootConfig_t.rollback_counter` rejects images with
`FirmwareHeader_t.version` below the counter; bumped after successful swap.

**Delta OTA?** Patch staged at inactive slot tail; HPatchLite apply; full
signature verify on reconstructed image; [`tests/test_delta_ota.py`](../tests/test_delta_ota.py).

**Rollback without new OTA?** App must call `bootloader_confirm_boot()`; three
failed boots switch slots — [`tests/test_rollback.py`](../tests/test_rollback.py).

**QEMU limits?** 32 KB SRAM shadow per slot; demo images are small; post-swap
*execution* of large swapped images is best validated on Renode/hardware
([`docs/architecture.md`](architecture.md)).

**Production TLS?** Not in firmware; see [`docs/ota_production_transport.md`](ota_production_transport.md).

## Do not claim

- Production deployment counts or field KPIs (not in repo).
- Bandwidth ratios (e.g. “180 KB → 15 KB”) without `make_delta.py` output for your images.
- “Production-certified” — README states portfolio-grade scope.

## Test coverage map

| Test | Proves |
|------|--------|
| `test_signature_python.py` | Host signing pipeline |
| `test_signature.c` | On-device verify code (host-linked) |
| `test_delta_common.py` | Logical image + SHA-256 helpers |
| `test_power_loss.py` | Kill mid-OTA, device recovers |
| `test_recovery_ota.py` | Recovery menu + OTA + swap |
| `test_delta_ota.py` | Delta apply + verify on device |
| `test_rollback.py` | Boot-attempt rollback to good slot |
