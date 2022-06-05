cmake_minimum_required(VERSION 3.15)

# CLion: use conan plugin and add -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake to the CMake options

# set project
project(RoomControl)

# configure languages
enable_language(C CXX ASM)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)

# debug generated makefile
#set(CMAKE_VERBOSE_MAKEFILE TRUE)

message("*** TYPE ${CMAKE_BUILD_TYPE}")
message("*** PLATFORM ${PLATFORM}")
message("*** BOARD ${BOARD}")
message("*** MCU ${MCU}")
message("*** CPU ${CPU}")
message("*** FPU ${FPU}")


# cpu dependent configuration
if(${CPU} MATCHES "cortex")
	# cpu flags
	set(CPU_FLAGS "-mcpu=${CPU} -mthumb -mabi=aapcs")

	# fpu flags
	if(${FPU} STREQUAL "None")
		set(FPU_FLAGS "-mfloat-abi=soft")
	else()
		set(FPU_FLAGS "-mfpu=${FPU} -mfloat-abi=hard")
	endif()

	# keep every function in a separate section, this allows linker to discard unused ones
	set(C_FLAGS "-D__STACK_SIZE=8192 -D__HEAP_SIZE=8192 -fshort-enums -fno-exceptions -fdata-sections -ffunction-sections -Wall")
	#-fno-builtin
	set(CXX_FLAGS "${C_FLAGS} -fno-rtti -fno-use-cxa-atexit")

	# let linker dump unused sections, use newlib in nano version, add standard libs at end so that their symbols get found
	set(LINKER_FLAGS "-Wl,--gc-sections -specs=nano.specs -specs=nosys.specs")
	#-lstdc++ -lc -lm -lnosys

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CPU_FLAGS} ${FPU_FLAGS} ${C_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CPU_FLAGS} ${FPU_FLAGS} ${CXX_FLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINKER_FLAGS}")
endif()


# platform dependent dependencies
if(${PLATFORM} STREQUAL "Linux" OR ${PLATFORM} STREQUAL "Macos")
	find_package(libusb CONFIG)
	find_package(GTest CONFIG)
	set(LIBRARIES
		libusb::libusb
		gtest::gtest
	)
endif()
if(${PLATFORM} STREQUAL "emu")
	find_package(libusb CONFIG)
	find_package(Boost CONFIG)
	find_package(glfw3 CONFIG)
	find_package(GTest CONFIG)
	set(LIBRARIES
		libusb::libusb
		Boost::headers
		Boost::filesystem
		glfw
		gtest::gtest
	)
endif()


# platform dependent configuration
if(${PLATFORM} STREQUAL "Linux" OR ${PLATFORM} STREQUAL "Macos" OR ${PLATFORM} STREQUAL "emu")
	# posix (linux, osx) or emulator

	# enable address sanitizer
	if(CMAKE_BUILD_TYPE STREQUAL Debug)
		message("Enable Address Sanitizer")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
	endif()

	# set rpath so that shared libraries are searched in ../lib
	if(APPLE)
		set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
	else()
		set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")
	endif()
	set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

	# define emulator flag
	add_definitions(-DEMU)
elseif(${PLATFORM} STREQUAL "nrf52")
	# Nordic nRF52
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${CMAKE_SOURCE_DIR}/system/src/nrf52/system -T${CMAKE_SOURCE_DIR}/board/${BOARD}/link.ld")
elseif(${PLATFORM} STREQUAL "stm32f0")
	# ST STM32F0
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${CMAKE_SOURCE_DIR}/board/${BOARD}/link.ld")
endif()


# compiler dependent configuration
# enable coroutines
# enable concepts
# suppress warning for own s and ms literal operators that are used instad of those in std::chrono
# suppress warning for switch case labels outside of enumeration range (for "flags enum")
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	# clang
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts -Wno-user-defined-literals")
else()
	# gcc
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines -fconcepts -Wno-literal-suffix -Wno-switch")
endif()


# set debug define
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DDEBUG)
endif()

# strip unused dylibs
#add_link_options(-dead_strip_dylibs)


# system independent utilities
set(UTIL
	util/src/Array.hpp
	util/src/ArrayList.hpp
	util/src/assert.hpp
	util/src/BitField.hpp
	util/src/Bitmap.cpp
	util/src/Bitmap.hpp
	util/src/convert.cpp
	util/src/convert.hpp
	util/src/Coroutine.hpp
	util/src/Data.hpp
	util/src/DataQueue.hpp
	util/src/defines.hpp
	util/src/DrawMode.hpp
	util/src/enum.hpp
	util/src/Font.cpp
	util/src/Font.hpp
	util/src/LinkedListNode.hpp
	util/src/optional.hpp
	util/src/Pointer.hpp
	util/src/Queue.hpp
	util/src/Stream.hpp
	util/src/String.hpp
	util/src/StringBuffer.hpp
	util/src/StringHash.hpp
	util/src/StringOperators.hpp
	util/src/StringSet.hpp
	util/src/TopicBuffer.cpp
	util/src/TopicBuffer.hpp
	util/src/util.hpp
)
source_group(util FILES ${UTIL})


# protocol utilities
set(PROTOCOL
	protocol/src/bus.cpp
	protocol/src/bus.hpp
	protocol/src/crypt.cpp
	protocol/src/crypt.hpp
	protocol/src/DataBuffer.hpp
	protocol/src/MessageReader.hpp
	protocol/src/MessageWriter.hpp
	protocol/src/gp.hpp
	protocol/src/hash.cpp
	protocol/src/hash.hpp
	protocol/src/ieee.hpp
	protocol/src/mqttsn.hpp
	protocol/src/Nonce.hpp
	protocol/src/usb.hpp
	protocol/src/zb.cpp
	protocol/src/zb.hpp
)
set(TINYCRYPT
	protocol/src/tinycrypt/aes.h
	protocol/src/tinycrypt/aes_encrypt.c
	protocol/src/tinycrypt/constants.h
	protocol/src/tinycrypt/utils.c
	protocol/src/tinycrypt/utils.h
)
source_group(protocol FILES ${PROTOCOL})
source_group(protocol/tinycrypt FILES ${TINYCRYPT})
list(APPEND PROTOCOL ${TINYCRYPT})


# drivers
if(${PLATFORM} STREQUAL "Linux" OR ${PLATFORM} STREQUAL "Macos")
	set(LOOP system/src/Loop.hpp system/src/posix/Loop.hpp system/src/posix/Loop.cpp)
	set(NETWORK system/src/Network.hpp system/src/posix/Network.cpp)
	set(OUTPUT system/src/Output.hpp system/src/posix/Output.cpp system/src/Debug.hpp)
	set(TERMINAL system/src/Terminal.hpp system/src/posix/Terminal.cpp)
	set(TIMER system/src/SystemTime.hpp system/src/Timer.hpp system/src/posix/Timer.cpp)
elseif(${PLATFORM} STREQUAL "emu")
	set(BUS_NODE system/src/BusNode.hpp system/src/emu/BusNode.cpp)
	set(BUS_MASTER system/src/BusMaster.hpp system/src/emu/BusMaster.cpp)
	set(CALENDAR system/src/ClockTime.hpp system/src/Calendar.hpp system/src/emu/Calendar.cpp)
	set(FLASH system/src/Flash.hpp system/src/emu/Flash.cpp)
	set(INPUT system/src/Input.hpp system/src/emu/Input.hpp system/src/emu/Input.cpp)
	set(LOOP
		system/src/Loop.hpp
		system/src/emu/Loop.hpp
		system/src/emu/Loop.cpp
		system/src/emu/Gui.cpp
		system/src/emu/Gui.hpp
		font/tahoma_8pt.cpp # used by Gui
		font/tahoma_8pt.hpp
		glad/glad.c
		glad/glad.h
		glad/khrplatform.h
	)
	set(NETWORK system/src/Network.hpp system/src/emu/Network.cpp)
	set(OUTPUT system/src/Output.hpp system/src/emu/Output.cpp system/src/Debug.hpp)
	set(POTI system/src/Poti.hpp system/src/emu/Poti.cpp)
	set(RADIO system/src/RadioDefs.hpp system/src/Radio.hpp system/src/emu/Radio.cpp)
	set(RANDOM system/src/Random.hpp system/src/emu/Random.cpp)
	set(SPI system/src/Spi.hpp system/src/emu/Spi.hpp system/src/emu/Spi.cpp)
	set(TERMINAL system/src/Terminal.hpp system/src/posix/Terminal.cpp)
	set(TIMER system/src/SystemTime.hpp system/src/Timer.hpp system/src/emu/Timer.cpp)
	set(USB_DEVICE system/src/UsbDevice.hpp system/src/emu/UsbDevice.cpp)
elseif(${PLATFORM} STREQUAL "nrf52")
	set(BUS_MASTER system/src/BusMaster.hpp system/src/nrf52/BusMaster.cpp)
	set(CALENDAR system/src/ClockTime.hpp system/src/Calendar.hpp system/src/nrf52/Calendar.cpp)
	set(FLASH system/src/Flash.hpp system/src/nrf52/Flash.cpp)
	set(INPUT system/src/Input.hpp system/src/nrf52/Input.cpp)
	set(LOOP
		system/src/Loop.hpp system/src/nrf52/Loop.hpp
		system/src/nrf52/Loop.cpp
		system/src/nrf52/system/gcc_startup_nrf52840.S
		system/src/nrf52/system/system_nrf52840.c
	)
	set(OUTPUT system/src/Output.hpp system/src/nrf52/Output.cpp system/src/Debug.hpp)
	set(POTI system/src/Poti.hpp system/src/nrf52/Poti.cpp)
	set(RADIO system/src/RadioDefs.hpp system/src/Radio.hpp system/src/nrf52/Radio.cpp)
	set(RANDOM system/src/Random.hpp system/src/nrf52/Random.cpp)
	set(SPI system/src/Spi.hpp system/src/nrf52/Spi.cpp)
	set(TERMINAL system/src/Terminal.hpp system/src/nrf52/Terminal.cpp)
	set(TIMER system/src/SystemTime.hpp system/src/Timer.hpp system/src/nrf52/Timer.cpp)
	set(USB_DEVICE system/src/UsbDevice.hpp system/src/nrf52/UsbDevice.cpp)
elseif(${PLATFORM} STREQUAL "stm32f0")
	set(BUS_NODE system/src/BusNode.hpp system/src/stm32f0/BusNode.cpp)
	set(INPUT system/src/Input.hpp system/src/stm32f0/gpio.hpp system/src/stm32f0/Input.cpp)
	set(LOOP
		system/src/Loop.hpp
		system/src/stm32f0/defs.hpp
		system/src/stm32f0/Loop.hpp
		system/src/stm32f0/Loop.cpp
		system/src/stm32f0/system/startup_stm32f042x6.s
	)
	set(OUTPUT system/src/Output.hpp system/src/stm32f0/Output.cpp system/src/Debug.hpp)
	set(SPI system/src/Spi.hpp system/src/stm32f0/Spi.cpp)
	set(TERMINAL system/src/Terminal.hpp system/src/stm32f0/Terminal.cpp)
	set(TIMER system/src/SystemTime.hpp system/src/Timer.hpp system/src/stm32f0/Timer.cpp)
endif()

set(SYSTEM
	${BUS_NODE}
	${BUS_MASTER}
	${CALENDAR}
	${FLASH}
	${INPUT}
	${LOOP}
	${NETWORK}
	${OUTPUT}
	${POTI}
	${RADIO}
	${RANDOM}
	${SPI}
	${TERMINAL}
	${TIMER}
	${USB_DEVICE}
)
source_group(system FILES ${SYSTEM})

## system files and drivers
#set(SYSTEM
#	system/src/ClockTime.hpp
#	system/src/Debug.hpp
#	system/src/RadioDefs.hpp
#	system/src/SystemTime.hpp
#)
#macro(add_drivers)
#	foreach(NAME ${ARGV})
#		if(EXISTS ${CMAKE_SOURCE_DIR}/system/src/${PLATFORM}/${NAME}.cpp)
#			list(APPEND SYSTEM system/src/${NAME}.hpp)
#			if(EXISTS ${CMAKE_SOURCE_DIR}/system/src/${PLATFORM}/${NAME}.hpp)
#				list(APPEND SYSTEM system/src/${PLATFORM}/${NAME}.hpp)
#			endif()
#			list(APPEND SYSTEM system/src/${PLATFORM}/${NAME}.cpp)
#			list(APPEND DRIVERS ${NAME})
#		endif()
#	endforeach()
#endmacro()
#add_drivers(
#	BusMaster
#	BusNode
#	Calendar
#	ClockTime
#	Debug
#	Flash
#	Input
#	Loop
#	Network
#	Output
#	Poti
#	Radio
#	RadioDefs
#	Random
#	Spi
#	SystemTime
#	Terminal
#	Timer
#	UsbDevice
#)
#if(${PLATFORM} STREQUAL "emu")
#	list(APPEND SYSTEM
#		system/src/emu/Gui.cpp
#		system/src/emu/Gui.hpp
#		font/tahoma_8pt.cpp # used by Gui
#		font/tahoma_8pt.hpp
#	)
#endif()
#if (${PLATFORM} STREQUAL "nrf52")
#	list(APPEND SYSTEM
#		system/src/nrf52/system/gcc_startup_nrf52840.S
#		system/src/nrf52/system/system_nrf52840.c
#	)
#endif()
#if (${PLATFORM} STREQUAL "stm32f0")
#	list(APPEND SYSTEM
#		system/src/stm32f0/system/startup_stm32f042x6.s
#	)
#endif()
#source_group(system FILES ${SYSTEM})
#
#
## OpenGL support for emulator
#if(${PLATFORM} STREQUAL "emu")
#	set(GLAD
#		glad/glad.c
#		glad/glad.h
#		glad/khrplatform.h
#	)
#	source_group(glad FILES ${GLAD})
#	list(APPEND SYSTEM ${GLAD})
#endif()


# base code for nodes (control and gateway)
set(NODE
	node/src/Message.cpp
	node/src/Message.hpp
	node/src/MqttSnClient.cpp
	node/src/MqttSnClient.hpp
	node/src/MqttSnBroker.cpp
	node/src/MqttSnBroker.hpp
	node/src/Publisher.hpp
	node/src/State.cpp
	node/src/State.hpp
	node/src/Storage.cpp
	node/src/Storage.hpp
	node/src/Subscriber.hpp
)
source_group(node FILES ${NODE})


# room control
set(CONTROL
	control/src/AlarmInterface.cpp
	control/src/AlarmInterface.hpp
	control/src/BME680.cpp
	control/src/BME680.hpp
	control/src/BusInterface.cpp
	control/src/BusInterface.hpp
	control/src/Configuration.hpp
	control/src/Interface.cpp
	control/src/Interface.hpp
	control/src/LocalInterface.cpp
	control/src/LocalInterface.hpp
	control/src/Menu.cpp
	control/src/Menu.hpp
	control/src/RadioInterface.cpp
	control/src/RadioInterface.hpp
	control/src/RoomControl.cpp
	control/src/RoomControl.hpp
	control/src/SSD1309.cpp
	control/src/SSD1309.hpp
	control/src/SwapChain.cpp
	control/src/SwapChain.hpp
)
source_group(control FILES ${CONTROL})


# hex generator
separate_arguments(GENERATE_HEX)
macro(generate_hex TARGET)
	if(NOT "${GENERATE_HEX}" STREQUAL "" AND NOT "${GENERATE_HEX}" STREQUAL "None")
		#message("*** Generate Hex for ${TARGET}: ${GENERATE_HEX}")
		add_custom_command(TARGET ${TARGET}
			POST_BUILD
			COMMAND ${GENERATE_HEX} ${TARGET} ${TARGET}.hex
		)
	endif()
endmacro()


if(${BOARD} STREQUAL "emuControl")

# room control executable
add_executable(control
	control/src/main.cpp
	control/src/appConfig.hpp
	board/${BOARD}/boardConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
	${NODE}
	${CONTROL}
)
target_include_directories(control
	PRIVATE
	board/${BOARD} # boardConfig.hpp
	font
	control/src
	node/src
	system/src
	protocol/src
	util/src
	glad
)
target_link_libraries(control ${LIBRARIES})

# install the room control
install(TARGETS control
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
)

endif() # ${BOARD} STREQUAL "emuControl"

if(${PLATFORM} STREQUAL "Linux" OR ${PLATFORM} STREQUAL "Macos")

# tools
# -----

# terminal for tests that output text or binary data via usb
add_executable(terminal
	tools/src/terminal.cpp
)
target_include_directories(terminal
	PRIVATE
	util/src
	protocol/src
	system/src
)
target_link_libraries(terminal ${LIBRARIES})


# IEEE 802.15.4 radio packet sniffer that can write .pcap files and analyze packets (needs radioDevice on device)
add_executable(ieeeSniffer
	tools/src/ieeeSniffer.cpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
)
target_include_directories(ieeeSniffer
	PRIVATE
	util/src
	protocol/src
	system/src
)
target_link_libraries(ieeeSniffer ${LIBRARIES})


# sniffer for mDNS multicast udp packets that can write .pcap files
add_executable(mdnsSniffer
	tools/src/mdnsSniffer.cpp
	${UTIL}
	${PROTOCOL}
	${LOOP}
	${NETWORK}
	${TERMINAL}
)
target_include_directories(mdnsSniffer
	PRIVATE
	util/src
	protocol/src
	system/src
)
target_link_libraries(mdnsSniffer ${LIBRARIES})


endif() # ${PLATFORM} STREQUAL "Linux"


# wall switch executable
set(SWITCH_BOARDS emuSwitch canable)
if(${BOARD} IN_LIST SWITCH_BOARDS)
	add_executable(switch
		switch/src/main.cpp
		switch/src/appConfig.hpp
		board/${BOARD}/boardConfig.hpp
		${UTIL}
		${PROTOCOL}
		${LOOP}
		${TIMER}
		${INPUT}
		${OUTPUT}
		${SPI}
	)
	target_include_directories(switch
		PRIVATE
		board/${BOARD} # boardConfig.hpp
		font
		switch/src # appConfig.hpp
		system/src
		protocol/src
		util/src
		glad
	)
	target_link_libraries(switch ${LIBRARIES})
	generate_hex(switch)
endif()


# radio device that is used by the sniffer and radio driver of the emulator
if(NOT ${PLATFORM} STREQUAL "emu" AND RADIO)
	add_executable(radioDevice
		tools/src/radioDevice.cpp
		tools/src/appConfig.hpp
		board/${BOARD}/boardConfig.hpp
		${UTIL}
		${PROTOCOL}
		${LOOP}
		${TIMER}
		${RADIO}
		${USB_DEVICE}
	)
	target_include_directories(radioDevice
		PRIVATE
		font
		board/${BOARD} # boardConfig.hpp
		tools/src
		system/src
		protocol/src
		util/src
		glad
	)
	target_link_libraries(radioDevice ${LIBRARIES})
	generate_hex(radioDevice)
endif()


#if(NOT ${BOARD} STREQUAL "None")

# system test executables
# -----------------------

function(add_system_test_executable NAME)
	# check if driver is available
	if(system/src/${NAME}.hpp IN_LIST SYSTEM)
		message("*** Add ${NAME}Test")
		# add test executable
		add_executable(${NAME}Test
			system/test/${NAME}Test.cpp
			system/test/appConfig.hpp
			board/${BOARD}/boardConfig.hpp
			${UTIL}
			${PROTOCOL}
			${SYSTEM}
		)
		target_include_directories(${NAME}Test
			PRIVATE
			font
			board/${BOARD} # boardConfig.hpp
			system/test # appConfig.hpp
			system/src
			protocol/src
			util/src
			glad
		)
		target_link_libraries(${NAME}Test ${LIBRARIES})
		generate_hex(${NAME}Test)
	endif()
endfunction()

add_system_test_executable(BusMaster)
add_system_test_executable(Calendar)
add_system_test_executable(Input)
add_system_test_executable(Network)
add_system_test_executable(Poti)
add_system_test_executable(Radio)
add_system_test_executable(Random)
add_system_test_executable(Spi)
add_system_test_executable(Timer)
add_system_test_executable(UsbDevice)

#endif() # NOT ${BOARD} STREQUAL "None"

if(${BOARD} STREQUAL "emuControl")

# system/usb test host, host for UsbTest on device (command line tool)
add_executable(UsbTestHost
	system/test/UsbTestHost.cpp
)
target_include_directories(UsbTestHost
	PRIVATE
	protocol/src
	util/src
)
target_link_libraries(UsbTestHost ${LIBRARIES})


# node/MqttSnClient test
add_executable(MqttSnClientTest
	node/test/MqttSnClientTest.cpp
	control/src/appConfig.hpp
	board/${BOARD}/boardConfig.hpp
	${NODE}
	${PROTOCOL}
	${SYSTEM}
	${UTIL}
)
target_include_directories(MqttSnClientTest
	PRIVATE
	board/${BOARD} # boardConfig.hpp
	control/src # appConfig.hpp
	font
	node/test
	node/src
	system/src
	protocol/src
	util/src
	glad
)
#conan_target_link_libraries(MqttSnClientTest)
target_link_libraries(MqttSnClientTest ${LIBRARIES})


# node/MqttSnBroker test
add_executable(MqttSnBrokerTest
	node/test/MqttSnBrokerTest.cpp
	control/src/appConfig.hpp
	board/${BOARD}/boardConfig.hpp
	${NODE}
	${PROTOCOL}
	${SYSTEM}
	${UTIL}
)
target_include_directories(MqttSnBrokerTest
	PRIVATE
	board/${BOARD} # boardConfig.hpp
	control/src # appConfig.hpp
	font
	node/test
	node/src
	system/src
	protocol/src
	util/src
	glad
)
#conan_target_link_libraries(MqttSnBrokerTest)
target_link_libraries(MqttSnBrokerTest ${LIBRARIES})


# test for BME680 air sensor
add_executable(BME680Test
	control/test/BME680Test.cpp
	board/${BOARD}/boardConfig.hpp
	control/src/appConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
	control/src/BME680.cpp
	control/src/BME680.hpp
	font
)
target_include_directories(BME680Test
	PRIVATE
	board/${BOARD} # boardConfig.hpp
	font
	control/src
	system/src
	protocol/src
	util/src
	glad
)
target_link_libraries(BME680Test ${LIBRARIES})


# test for SSD1309 display
add_executable(SSD1309Test
	control/test/SSD1309Test.cpp
	board/${BOARD}/boardConfig.hpp
	control/src/appConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
	control/src/SSD1309.cpp
	control/src/SSD1309.hpp
)
target_include_directories(SSD1309Test
	PRIVATE
	board/${BOARD} # boardConfig.hpp
	font
	control/src
	system/src
	protocol/src
	util/src
	glad
)
target_link_libraries(SSD1309Test ${LIBRARIES})


# test for Storage class
add_executable(StorageTest
	control/test/StorageTest.cpp
	board/${BOARD}/boardConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
	node/src/State.cpp
	node/src/State.hpp
	node/src/Storage.cpp
	node/src/Storage.hpp
	system/src/emu/Flash.cpp
)
target_include_directories(StorageTest
	PRIVATE
	board/${BOARD} # boardConfig.hpp
	font
	control/src
	node/src
	system/src
	protocol/src
	util/src
	glad
)
target_link_libraries(StorageTest ${LIBRARIES})


# unit tests (GTest)
# ------------------

# enable testing, adds a target to run all unit tests
enable_testing()

# util test
add_executable(utilTest
	util/test/utilTest.cpp
	${UTIL}
)
target_include_directories(utilTest
	PRIVATE
	util/src
)
target_link_libraries(utilTest ${LIBRARIES})
add_test(NAME utilTest
	COMMAND utilTest --gtest_output=xml:report.xml
	#WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../testdata
)

# protocol test
add_executable(protocolTest
	protocol/test/protocolTest.cpp
	${UTIL}
	${PROTOCOL}
)
target_include_directories(protocolTest
	PRIVATE
	protocol/src
	util/src
)
target_link_libraries(protocolTest ${LIBRARIES})
add_test(NAME protocolTest
	COMMAND protocolTest --gtest_output=xml:report.xml
	#WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../testdata
)

# system test (only helper classes such as SystemTime)
add_executable(systemTest
	system/test/systemTest.cpp
	${UTIL}
)
target_include_directories(systemTest
	PRIVATE
	system/src
	util/src
)
target_link_libraries(systemTest ${LIBRARIES})
add_test(NAME systemTest
	COMMAND systemTest --gtest_output=xml:report.xml
	#WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../testdata
)
endif()
