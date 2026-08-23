# ---------------------------------------------------------------------------
# PERDICAN V1 firmware — STM32G431CBT6
# ---------------------------------------------------------------------------

TARGET  := perdican

PREFIX  := arm-none-eabi-
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump
SIZE    := $(PREFIX)size
GDB     := $(PREFIX)gdb

BUILD   := build

SRCS := \
  src/startup.c \
  src/board.c \
  src/clock.c \
  src/systick.c \
  src/i2c.c \
  src/lsm6ds3.c \
  src/dfu.c \
  src/retarget.c \
  src/console.c \
  src/main.c \
  src/usb/usb_device.c \
  src/usb/usb_desc.c \
  src/usb/usb_cdc.c

INCLUDES := -Iinclude -Isrc -Isrc/usb -Icmsis/core -Icmsis/device

# STM32G431CBT6: Cortex-M4 with single-precision FPU.
CPUFLAGS := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

DEFS := -DSTM32G431xx

CFLAGS := $(CPUFLAGS) $(DEFS) $(INCLUDES) \
  -std=gnu11 -O2 -g3 \
  -Wall -Wextra -Wshadow -Wundef -Wdouble-promotion \
  -ffunction-sections -fdata-sections -fno-common \
  -fsingle-precision-constant \
  -MMD -MP

LDSCRIPT := linker/stm32g431cb.ld

LDFLAGS := $(CPUFLAGS) -T$(LDSCRIPT) \
  --specs=nano.specs -nostartfiles \
  -Wl,--gc-sections \
  -Wl,-Map=$(BUILD)/$(TARGET).map,--cref \
  -Wl,--print-memory-usage \
  -u _printf_float

OBJS := $(SRCS:%.c=$(BUILD)/%.o)
DEPS := $(OBJS:.o=.d)

.PHONY: all clean size flash dfu dfu-reset openocd gdb disasm

all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).bin $(BUILD)/$(TARGET).hex size

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/$(TARGET).bin: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/$(TARGET).hex: $(BUILD)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

size: $(BUILD)/$(TARGET).elf
	@$(SIZE) $<

disasm: $(BUILD)/$(TARGET).elf
	$(OBJDUMP) -d -S $< > $(BUILD)/$(TARGET).lst
	@echo "wrote $(BUILD)/$(TARGET).lst"

# --- Flashing -------------------------------------------------------------
# DFU needs no debugger: hold BOOT, tap RESET, release BOOT, then `make dfu`.
# The firmware also exposes a 1200-baud-touch reset, so `make dfu-reset dfu`
# reflashes a running board over the same USB-C cable.
dfu: $(BUILD)/$(TARGET).bin
	dfu-util -a 0 -d 0483:df11 -s 0x08000000:leave -D $<

dfu-reset:
	@tools/dfu-touch.sh

# SWD over the 4-pin DEBUG header.
openocd:
	openocd -f interface/stlink.cfg -f target/stm32g4x.cfg

flash: $(BUILD)/$(TARGET).elf
	openocd -f interface/stlink.cfg -f target/stm32g4x.cfg \
	  -c "program $< verify reset exit"

gdb: $(BUILD)/$(TARGET).elf
	$(GDB) $< -ex "target extended-remote :3333"

clean:
	rm -rf $(BUILD)

-include $(DEPS)
