cmake_minimum_required(VERSION 3.9)

# set project
project(RoomControl)

# enable c, c++ and asm
enable_language(C CXX ASM)

# debug generated makefile
#set(CMAKE_VERBOSE_MAKEFILE TRUE)

# use C99 and C++20 standards
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)

separate_arguments(GENERATE_HEX)

message("*** BOARD ${BOARD}")
message("*** FAMILY ${FAMILY}")
message("*** CPU ${CPU}")
message("*** FPU ${FPU}")

# CLion: use conan plugin and add -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake to the CMake options

if(${CPU} MATCHES "cortex")
	# cpu flags
	set(CPU_FLAGS "-mcpu=${CPU} -mthumb -mabi=aapcs")

	# fpu flags
	set(FPU_FLAGS "-mfpu=${FPU} -mfloat-abi=hard")

	# keep every function in a separate section, this allows linker to discard unused ones
	set(COMPILER_FLAGS "-D__STACK_SIZE=8192 -D__HEAP_SIZE=8192 -fno-builtin -fshort-enums -fdata-sections -ffunction-sections -Wall")

	# let linker dump unused sections, use newlib in nano version, add standard libs at end so that their symbols get found
	set(LINKER_FLAGS "-Wl,--gc-sections -specs=nano.specs -lc -lm -lnosys -lstdc++")

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CPU_FLAGS} ${FPU_FLAGS} ${COMPILER_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CPU_FLAGS} ${FPU_FLAGS} ${COMPILER_FLAGS}")
	#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CPU_FLAGS} ${FPU_FLAGS} ${LINKER_FLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINKER_FLAGS}")
endif()

if(${BOARD} STREQUAL "emu")
	# dependencies
	find_package(Boost CONFIG)
	find_package(GTest CONFIG)
	find_package(glfw3 CONFIG)
	find_package(libusb CONFIG)

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
endif()
if(${FAMILY} STREQUAL "nrf52")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${CMAKE_SOURCE_DIR}/system/src/nrf52/system -T${CMAKE_SOURCE_DIR}/board/${BOARD}/link.ld")
endif()


# enable coroutines
# enable concepts
# suppress warning for own s and ms literal operators that are used instad of those in std::chrono
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	# clang
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts -Wno-user-defined-literals -ftemplate-backtrace-limit=0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
else()
	# gcc
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines -fconcepts -Wno-literal-suffix")
endif()


# set debug define
add_definitions(-DDEBUG)

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

# system
set(SYSTEM
	system/src/${FAMILY}/BusMaster.cpp
	system/src/BusMaster.hpp
	system/src/${FAMILY}/Calendar.cpp
	system/src/Calendar.hpp
	system/src/ClockTime.hpp
	system/src/Debug.hpp
	system/src/${FAMILY}/Flash.cpp
	system/src/Flash.hpp
	system/src/${FAMILY}/Input.cpp
	#system/src/${FAMILY}/Input.hpp
	system/src/Input.hpp
	system/src/${FAMILY}/Loop.cpp
	system/src/${FAMILY}/Loop.hpp
	system/src/Loop.hpp
	system/src/${FAMILY}/Output.cpp
	system/src/Output.hpp
	system/src/${FAMILY}/Poti.cpp
	system/src/Poti.hpp
	system/src/${FAMILY}/Radio.cpp
	system/src/Radio.hpp
	system/src/RadioDefs.hpp
	system/src/${FAMILY}/Random.cpp
	system/src/Random.hpp
	system/src/${FAMILY}/Spi.cpp
	system/src/Spi.hpp
	system/src/SystemTime.hpp
	system/src/${FAMILY}/Terminal.cpp
	system/src/Terminal.hpp
	system/src/${FAMILY}/Timer.cpp
	system/src/Timer.hpp
	system/src/${FAMILY}/Usb.cpp
	system/src/Usb.hpp
	system/src/UsbDefs.hpp
)
if(${BOARD} STREQUAL "emu")
	list(APPEND SYSTEM
		system/src/${FAMILY}/Network.cpp
		system/src/Network.hpp
		system/src/emu/Gui.cpp
		system/src/emu/Gui.hpp
		font/tahoma_8pt.cpp # used by Gui
		font/tahoma_8pt.hpp
	)
endif()
if (${FAMILY} STREQUAL "nrf52")
	list(APPEND SYSTEM
		system/src/nrf52/system/gcc_startup_nrf52840.S
		system/src/nrf52/system/system_nrf52840.c
	)
endif()
source_group(system FILES ${SYSTEM})

if(${BOARD} STREQUAL "emu")
	# OpenGL support
	set(GLAD
		glad/glad.c
		glad/glad.h
		glad/khrplatform.h
	)
	source_group(glad FILES ${GLAD})
	list(APPEND SYSTEM ${GLAD})
endif()


# base code for nodes (control and gateway)
set(NODE
	node/src/Configuration.hpp
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
	control/src/BME680.cpp
	control/src/BME680.hpp
	control/src/BusInterface.cpp
	control/src/BusInterface.hpp
	control/src/Device.hpp
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


if(${BOARD} STREQUAL "emu")
	set(LIBRARIES
		Boost::headers
		Boost::filesystem
		libusb::libusb
		glfw
		gtest::gtest
	)
else()
	set(LIBRARIES)
endif()


if(${BOARD} STREQUAL "emu")

# room control executable
add_executable(control
	control/src/main.cpp
	control/src/appConfig.hpp
	board/emu/boardConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
	${NODE}
	${CONTROL}
)
target_include_directories(control
	PRIVATE
	board/emu # boardConfig.hpp
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


# tools
# -----


# radio packet sniffer that can write .pcap files and analyze packets (needs radioTest on device)
add_executable(sniffer
	tools/src/sniffer.cpp
	${UTIL}
	${PROTOCOL}
	system/src/Terminal.hpp
	system/src/emu/Terminal.cpp
)
target_include_directories(sniffer
	PRIVATE
	util/src
	protocol/src
	system/src
)
target_link_libraries(sniffer ${LIBRARIES})


# terminal for tests that output text or binary data via usb
add_executable(terminal
	tools/src/terminal.cpp
)
target_include_directories(terminal
	PRIVATE
	util/src
	system/src
)
target_link_libraries(terminal ${LIBRARIES})

endif()

if(NOT ${BOARD} STREQUAL "emu")

# terminal for tests that output text or binary data via usb
add_executable(radioDevice
	tools/src/radioDevice.cpp
	tools/src/appConfig.hpp
	board/${BOARD}/boardConfig.hpp
	#${PROTOCOL}

	#system/src/${FAMILY}/BusMaster.cpp
	#system/src/BusMaster.hpp
	#system/src/${FAMILY}/Calendar.cpp
	#system/src/Calendar.hpp
	system/src/ClockTime.hpp
	system/src/Debug.hpp
	#system/src/${FAMILY}/Flash.cpp
	#system/src/Flash.hpp
	#system/src/${FAMILY}/Input.cpp
	#system/src/${FAMILY}/Input.hpp
	#system/src/Input.hpp
	system/src/${FAMILY}/Loop.cpp
	system/src/${FAMILY}/Loop.hpp
	system/src/Loop.hpp
	system/src/${FAMILY}/Output.cpp
	system/src/Output.hpp
	#system/src/${FAMILY}/Poti.cpp
	#system/src/Poti.hpp
	system/src/${FAMILY}/Radio.cpp
	system/src/Radio.hpp
	system/src/RadioDefs.hpp
	system/src/${FAMILY}/Random.cpp
	system/src/Random.hpp
	#system/src/${FAMILY}/Spi.cpp
	#system/src/Spi.hpp
	system/src/SystemTime.hpp
	#system/src/${FAMILY}/Terminal.cpp
	#system/src/Terminal.hpp
	system/src/${FAMILY}/Timer.cpp
	system/src/Timer.hpp
	system/src/${FAMILY}/Usb.cpp
	system/src/Usb.hpp
	system/src/UsbDefs.hpp
	system/src/nrf52/system/gcc_startup_nrf52840.S
	system/src/nrf52/system/system_nrf52840.c

	#${SYSTEM}
	${UTIL}
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
if(NOT "${GENERATE_HEX}" STREQUAL "")
	add_custom_command(TARGET radioDevice
		POST_BUILD
		COMMAND ${GENERATE_HEX} radioDevice radioDevice.hex
	)
endif()

endif()



# system test executables
# -----------------------

function(add_system_test_executable NAME)
	add_executable(${NAME}
		system/test/${NAME}.cpp
		system/test/appConfig.hpp
		board/${BOARD}/boardConfig.hpp
		${PROTOCOL}
		${SYSTEM}
		${UTIL}
	)
	target_include_directories(${NAME}
		PRIVATE
		font
		board/${BOARD} # boardConfig.hpp
		system/test # appConfig.hpp
		system/src
		protocol/src
		util/src
		glad
	)
	target_link_libraries(${NAME} ${LIBRARIES})
	if(NOT "${GENERATE_HEX}" STREQUAL "")
		add_custom_command(TARGET ${NAME}
			POST_BUILD
			COMMAND ${GENERATE_HEX} ${NAME} ${NAME}.hex
		)
	endif()
endfunction()

# system/Timer test (command line tool)
add_system_test_executable(TimerTest)
add_system_test_executable(PotiTest)
add_system_test_executable(CalendarTest)
add_system_test_executable(RandomTest)
add_system_test_executable(RadioSendTest)
add_system_test_executable(SpiTest)
add_system_test_executable(BusMasterTest)
add_system_test_executable(UsbTest)
if(${BOARD} STREQUAL "emu")
	add_system_test_executable(NetworkTest)
endif()


if(${BOARD} STREQUAL "emu")

# system/usb test host, host for UsbTest on device (command line tool)
add_executable(UsbTestHost
	system/test/UsbTestHost.cpp
)
target_include_directories(UsbTestHost
	PRIVATE
	system/src
	util/src
)
#conan_target_link_libraries(UsbTestHost)
target_link_libraries(UsbTestHost ${LIBRARIES})


# node/MqttSnClient test
add_executable(MqttSnClientTest
	node/test/MqttSnClientTest.cpp
	control/src/appConfig.hpp
	board/emu/boardConfig.hpp
	${NODE}
	${PROTOCOL}
	${SYSTEM}
	${UTIL}
)
target_include_directories(MqttSnClientTest
	PRIVATE
	board/emu # boardConfig.hpp
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
	board/emu/boardConfig.hpp
	${NODE}
	${PROTOCOL}
	${SYSTEM}
	${UTIL}
)
target_include_directories(MqttSnBrokerTest
	PRIVATE
	board/emu # boardConfig.hpp
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
	board/emu/boardConfig.hpp
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
	board/emu # boardConfig.hpp
	font
	control/src
	system/src
	protocol/src
	util/src
	glad
)
#conan_target_link_libraries(BME680Test)
target_link_libraries(BME680Test ${LIBRARIES})


# test for SSD1309 display
add_executable(SSD1309Test
	control/test/SSD1309Test.cpp
	board/emu/boardConfig.hpp
	control/src/appConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
	control/src/SSD1309.cpp
	control/src/SSD1309.hpp
)
target_include_directories(SSD1309Test
	PRIVATE
	board/emu # boardConfig.hpp
	font
	control/src
	system/src
	protocol/src
	util/src
	glad
)
#conan_target_link_libraries(SSD1309Test)
target_link_libraries(SSD1309Test ${LIBRARIES})


# test for Storage class
add_executable(StorageTest
	control/test/StorageTest.cpp
	board/emu/boardConfig.hpp
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
	board/emu # boardConfig.hpp
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
