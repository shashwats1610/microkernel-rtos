# Metrics, bandwidth, and what this repository can prove

This note documents **evidence boundaries** for interviews and résumés: what the Secure BootLoader implementation actually measures versus what must come from external operational data.

## OTA transfer model (implemented)

- **Full OTA:** **`START`** with **`total_size`**, stream **`DATA`** into the inactive slot, **`END`** → **`crypto_verify_firmware()`** on the full written image.
- **Delta OTA:** **`START_DELTA`** with patch length, expected new image length, and **SHA-256 of the active-slot image**; stream patch bytes into the **tail** of the inactive slot; **`END`** → HPatchLite apply (uncompressed or tinyuz-compressed) → **`crypto_verify_firmware()`** on the reconstructed image at the inactive slot base.

References: [`bootloader/include/ota_client.h`](../bootloader/include/ota_client.h), [`docs/update_protocol.md`](update_protocol.md), [`tools/make_delta.py`](../tools/make_delta.py).

Host tooling generates patches with **`hdiffi`** (HPatchLite). The bootloader supports **`hpi_compressType_no`** and **`hpi_compressType_tuz`** (tinyuz decoder vendored under `third_party/tinyuz/`). Other HPatchLite compressors are not linked.

## Interpreting “180 KB → 15 KB”

| Interpretation | Supported by this repo? |
|----------------|-------------------------|
| **15 KB as an OTA transfer size** | **Yes, only if** it refers to the **`patch_total`** sent after **`START_DELTA`** — wire cost is the patch file size, not the full new image size. |
| **180 KB → 15 KB as measured bandwidth savings** | **Only with evidence:** compare **`len(patch)`** from **`make_delta.py`** output vs **`len(full_signed_new.bin)`** for the **same** old/new pair you built — not from numbers invented outside that pipeline. |
| **Verification after delta** | Always a **full signed image** in the inactive slot after apply; verification is identical to full OTA (**CRC → SHA-256 → ECDSA**). |

**Bounds from layout:** Maximum payload per slot follows `FIRMWARE_MAX_PAYLOAD_SIZE` in [`common/firmware_format.h`](../common/firmware_format.h) (approximately 470 KiB payload class, plus the 512-byte header). Minimum payload is any **`image_size > 0`** accepted by [`bootloader/src/crypto.c`](../bootloader/src/crypto.c).

## Maintenance reduction (visits, %)

This repository contains **no** field KPIs, ticketing data, or before/after truck-roll statistics. You **cannot** defend a specific visit reduction or percentage from this codebase alone—only the general principle that remote OTA can reduce manual visits compared to in-person flashing.

## Field deployment versus lab / QEMU

Project documentation emphasizes **QEMU / lab-style** validation and a **portfolio-grade** scope (see root [`README.md`](../README.md)). There is **no** stated count of production units shipped in this tree.

## Defensible talking points (repo-only)

1. **Transfer size:** Either **full signed image** (`START`) or **patch blob size** (`START_DELTA`). Upper bounds still come from `firmware_format.h` and `memory_map.h`; delta additionally requires `expected_new_total + patch_total <= SLOT_SIZE`.

2. **Maintenance metrics:** **Not** recorded in this repo; do not cite visit counts or percentage reduction unless you have separate employer or operations evidence.

3. **Deployment:** Describe validation as **documented lab/QEMU flows** (`make qemu`, `make test`, etc.), not as a field rollout supported by numbers in this repository.

Employer-specific or LLM-generated figures belong to **that** context; align public claims with either measured data or this implementation’s limits.
