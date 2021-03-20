PROJECT_NAME     := FunctionTest
TARGETS          := control
OUTPUT_DIRECTORY := build
TEMPLATE_PATH    := nrf
#VERBOSE          := 1


# Include folders
INC_FOLDERS += nrf system .

# Source files
SRC_FILES += $(TEMPLATE_PATH)/gcc_startup_nrf52840.S
SRC_FILES += $(TEMPLATE_PATH)/system_nrf52840.c

# Target specific source files
SRC_FILES_control += main.cpp
SRC_FILES_control += system/clock.cpp
SRC_FILES_control += system/poti.cpp
SRC_FILES_control += system/spi.cpp
SRC_FILES_control += system/system.cpp

# Libraries
LIB_FILES +=
LIB_FILES += -lc -lnosys -lm -lstdc++ # add standard libs at end so that their symbols get found

# Linker script
LINKER_SCRIPT := link.ld

# C flags common to all targets
#CFLAGS += -Og -g3 # debug
CFLAGS += -O3 -g3 # release
CFLAGS += -mcpu=cortex-m4
CFLAGS += -mthumb -mabi=aapcs
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# keep every function in a separate section, this allows linker to discard unused ones
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin -fshort-enums
CFLAGS += -D__HEAP_SIZE=8192
CFLAGS += -D__STACK_SIZE=8192
CFLAGS += -Wall

# C++ flags (C++ also uses CFLAGS)
CXXFLAGS += -Wno-literal-suffix

# Assembler flags
ASMFLAGS += -g3
ASMFLAGS += -mcpu=cortex-m4
ASMFLAGS += -mthumb -mabi=aapcs
ASMFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
ASMFLAGS += -D__HEAP_SIZE=8192
ASMFLAGS += -D__STACK_SIZE=8192

# Linker flags
#LDFLAGS += -flto # link time optimization
LDFLAGS += -mthumb -mabi=aapcs -L$(TEMPLATE_PATH) -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m4
LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
LDFLAGS += -Wl,--gc-sections # let linker dump unused sections
LDFLAGS += --specs=nano.specs # use newlib in nano version



.PHONY: default help

# Default target - first one defined
default: control

# Print all targets that can be built
help:
	@echo following targets are available:
	@echo control


include $(TEMPLATE_PATH)/Makefile.common

$(foreach target, $(TARGETS), $(call define_target, $(target)))
