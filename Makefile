# Makefile for STM32F407VETx Bare Metal Project
# Goal: Build an executable binary (.bin) from C sources for the target MCU.

################################################################################
# Toolchain Configuration
################################################################################

# The prefix for the ARM cross-compiler toolchain (e.g., 'arm-none-eabi-')
PREFIX = arm-none-eabi-

# Tools 
CC = $(PREFIX)gcc            # C/C++ Compiler and Linker
OBJCOPY = $(PREFIX)objcopy   # Utility to convert file formats (ELF to BIN/HEX)
OBJDUMP = $(PREFIX)objdump   # Utility to display information about object files
SIZE = $(PREFIX)size         # Utility to list section sizes and total image size

################################################################################
# Project Configuration
################################################################################

# The name of the final output files (e.g., 'blinky.elf', 'blinky.bin')
# Uses the name of the current directory as the project name by default.
TARGET = $(notdir $(CURDIR))

# The linker script file defining memory layout, sections, and entry point.
LDSCRIPT = stm32f407vetx.ld

# Directories containing C source files.
SRC_DIRS = Src Startup

# Generate a list of all C source files recursively from the defined directories.
C_SOURCES = $(wildcard $(addsuffix /*.c,$(SRC_DIRS)))

# Directory to hold all intermediate and final generated files.
BUILD_DIR = build

# Generate a list of object files (.o) that will be created in the build directory.
OBJECTS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SOURCES))

# List of header search paths for the C compiler (-I options).
INCLUDE_DIRS = \
	-IInc \
	-IChip_headers/CMSIS/Include \
	-IChip_headers/CMSIS/Device/ST/STM32F4xx/Include

# C preprocessor definitions (-D options). Define the target device for headers/libraries.
C_DEFS = \
	-DSTM32F407xx

################################################################################
# Compiler and Linker Flags
################################################################################

# MCU-specific flags for STM32F407 (Cortex-M4 with FPU)
# -mcpu=...: Target CPU architecture
# -mthumb: Generate Thumb-2 instruction set code
# -mfpu=...: Target FPU type
# -mfloat-abi=hard: Use hardware floating-point calling convention
MCU_FLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

# C compiler flags (passed to $(CC) during compilation)
# -g3: Generate maximum debugging information
# -O0: Optimization disabled (better for initial debugging)
# -Wall: Enable all common warnings
# -std=gnu11: Use the C11 standard with GNU extensions
# -ffunction-sections, -fdata-sections: Crucial for linker garbage collection (see LDFLAGS)
CFLAGS = $(MCU_FLAGS) -g3 -O0 -Wall -std=gnu11 -fno-inline -DREENTRANT_SYSCALLS_PROVIDED=1 -ffunction-sections -fdata-sections $(INCLUDE_DIRS) $(C_DEFS)

# Linker flags (passed to $(CC) during linking)
# -T $(LDSCRIPT): Specify the custom linker script.
# --specs=nano.specs: Use the compact Newlib-nano C library optimized for embedded systems.
# -nostartfiles: Do not link the standard startup files (we provide 'Startup' code).
# -Wl,--gc-sections: Remove unused functions/data (garbage collection) to minimize image size.
# -Wl,-Map=...: Generate a map file for memory usage and symbol analysis.
# -Wl,--start-group/-Wl,--end-group: Group libraries to resolve circular dependencies.
LDFLAGS = -T $(LDSCRIPT) $(MCU_FLAGS) \
		--specs=nano.specs \
		-nostartfiles \
		-Wl,--gc-sections \
		-Wl,-Map=$(BUILD_DIR)/$(TARGET).map \
		-Wl,--start-group -lc -lgcc -Wl,--end-group


################################################################################
# Build Rules
################################################################################

# The default target: builds the project and generates the final binary file.
.PHONY: all clean load
all: $(BUILD_DIR)/$(TARGET).bin

# Rule to create the build directory if it doesn't exist.
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Rule to link the object files into the final ELF file (executable).
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) | $(BUILD_DIR)
	@echo "Linking $(TARGET).elf..."
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	@echo "----------------"
	@$(SIZE) $@ # Report memory usage (Text/Data/BSS)
	@echo "ELF file created: $@"
	@echo "----------------"

# Rule to convert the ELF file to a raw binary (.bin) for flashing.
$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	@echo "Creating binary..."
	$(OBJCOPY) -O binary $< $@
	@echo "Binary file created: $@"

# Rule to compile C source files into object files.
# This uses a static pattern rule to handle sources in subdirectories correctly.
$(OBJECTS): $(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@) # Ensure subdirectory exists for the object file
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c -o $@ $<

# Rule to flash the binary to the target MCU using OpenOCD.
load: all
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

# Rule to clean all generated files and the build directory.
clean:
	@echo "Cleaning project..."
	rm -rf $(BUILD_DIR)