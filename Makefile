# Microkernel RTOS — GNU Arm Embedded build (single-shot compile + link).

PREFIX ?= arm-none-eabi-
CC := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy

BUILD ?= debug
ifeq ($(BUILD),debug)
  OPTFLAGS := -O0 -g3
else
  OPTFLAGS := -O2 -g
endif

# blinky | mutex_demo | stress_test | test_scheduler | test_mutex | test_priority_inheritance | test_msgqueue
APP ?= blinky

ifeq ($(APP),blinky)
  MAIN_SRC := examples/blinky/main.c
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
else
  $(error Unknown APP=$(APP))
endif

KERNEL_SRCS := \
  platform/stm32f4/startup_stm32f407xx.s \
  src/core/context_switch.s \
  src/platform/system.c \
  src/platform/syscalls.c \
  src/memory/heap.c \
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
  -std=c11 -Wall -Wextra -Wno-unused-parameter \
  -DQEMU_BUILD

LDFLAGS := -T platform/stm32f4/linker.ld \
  -Wl,--gc-sections -Wl,-Map=build/firmware.map \
  --specs=nano.specs --specs=nosys.specs -nostartfiles

.PHONY: all clean flash debug test docs

all: build/firmware.elf build/firmware.bin

build/firmware.elf: $(SRCS) platform/stm32f4/linker.ld Makefile
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -o $@

build/firmware.bin: build/firmware.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf build html latex

flash: all
	qemu-system-arm -M netduino2 -kernel build/firmware.elf -nographic -serial stdio

debug: all
	qemu-system-arm -M netduino2 -kernel build/firmware.elf -s -S -nographic -serial stdio

test:
	$(MAKE) APP=test_scheduler all
	qemu-system-arm -M netduino2 -kernel build/firmware.elf -nographic -serial stdio

docs:
	doxygen Doxyfile
