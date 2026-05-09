# =====================================================================
# Secure Dual-Bank Bootloader with OTA - Top-level Makefile
# =====================================================================
#
# Two target profiles (use either via the `BUILD=qemu` / `BUILD=hw`
# variable; default is qemu):
#
#   BUILD=qemu  -> Cortex-M3 + soft-float + QEMU_FLASH_SIM=1
#                  Boots in `qemu-system-arm -machine netduino2`.
#   BUILD=hw    -> Cortex-M4 + hard-float + FPU + real FLASH register
#                  code path. Use with Renode (stm32f4_discovery) or
#                  physical STM32F407 silicon.
#
# Common targets:
#   make preflight       - check for required toolchain components
#   make keys            - generate ECDSA P-256 keys + embed pubkey
#   make all             - bootloader + app + signed image + flash.bin
#   make qemu            - run QEMU with build/flash.bin
#   make ota_server      - push the latest signed app over TCP 4444
#   make test            - run host + target tests
#   make clean           - remove build artifacts (keeps keys + headers)
#   make distclean       - clean + remove keys
# ---------------------------------------------------------------------

BUILD ?= qemu

CC      = arm-none-eabi-gcc
AS      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size
PYTHON  = python

# Cross-platform mkdir / rmdir helpers using Python (works on Windows cmd
# and Unix sh alike).
MKDIR_P = $(PYTHON) -c "import os, sys; os.makedirs(sys.argv[1], exist_ok=True)"
RM_RF   = $(PYTHON) -c "import shutil, sys; shutil.rmtree(sys.argv[1], ignore_errors=True)"
RM_F    = $(PYTHON) -c "import os, sys; [os.remove(p) for p in sys.argv[1:] if os.path.exists(p)]"

ROOT       := $(CURDIR)
BUILD_DIR  := build
B_DIR      := $(BUILD_DIR)/bootloader
A_DIR      := $(BUILD_DIR)/app

# --- Profile-specific flags -----------------------------------------------
ifeq ($(BUILD),qemu)
  PROFILE_CFLAGS := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft \
                    -DQEMU_FLASH_SIM=1
else ifeq ($(BUILD),hw)
  PROFILE_CFLAGS := -mcpu=cortex-m4 -mthumb \
                    -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
                    -D__FPU_PRESENT=1U
else
  $(error BUILD must be 'qemu' or 'hw' (got '$(BUILD)'))
endif

# --- Common flags ---------------------------------------------------------
WARN := -Wall -Wextra -Werror \
        -Wno-unused-parameter -Wno-unused-but-set-variable

CFLAGS_COMMON := $(WARN) -ffunction-sections -fdata-sections \
                 -O2 -g3 -std=c11 -fno-common -fstack-usage

INCLUDES_BL := -Ibootloader/include -Icommon -Ithird_party/micro-ecc

# micro-ecc configuration: SECP256R1 only, generic-C platform (compiles
# everywhere; the small speed loss versus the ARM-thumb asm path is well
# worth the portability and binary-size predictability).
UECC_DEFINES := \
  -DuECC_PLATFORM=0 \
  -DuECC_SUPPORTS_secp160r1=0 \
  -DuECC_SUPPORTS_secp192r1=0 \
  -DuECC_SUPPORTS_secp224r1=0 \
  -DuECC_SUPPORTS_secp256r1=1 \
  -DuECC_SUPPORTS_secp256k1=0 \
  -DuECC_OPTIMIZATION_LEVEL=2 \
  -DuECC_SQUARE_FUNC=1 \
  -DuECC_VLI_NATIVE_LITTLE_ENDIAN=0 \
  -DuECC_ENABLE_VLI_API=0

LDFLAGS_COMMON := -Wl,--gc-sections -nostartfiles -specs=nano.specs \
                  -specs=nosys.specs

# --- Bootloader sources ---------------------------------------------------
BL_SRCS_C := \
  bootloader/src/main.c \
  bootloader/src/boot_config.c \
  bootloader/src/flash_driver.c \
  bootloader/src/crypto.c \
  bootloader/src/ota_client.c \
  bootloader/src/recovery.c \
  bootloader/src/crc32.c \
  bootloader/src/sha256.c \
  bootloader/src/uart.c \
  bootloader/src/iwdg.c \
  third_party/micro-ecc/uECC.c

BL_SRCS_S := bootloader/startup.s

BL_OBJS := $(addprefix $(B_DIR)/, \
            $(notdir $(BL_SRCS_C:.c=.o) $(BL_SRCS_S:.s=.o)))

BL_LDSCRIPT := bootloader/linker_bootloader.ld
BL_ELF      := $(B_DIR)/bootloader.elf
BL_BIN      := $(B_DIR)/bootloader.bin

# --- Application sources --------------------------------------------------
APP_VERSION ?= 1.0.0

APP_SRCS_C := \
  application/src/app_main.c \
  application/src/system_app.c \
  bootloader/src/uart.c \
  bootloader/src/iwdg.c

APP_SRCS_S := application/startup_app.s

APP_OBJS := $(addprefix $(A_DIR)/, \
             $(notdir $(APP_SRCS_C:.c=.o) $(APP_SRCS_S:.s=.o)))

APP_LDSCRIPT := application/linker_app.ld
APP_ELF      := $(A_DIR)/app.elf
APP_BIN      := $(A_DIR)/app.bin

# --- VPATH so wildcard %.o rules can find sources by basename -------------
vpath %.c bootloader/src application/src third_party/micro-ecc
vpath %.s bootloader application

# =====================================================================
# Default target
# =====================================================================
.PHONY: all
all: keys $(BL_BIN) $(APP_BIN) image sign flash_image
	@echo "==== build complete ===="
	@$(SIZE) $(BL_ELF) $(APP_ELF)

# =====================================================================
# preflight: toolchain sanity check (cross-platform via Python)
# =====================================================================
.PHONY: preflight
preflight:
	@$(PYTHON) -c "import shutil, sys; \
[print(t, '->', shutil.which(t) or 'MISSING') for t in ['arm-none-eabi-gcc','qemu-system-arm',sys.executable,'make']]; \
sys.exit(0 if shutil.which('arm-none-eabi-gcc') else 1)"

# =====================================================================
# keys + embedded pubkey header
# =====================================================================
KEYS_PRIV := keys/private_key.pem
PUBKEY_H  := bootloader/include/public_key.h

.PHONY: keys
keys: $(PUBKEY_H)

$(KEYS_PRIV):
	$(PYTHON) tools/generate_keys.py

$(PUBKEY_H): $(KEYS_PRIV)
	$(PYTHON) tools/embed_pubkey.py

# =====================================================================
# Bootloader
# =====================================================================
$(B_DIR):
	@$(MKDIR_P) $@

$(B_DIR)/%.o: %.c | $(B_DIR) $(PUBKEY_H)
	$(CC) $(PROFILE_CFLAGS) $(CFLAGS_COMMON) $(INCLUDES_BL) $(UECC_DEFINES) \
	      -c -o $@ $<

$(B_DIR)/%.o: %.s | $(B_DIR)
	$(AS) $(PROFILE_CFLAGS) -c -o $@ $<

$(BL_ELF): $(BL_OBJS) $(BL_LDSCRIPT)
	$(CC) $(PROFILE_CFLAGS) $(CFLAGS_COMMON) -T $(BL_LDSCRIPT) \
	      $(LDFLAGS_COMMON) \
	      -Wl,-Map=$(B_DIR)/bootloader.map \
	      -o $@ $(BL_OBJS)
	@$(SIZE) $@

$(BL_BIN): $(BL_ELF)
	$(OBJCOPY) -O binary $< $@

.PHONY: bootloader
bootloader: $(BL_BIN)

# =====================================================================
# Application
# =====================================================================
$(A_DIR):
	@$(MKDIR_P) $@

# App-specific compile rule (separate from bootloader rule because we want
# different output dir, different defines).
$(A_DIR)/%.o: %.c | $(A_DIR)
	$(CC) $(PROFILE_CFLAGS) $(CFLAGS_COMMON) \
	      -Iapplication/include -Ibootloader/include -Icommon \
	      -DAPP_VERSION_STR=\"$(APP_VERSION)\" \
	      -c -o $@ $<

$(A_DIR)/%.o: %.s | $(A_DIR)
	$(AS) $(PROFILE_CFLAGS) -c -o $@ $<

$(APP_ELF): $(APP_OBJS) $(APP_LDSCRIPT)
	$(CC) $(PROFILE_CFLAGS) $(CFLAGS_COMMON) -T $(APP_LDSCRIPT) \
	      $(LDFLAGS_COMMON) \
	      -Wl,-Map=$(A_DIR)/app.map \
	      -o $@ $(APP_OBJS)
	@$(SIZE) $@

$(APP_BIN): $(APP_ELF)
	$(OBJCOPY) -O binary $< $@

.PHONY: app
app: $(APP_BIN)

# =====================================================================
# Sign and image
# =====================================================================
APP_UNSIGNED := $(BUILD_DIR)/app_unsigned.bin
APP_SIGNED   := $(BUILD_DIR)/app_signed.bin

.PHONY: image
image: $(APP_UNSIGNED)

$(APP_UNSIGNED): $(APP_BIN)
	$(PYTHON) tools/create_image.py $< $@ --version $(APP_VERSION)

.PHONY: sign
sign: $(APP_SIGNED)

$(APP_SIGNED): $(APP_UNSIGNED) $(KEYS_PRIV)
	$(PYTHON) tools/sign_firmware.py $< $@

# =====================================================================
# flash.bin combined image
# =====================================================================
FLASH_BIN := $(BUILD_DIR)/flash.bin

.PHONY: flash_image
flash_image: $(FLASH_BIN)

$(FLASH_BIN): $(BL_BIN) $(APP_SIGNED)
	$(PYTHON) tools/flash_layout.py \
	    --bootloader $(BL_BIN) \
	    --slot-a $(APP_SIGNED) \
	    --out $@

# =====================================================================
# QEMU launch
# =====================================================================
.PHONY: qemu
qemu: $(FLASH_BIN)
	@echo "Launching QEMU - bootloader logs on stdio, OTA UART on TCP 4444"
	qemu-system-arm -machine netduino2 -nographic \
	    -kernel $(FLASH_BIN) \
	    -serial mon:stdio \
	    -serial tcp:127.0.0.1:4444,server,nowait

# =====================================================================
# OTA server (push the latest signed app to a running QEMU/HW target)
# =====================================================================
.PHONY: ota_server
ota_server: $(APP_SIGNED)
	$(PYTHON) tools/ota_server.py $(APP_SIGNED) --tcp 127.0.0.1:4444

# =====================================================================
# Renode (emit a resc script; user invokes Renode manually)
# =====================================================================
RENODE_SCRIPT := $(BUILD_DIR)/run.resc
.PHONY: renode
renode: $(BL_ELF) $(APP_SIGNED)
	@$(MKDIR_P) $(BUILD_DIR)
	@$(PYTHON) -c "import sys; \
open(sys.argv[1], 'w').write( \
'using sysbus\\nmach create\\n' \
'machine LoadPlatformDescription @platforms/boards/stm32f4_discovery.repl\\n' \
'sysbus LoadELF @' + sys.argv[2] + '\\n' \
'sysbus LoadBinary @' + sys.argv[3] + ' 0x08011000\\n' \
'showAnalyzer sysbus.usart2\\nstart\\n')" \
$(RENODE_SCRIPT) $(BL_ELF) $(APP_SIGNED)
	@echo "Wrote $(RENODE_SCRIPT). Launch with:  renode $(RENODE_SCRIPT)"

# =====================================================================
# Tests
# =====================================================================
.PHONY: test
test:
	$(PYTHON) tests/run_all.py

# =====================================================================
# Cleanup
# =====================================================================
.PHONY: clean
clean:
	@$(RM_RF) $(BUILD_DIR)

.PHONY: distclean
distclean: clean
	@$(RM_F) $(KEYS_PRIV) keys/public_key.pem $(PUBKEY_H)
