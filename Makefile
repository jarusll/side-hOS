CC := gcc
LD := ld

SRC := src
BUILD_DIR := build

# User controllable C flags.
CFLAGS := -ggdb -O0 -pipe

# User controllable linker flags. We set none by default.
LDFLAGS :=

# Internal C flags that should not be changed by the user.
override CFLAGS += \
    -Wall \
    -Wextra \
    -std=gnu11 \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -fno-PIC \
    -ffunction-sections \
    -fdata-sections \
    -m64 \
    -march=x86-64 \
    -mabi=sysv \
    -mno-80387 \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=kernel \

# Internal linker flags that should not be changed by the user.
override LDFLAGS += \
    -m elf_x86_64 \
    -nostdlib \
    -static \
    -z max-page-size=0x1000 \
    --gc-sections \
    -T linker.lds

# Virt flags
QEMU_FLAGS := -m 1G -cdrom $(BUILD_DIR)/side-hOS.iso \
    -serial stdio \
    -display none \
    -smp 2
#     -no-reboot \
#     -d int,cpu_reset

# Default target. This must come first, before header dependencies.
.PHONY: all
all: $(BUILD_DIR)/kernel.elf

# Link rules for the final executable.
$(BUILD_DIR)/kernel.elf: $(BUILD_DIR)/kernel.o $(BUILD_DIR)/serial.o
	mkdir -p "$(dir $@)"
	$(LD) $(LDFLAGS) $^ -o $@

# Compilation rules for *.c files.
$(BUILD_DIR)/kernel.o: $(SRC)/kernel.c
	mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/serial.o: $(SRC)/serial.c
	mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -c $< -o $@

# Remove object files and the final executable.
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: iso
iso: all
	cd $(dir $(lastword $(MAKEFILE_LIST))) && ./scripts/make-iso.sh

.PHONY: boot
boot: iso
	qemu-system-x86_64 $(QEMU_FLAGS)

.PHONY: debug
debug: iso
	qemu-system-x86_64 $(QEMU_FLAGS) \
        -s -S
