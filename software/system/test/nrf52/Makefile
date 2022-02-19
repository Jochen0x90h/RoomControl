PROJECT_NAME     := SystemTest
TARGETS          := timerTest calendarTest spiTest displayTest busTest radioSendTest usbTest potiTest rngTest
OUTPUT_DIRECTORY := build
UTIL_PATH        := ../../../util/src
SYSTEM_PATH      := ../../src
TEMPLATE_PATH    := $(SYSTEM_PATH)/nrf52/system
#VERBOSE          := 1


# Include folders
INC_FOLDERS += .. ../../../board/nrf52Dongle $(SYSTEM_PATH) ../../../protocol/src $(UTIL_PATH) 

# Source files common to all targets
SRC_FILES += $(TEMPLATE_PATH)/gcc_startup_nrf52840.S
SRC_FILES += $(TEMPLATE_PATH)/system_nrf52840.c
SRC_FILES += $(SYSTEM_PATH)/nrf52/Loop.cpp
SRC_FILES += $(SYSTEM_PATH)/nrf52/Output.cpp

# Target specific source files
SRC_FILES_timerTest += $(SYSTEM_PATH)/nrf52/Timer.cpp
SRC_FILES_timerTest += ../TimerTest.cpp
SRC_FILES_calendarTest += $(SYSTEM_PATH)/nrf52/Calendar.cpp
SRC_FILES_calendarTest += ../CalendarTest.cpp
SRC_FILES_spiTest += ../SpiTest.cpp
SRC_FILES_spiTest += $(SYSTEM_PATH)/nrf52/Spi.cpp
SRC_FILES_displayTest += ../DisplayTest.cpp
SRC_FILES_displayTest += $(SYSTEM_PATH)/nrf52/Spi.cpp
#SRC_FILES_audioTest += ../SudioTest.cpp
#SRC_FILES_audioTest += $(SYSTEM_PATH)/nrf52/Timer.cpp
#SRC_FILES_audioTest += $(SYSTEM_PATH)/nrf52/Audio.cpp
SRC_FILES_busTest += ../BusMasterTest.cpp
SRC_FILES_busTest += $(SYSTEM_PATH)/nrf52/Timer.cpp
SRC_FILES_busTest += $(SYSTEM_PATH)/nrf52/BusMaster.cpp
SRC_FILES_usbTest += ../UsbTest.cpp
SRC_FILES_usbTest += $(SYSTEM_PATH)/nrf52/Usb.cpp
SRC_FILES_radioSendTest += ../RadioSendTest.cpp
SRC_FILES_radioSendTest += $(SYSTEM_PATH)/nrf52/Timer.cpp
SRC_FILES_radioSendTest += $(SYSTEM_PATH)/nrf52/Random.cpp
SRC_FILES_radioSendTest += $(SYSTEM_PATH)/nrf52/Radio.cpp
SRC_FILES_potiTest += ../PotiTest.cpp
SRC_FILES_potiTest += $(SYSTEM_PATH)/nrf52/Timer.cpp
SRC_FILES_potiTest += $(SYSTEM_PATH)/nrf52/Poti.cpp
SRC_FILES_potiTest += $(SYSTEM_PATH)/nrf52/Input.cpp
SRC_FILES_rngTest += ../RandomTest.cpp
SRC_FILES_rngTest += $(SYSTEM_PATH)/nrf52/Timer.cpp
SRC_FILES_rngTest += $(SYSTEM_PATH)/nrf52/Usb.cpp
SRC_FILES_rngTest += $(SYSTEM_PATH)/nrf52/Random.cpp
#SRC_FILES_motionDetectorTest += ../MotionDetectorTest.cpp
#SRC_FILES_motionDetectorTest += $(SYSTEM_PATH)/nrf52/MotionDetector.cpp

# Libraries
LIB_FILES +=
LIB_FILES += -lc -lnosys -lm -lstdc++ # add standard libs at end so that their symbols get found

# Linker script
LINKER_SCRIPT := $(SYSTEM_PATH)/nrf52/link.ld

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
CXXFLAGS += -Wno-literal-suffix -fcoroutines -fconcepts

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
default: $(TARGETS)

# Print all targets that can be built
help:
	@echo following targets are available:
	@echo $(TARGETS)

include $(TEMPLATE_PATH)/Makefile.common

$(foreach target, $(TARGETS), $(call define_target, $(target)))
