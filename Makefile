# Cortex-M RTOS — GNU Arm Embedded build (single-shot compile + link).

PREFIX ?= arm-none-eabi-
CC := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
GDB := $(PREFIX)gdb

BUILD ?= debug
ifeq ($(BUILD),debug)
  OPTFLAGS := -O0 -g3
else
  OPTFLAGS := -O2 -g
endif

# qemu (default) | hw
TARGET ?= qemu

# sched_demo | mutex_demo | stress_test | test_* 
APP ?= sched_demo

ifeq ($(APP),sched_demo)
  MAIN_SRC := examples/sched_demo/main.c
else ifeq ($(APP),mutex_demo)
  MAIN_SRC := examples/mutex_demo/main.c
else ifeq ($(APP),stress_test)
  MAIN_SRC := examples/stress_test/main.c
else ifeq ($(APP),test_scheduler)
  MAIN_SRC := tests/test_scheduler.c
else ifeq ($(APP),test_mutex)
  MAIN_SRC := tests/test_mutex.c
else ifeq ($(APP),test_priority_inheritance)
  MAIN_SRC := tests/test_priority_inheritance.c
else ifeq ($(APP),test_msgqueue)
  MAIN_SRC := tests/test_msgqueue.c
else ifeq ($(APP),test_semaphore)
  MAIN_SRC := tests/test_semaphore.c
else ifeq ($(APP),test_timeout)
  MAIN_SRC := tests/test_timeout.c
else
  $(error Unknown APP=$(APP))
endif

KERNEL_SRCS := \
  platform/stm32f4/startup_stm32f407xx.s \
  src/core/context_switch.s \
  src/platform/system.c \
  src/platform/syscalls.c \
  src/memory/heap.c \
  src/core/block.c \
  src/core/scheduler.c \
  src/core/task.c \
  src/timer/sw_timer.c \
  src/sync/mutex.c \
  src/sync/semaphore.c \
  src/ipc/msgqueue.c

SRCS := $(KERNEL_SRCS) $(MAIN_SRC)

CFLAGS := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
  -ffunction-sections -fdata-sections -fno-common \
  $(OPTFLAGS) \
  -Iinclude -Isrc/core -Isrc/sync -Isrc/ipc -Isrc/memory -Isrc/timer \
  -std=c11 -Wall -Wextra -Wno-unused-parameter

ifeq ($(TARGET),qemu)
  CFLAGS += -DQEMU_BUILD
  QEMU_FLAGS := -semihosting-config enable=on,target=native
else ifeq ($(TARGET),hw)
  QEMU_FLAGS :=
else
  $(error Unknown TARGET=$(TARGET))
endif

LDFLAGS := -T platform/stm32f4/linker.ld \
  -Wl,--gc-sections -Wl,-Map=build/firmware.map \
  --specs=nano.specs --specs=nosys.specs -nostartfiles

.PHONY: all clean flash debug test test-all docs flash-hw debug-hw

all: build/firmware.elf build/firmware.bin

build/firmware.elf: $(SRCS) platform/stm32f4/linker.ld Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -o $@

build/firmware.bin: build/firmware.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf build html latex

flash: all
	qemu-system-arm -M netduino2 -kernel build/firmware.elf -nographic $(QEMU_FLAGS)

debug: all
	qemu-system-arm -M netduino2 -kernel build/firmware.elf -s -S -nographic $(QEMU_FLAGS)

test:
	$(MAKE) APP=test_scheduler all
	qemu-system-arm -M netduino2 -kernel build/firmware.elf -nographic $(QEMU_FLAGS)

test-all:
	python3 scripts/run_tests.py

docs:
	doxygen Doxyfile

flash-hw: all
	@echo "Flash with: st-flash write build/firmware.bin 0x08000000"
	st-flash write build/firmware.bin 0x08000000

debug-hw: all
	@echo "Connect OpenOCD, then: $(GDB) build/firmware.elf"
