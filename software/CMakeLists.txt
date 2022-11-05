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

# linux
if("Linux" IN_LIST PLATFORM)
	set(LINUX 1)
endif()

# posix compatible platforms
if(LINUX OR "Macos" IN_LIST PLATFORM OR "FreeBSD" IN_LIST PLATFORM)
	set(POSIX 1)
endif()

# emulator
if("emu" IN_LIST PLATFORM)
	set(EMU 1)
endif()


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


# platform dependent configuration
if(POSIX)
	#message("*** posix")
	# dependencies
	find_package(Boost CONFIG)
	find_package(libusb CONFIG)
	find_package(miniaudio CONFIG)
	find_package(Opus CONFIG)
	find_package(Ogg CONFIG)
	find_package(GTest CONFIG)
	set(LIBRARIES
		Boost::headers
		Boost::filesystem
		libusb::libusb
		miniaudio::miniaudio
		Opus::opus
		Ogg::ogg
		gtest::gtest
	)

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

	# define platform
	add_definitions(-DPLATFORM_POSIX)
endif()
if(LINUX)
	#message("*** linux")
	# dependencies
	find_package(PkgConfig REQUIRED)
	pkg_search_module(BlueZ IMPORTED_TARGET bluez) # install libbluetooth-dev
	if(TARGET PkgConfig::BlueZ)
		set(LIBRARIES ${LIBRARIES} PkgConfig::BlueZ)
	endif()

	# define platform
	add_definitions(-DPLATFORM_LINUX)
endif()
if(EMU)
	#message("*** emu")
	find_package(glfw3 CONFIG)
	set(LIBRARIES ${LIBRARIES}
		glfw
	)

	# define platform
	add_definitions(-DPLATFORM_EMU)
endif()

if("nrf52" IN_LIST PLATFORM)
	# Nordic nRF52
	# https://interrupt.memfault.com/blog/how-to-write-linker-scripts-for-firmware
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${CMAKE_SOURCE_DIR}/system/src/nrf52/system -T${CMAKE_SOURCE_DIR}/board/${BOARD}/link.ld")

	# define platform
	add_definitions(-DPLATFORM_NRF52)
endif()

if("stm32f0" IN_LIST PLATFORM)
	# ST STM32F0
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${CMAKE_SOURCE_DIR}/board/${BOARD}/link.ld")

	# define platform
	add_definitions(-DPLATFORM_STM32F0)
endif()


# compiler dependent configuration
# enable coroutines
# enable concepts
# suppress warning for own s and ms literal operators that are used instad of those in std::chrono
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	# clang
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts -Wno-user-defined-literals")
else()
	# gcc
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines -fconcepts -Wno-literal-suffix")
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
	util/src/Cie1931.cpp
	util/src/Cie1931.hpp
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
	util/src/IsSubclass.hpp
	util/src/LinkedList.hpp
	util/src/optional.hpp
	util/src/Pointer.hpp
	util/src/Queue.hpp
	util/src/Stream.cpp
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
	protocol/src/bt.hpp
	protocol/src/bus.cpp
	protocol/src/bus.hpp
	protocol/src/crc.cpp
	protocol/src/crc.hpp
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
if(POSIX)
	set(CALENDAR system/src/ClockTime.hpp system/src/Calendar.hpp system/src/posix/Calendar.cpp)
	set(FLASH
		system/src/Flash.hpp
		system/src/Flash.cpp
		system/src/posix/FlashImpl.hpp
		system/src/posix/FlashImpl.cpp
	)
	set(LOOP system/src/Loop.hpp system/src/posix/Loop.cpp system/src/posix/Loop2.cpp)
	set(NETWORK system/src/Network.hpp system/src/posix/Network.cpp)
	set(OUTPUT system/src/Output.hpp system/src/posix/Output.cpp system/src/Debug.hpp)
	set(SOUND system/src/Sound.hpp system/src/posix/Sound.cpp)
	set(SPI_MASTER
		system/src/SpiMaster.hpp
		system/src/SpiMaster.cpp
		system/src/posix/SpiMasterImpl.hpp
		system/src/posix/SpiMasterImpl.cpp
	)
	set(STORAGE
		system/src/Storage.hpp
		system/src/Storage.cpp
		system/src/posix/StorageImpl.hpp
		system/src/posix/StorageImpl.cpp
	)
	set(TERMINAL system/src/Terminal.hpp system/src/posix/Terminal.cpp)
	set(TIMER system/src/SystemTime.hpp system/src/Timer.hpp system/src/posix/Timer.cpp)
endif()
if(LINUX)
	if(TARGET PkgConfig::BlueZ)
		set(BLE system/src/Ble.hpp system/src/linux/Ble.cpp)
	endif()
endif()
if(EMU)
	set(BUS_NODE
		system/src/BusNode.hpp
		system/src/BusNode.cpp
		system/src/emu/BusNodeImpl.hpp
		system/src/emu/BusNodeImpl.cpp
	)
	set(BUS_MASTER
		system/src/BusMaster.hpp
		system/src/BusMaster.cpp
		system/src/emu/BusMasterImpl.hpp
		system/src/emu/BusMasterImpl.cpp
	)
	set(INPUT system/src/Input.hpp system/src/emu/Input.hpp system/src/emu/Input.cpp)
	set(LOOP
		system/src/Loop.hpp
		system/src/posix/Loop.hpp
		system/src/posix/Loop.cpp
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
	set(OUTPUT system/src/Output.hpp system/src/emu/Output.cpp system/src/Debug.hpp)
	set(QUADRATURE_DECODER
		system/src/QuadratureDecoder.hpp
		system/src/QuadratureDecoder.cpp
		system/src/emu/QuadratureDecoderImpl.hpp
		system/src/emu/QuadratureDecoderImpl.cpp
	)
	set(RADIO system/src/RadioDefs.hpp system/src/Radio.hpp system/src/emu/Radio.cpp)
	set(RANDOM system/src/Random.hpp system/src/emu/Random.cpp)
	set(SPI_MASTER
		system/src/SpiMaster.hpp
		system/src/SpiMaster.cpp
		system/src/emu/SpiBME680.hpp
		system/src/emu/SpiBME680.cpp
		system/src/emu/SpiMPQ6526.hpp
		system/src/emu/SpiMPQ6526.cpp
		system/src/emu/SpiMR45Vxxx.hpp
		system/src/emu/SpiMR45Vxxx.cpp
		system/src/emu/SpiSSD1309.hpp
		system/src/emu/SpiSSD1309.cpp
	)
	set(USB_DEVICE system/src/UsbDevice.hpp system/src/emu/UsbDevice.cpp)
endif()
if("nrf52" IN_LIST PLATFORM)
	set(BUS_MASTER
		system/src/BusMaster.hpp
		system/src/BusMaster.cpp
		system/src/nrf52/BusMasterImpl.hpp
		system/src/nrf52/BusMasterImpl.cpp
	)
	set(CALENDAR system/src/ClockTime.hpp system/src/Calendar.hpp system/src/nrf52/Calendar.cpp)
	set(FLASH
		system/src/Flash.hpp
		system/src/Flash.cpp
		system/src/nrf52/FlashImpl.hpp
		system/src/nrf52/FlashImpl.cpp
	)
	set(INPUT system/src/Input.hpp system/src/nrf52/Input.cpp)
	set(LOOP
		system/src/Loop.hpp system/src/nrf52/Loop.hpp
		system/src/nrf52/Loop.cpp
		system/src/nrf52/system/gcc_startup_nrf52840.S
		#system/src/nrf52/system/system_nrf52840.c
	)
	set(OUTPUT system/src/Output.hpp system/src/nrf52/Output.cpp system/src/Debug.hpp)
	set(QUADRATURE_DECODER
		system/src/QuadratureDecoder.hpp
		system/src/QuadratureDecoder.cpp
		system/src/nrf52/QuadratureDecoderImpl.hpp
		system/src/nrf52/QuadratureDecoderImpl.cpp
	)
	set(RADIO system/src/RadioDefs.hpp system/src/Radio.hpp system/src/nrf52/Radio.cpp)
	set(RANDOM system/src/Random.hpp system/src/nrf52/Random.cpp)
	set(SPI_MASTER
		system/src/SpiMaster.hpp
		system/src/SpiMaster.cpp
		system/src/nrf52/SpiMasterImpl.hpp
		system/src/nrf52/SpiMasterImpl.cpp
	)
	set(STORAGE
		system/src/Storage.hpp
		system/src/Storage.cpp
	)
	set(TERMINAL system/src/Terminal.hpp system/src/nrf52/Terminal.cpp)
	set(TIMER system/src/SystemTime.hpp system/src/Timer.hpp system/src/nrf52/Timer.cpp)
	set(USB_DEVICE system/src/UsbDevice.hpp system/src/nrf52/UsbDevice.cpp)
endif()
if("stm32f0" IN_LIST PLATFORM)
	set(BUS_NODE
		system/src/BusNode.hpp
		system/src/BusNode.cpp
		system/src/stm32f0/BusNodeImpl.hpp
		system/src/stm32f0/BusNodeImpl.cpp
	)
	set(FLASH
		system/src/Flash.hpp
		system/src/Flash.cpp
		system/src/stm32f0/FlashImpl.hpp
		system/src/stm32f0/FlashImpl.cpp
	)
	set(INPUT system/src/Input.hpp system/src/stm32f0/gpio.hpp system/src/stm32f0/Input.cpp)
	set(LOOP
		system/src/Loop.hpp
		system/src/stm32f0/defs.hpp
		system/src/stm32f0/Loop.hpp
		system/src/stm32f0/Loop.cpp
		system/src/stm32f0/system/startup_stm32f042x6.s
	)
	set(OUTPUT system/src/Output.hpp system/src/stm32f0/Output.cpp system/src/Debug.hpp)
	set(SPI_MASTER
		system/src/SpiMaster.hpp
		system/src/SpiMaster.cpp
		system/src/stm32f0/SpiMasterImpl.hpp
		system/src/stm32f0/SpiMasterImpl.cpp
	)
	set(STORAGE
		system/src/Storage.hpp
		system/src/Storage.cpp
	)
	set(TERMINAL system/src/Terminal.hpp system/src/stm32f0/Terminal.cpp)
	set(TIMER system/src/SystemTime.hpp system/src/Timer.hpp system/src/stm32f0/Timer.cpp)
endif()

set(SYSTEM
	${BLE}
	${BUS_NODE}
	${BUS_MASTER}
	${CALENDAR}
	${FLASH}
	${INPUT}
	${LOOP}
	${NETWORK}
	${OUTPUT}
	${QUADRATURE_DECODER}
	${RADIO}
	${RANDOM}
	${SOUND}
	${SPI_MASTER}
	${STORAGE}
	${TERMINAL}
	${TIMER}
	${USB_DEVICE}
	system/src/FeRamStorage4.cpp
	system/src/FeRamStorage4.hpp
	system/src/FlashStorage.cpp
	system/src/FlashStorage.hpp
)
source_group(system FILES ${SYSTEM})


# base code for nodes (control and gateway)
set(NODE
	node/src/Message.cpp
	node/src/Message.hpp
	node/src/MqttSnClient.cpp
	node/src/MqttSnClient.hpp
	node/src/MqttSnBroker.cpp
	node/src/MqttSnBroker.hpp
	node/src/Subscriber.cpp
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
	control/src/FunctionInterface.cpp
	control/src/FunctionInterface.hpp
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


# tools
# -----
if(POSIX AND NOT EMU)

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
	${TERMINAL}
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
	board/${BOARD}/boardConfig.hpp
	${UTIL}
	${PROTOCOL}
	${LOOP}
	${NETWORK}
	${TERMINAL}
)
target_include_directories(mdnsSniffer
	PRIVATE
	board/${BOARD} # boardConfig.hpp
	util/src
	protocol/src
	system/src
)
target_link_libraries(mdnsSniffer ${LIBRARIES})


endif() # POSIX AND NOT EMU


# wall switch executable
set(SWITCH_BOARDS emuSwitch switch)
if(${BOARD} IN_LIST SWITCH_BOARDS)
	add_executable(switch
		switch/src/main.cpp
		switch/src/appConfig.hpp
		board/${BOARD}/boardConfig.hpp
		${UTIL}
		${PROTOCOL}
		${SYSTEM}
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
if(NOT EMU AND RADIO)
	add_executable(radioDevice
		tools/src/radioDevice.cpp
		tools/src/appConfig.hpp
		board/${BOARD}/boardConfig.hpp
		${UTIL}
		${PROTOCOL}
		${LOOP}
		${OUTPUT}
		${RADIO}
		${RANDOM}
		${TIMER}
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


# system test executables
# -----------------------

function(add_system_test_executable NAME)
	# check if driver is available
	if(system/src/${NAME}.hpp IN_LIST SYSTEM)
		#message("*** Add ${NAME}Test")
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

add_system_test_executable(Ble)
add_system_test_executable(BusMaster)
add_system_test_executable(Calendar)
add_system_test_executable(Flash)
add_system_test_executable(Input)
add_system_test_executable(Network)
add_system_test_executable(QuadratureDecoder)
add_system_test_executable(Radio)
add_system_test_executable(Random)
add_system_test_executable(Sound)
add_system_test_executable(SpiMaster)
add_system_test_executable(Storage)
add_system_test_executable(Timer)
add_system_test_executable(UsbDevice)


if(POSIX AND NOT EMU)

# system/usb test host, host for UsbTest on device (command line tool)
add_executable(UsbTestHost
	system/test/UsbTestHost.cpp
	${UTIL}
	${PROTOCOL}
	${TERMINAL}
)
target_include_directories(UsbTestHost
	PRIVATE
	util/src
	protocol/src
	system/src
)
target_link_libraries(UsbTestHost ${LIBRARIES})


# node/MqttSnClient test
add_executable(MqttSnClientTest
	node/test/MqttSnClientTest.cpp
	control/src/appConfig.hpp
	board/${BOARD}/boardConfig.hpp
	#${NODE}
	node/src/MqttSnClient.cpp
	node/src/MqttSnClient.hpp
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
target_link_libraries(MqttSnClientTest ${LIBRARIES})


# node/MqttSnBroker test
add_executable(MqttSnBrokerTest
	node/test/MqttSnBrokerTest.cpp
	control/src/appConfig.hpp
	board/${BOARD}/boardConfig.hpp
	#${NODE}
	node/src/MqttSnBroker.cpp
	node/src/MqttSnBroker.hpp
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
target_link_libraries(MqttSnBrokerTest ${LIBRARIES})

endif() # POSIX AND NOT EMU


if(${BOARD} STREQUAL "emuControl")

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


endif() # ${BOARD} STREQUAL "emuControl"


# unit tests (GTest)
# ------------------
if(POSIX AND NOT EMU)

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

# node test
add_executable(nodeTest
	node/test/nodeTest.cpp
	${UTIL}
	node/src/Message.cpp
	node/src/Message.hpp
)
target_include_directories(nodeTest
	PRIVATE
	node/src
	protocol/src
	util/src
)
target_link_libraries(nodeTest ${LIBRARIES})
add_test(NAME nodeTest
	COMMAND nodeTest --gtest_output=xml:report.xml
	#WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../testdata
)

endif() # POSIX AND NOT EMU
