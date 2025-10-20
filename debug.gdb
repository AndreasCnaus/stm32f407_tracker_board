#  arm-none-eabi-gdb .\build\stm32f407_tracker_board.elf -x .\debug.gdb
# Connect to OpenOCD
target remote localhost:3333

# Reset and halt the MCU
monitor reset halt

# Load the firmware
load

# Set breakpoints
#break main


# Display breakpoints
#info breakpoints

# Continue execution
#continue
