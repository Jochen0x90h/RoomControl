cmake_minimum_required(VERSION 3.9)

# set project
project(Emulator)

#set(CMAKE_VERBOSE_MAKEFILE ON)

# use C++20 standard
set(CMAKE_CXX_STANDARD 20)

# enable coroutines
# suppress warning for own s and ms literal operators that are used instad of those in std::chrono
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	# clang
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts -Wno-user-defined-literals -ftemplate-backtrace-limit=0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
else()
	# gcc
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines -Wno-literal-suffix")
endif()

# enable address sanitizer
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

# set rpath so that shared libraries are searched in ../lib
if (APPLE)
	set(CMAKE_INSTALL_RPATH "@executable_path/../lib")
else()
	set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")
endif()
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)


# define emulator flag
add_definitions(-DEMU)


# install conan dependencies if not done yet (CLion: use https://github.com/conan-io/conan-clion-plugin)
if(${PROJECT_SOURCE_DIR}/conanfile.py IS_NEWER_THAN ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
	set(PROFILE default)
	if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
		set(PROFILE Debug)
	endif()
	message("install conan dependencies for profile ${PROFILE}")
	execute_process(COMMAND conan install ${PROJECT_SOURCE_DIR}/conanfile.py --profile ${PROFILE} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

# conan dependencies
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS KEEP_RPATHS)

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
	#util/src/Buffer.hpp
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
	protocol/src/crypt.cpp
	protocol/src/crypt.hpp
	protocol/src/DataBuffer.hpp
	#protocol/src/DataReader.hpp
	#protocol/src/DataWriter.hpp
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
	system/src/bus.hpp
	system/src/emu/calendar.cpp
	system/src/calendar.hpp
	system/src/ClockTime.hpp
	system/src/debug.hpp
	system/src/display.hpp
	system/src/emu/flash.cpp
	system/src/flash.hpp
	system/src/emu/Gui.cpp
	system/src/emu/Gui.hpp
	system/src/emu/input.cpp
	system/src/emu/input.hpp
	system/src/input.hpp
	system/src/emu/loop.cpp
	system/src/emu/loop.hpp
	system/src/loop.hpp
	system/src/emu/network.cpp
	system/src/network.hpp
	system/src/emu/output.cpp
	system/src/output.hpp
	system/src/emu/poti.cpp
	system/src/poti.hpp
	system/src/emu/radio.cpp
	system/src/radio.hpp
	system/src/radioDefs.hpp
	system/src/emu/rng.cpp
	system/src/rng.hpp
	system/src/emu/spi.cpp
	system/src/spi.hpp
	system/src/SystemTime.hpp
	system/src/emu/terminal.cpp
	system/src/terminal.hpp
	system/src/emu/timer.cpp
	system/src/timer.hpp
	system/src/emu/usb.cpp
	system/src/usb.hpp
	system/src/usbDefs.hpp
	font/tahoma_8pt.cpp # used by Gui
	font/tahoma_8pt.hpp
)
source_group(system FILES ${SYSTEM})

# OpenGL support
set(GLAD
	glad/glad.c
	glad/glad.h
	glad/khrplatform.h
)
source_group(glad FILES ${GLAD})
list(APPEND SYSTEM ${GLAD})


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

# room control software running on the nRF52840
set(CONTROL
	control/src/BME680.cpp
	control/src/BME680.hpp
	control/src/BusInterface.cpp
	control/src/BusInterface.hpp
	control/src/emu/bus.cpp
	control/src/Device.hpp
	control/src/emu/display.cpp
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


# emulator executable
add_executable(emulator
	control/src/main.cpp
	control/src/appConfig.hpp
	board/emu/boardConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
	${NODE}
	${CONTROL}
)
target_include_directories(emulator
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
conan_target_link_libraries(emulator)

# install the emulator
install(TARGETS emulator
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
)


# tools
# -----

# terminal for tests that output text or binary data via usb
add_executable(terminal
	tools/src/terminal.cpp
)
target_include_directories(terminal
	PRIVATE
	util/src
	system/src
)
conan_target_link_libraries(terminal)



# radio packet sniffer that can write .pcap files and analyze packets (needs radioTest on device)
add_executable(sniffer
	tools/src/sniffer.cpp
	${UTIL}
	${PROTOCOL}
	system/src/terminal.hpp
	system/src/emu/terminal.cpp
)
target_include_directories(sniffer
	PRIVATE
	util/src
	protocol/src
	system/src
)
conan_target_link_libraries(sniffer)


# test executables
# ----------------

# system/timer test (command line tool)
add_executable(timerTest
	system/test/timerTest.cpp
	system/test/appConfig.hpp
	board/emu/boardConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
)
target_include_directories(timerTest
	PRIVATE
	font
	board/emu # boardConfig.hpp
	system/test # appConfig.hpp
	system/src
	protocol/src
	util/src
	glad
)
conan_target_link_libraries(timerTest)


# system/poti test (command line tool)
add_executable(potiTest
	system/test/potiTest.cpp
	system/test/appConfig.hpp
	board/emu/boardConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
)
target_include_directories(potiTest
	PRIVATE
	font
	board/emu # boardConfig.hpp
	system/test # appConfig.hpp
	system/src
	protocol/src
	util/src
	glad
)
conan_target_link_libraries(potiTest)


# system/calendar test (command line tool)
add_executable(calendarTest
	system/test/calendarTest.cpp
	system/test/appConfig.hpp
	board/emu/boardConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
)
target_include_directories(calendarTest
	PRIVATE
	font
	board/emu # boardConfig.hpp
	system/test # appConfig.hpp
	system/src
	protocol/src
	util/src
	glad
)
conan_target_link_libraries(calendarTest)


# system/radio test that sends radio packets (command line tool)
add_executable(radioSendTest
	system/test/radioSendTest.cpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
)
target_include_directories(radioSendTest
	PRIVATE
	font
	board/emu # boardConfig.hpp
	system/test # appConfig.hpp
	system/src
	protocol/src
	util/src
	glad

)
conan_target_link_libraries(radioSendTest)


# system/network test that sends udp packets (command line tool)
add_executable(networkTest
	system/test/networkTest.cpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
)
target_include_directories(networkTest
	PRIVATE
	font
	board/emu # boardConfig.hpp
	system/test # appConfig.hpp
	system/src
	protocol/src
	util/src
	glad
)
conan_target_link_libraries(networkTest)


# system/usb test host, host for usbTest on device (command line tool)
add_executable(usbTestHost
	system/test/usbTestHost.cpp
)
target_include_directories(usbTestHost
	PRIVATE
	system/src
	util/src
)
conan_target_link_libraries(usbTestHost)


# node/MqttSnClient test
add_executable(MqttSnClientTest
	node/test/MqttSnClientTest.cpp
	control/src/appConfig.hpp
	board/emu/boardConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
	${NODE}
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
conan_target_link_libraries(MqttSnClientTest)


# node/MqttSnBroker test
add_executable(MqttSnBrokerTest
	node/test/MqttSnBrokerTest.cpp
	control/src/appConfig.hpp
	board/emu/boardConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
	${NODE}
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
conan_target_link_libraries(MqttSnBrokerTest)


# test for BME680 air sensor
add_executable(bme680Test
	control/test/bme680Test.cpp
	board/emu/boardConfig.hpp
	control/src/appConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
	control/src/BME680.cpp
	control/src/BME680.hpp
	font
)
target_include_directories(bme680Test
	PRIVATE
	board/emu # boardConfig.hpp
	font
	control/src
	system/src
	protocol/src
	util/src
	glad
)
conan_target_link_libraries(bme680Test)


# test for SSD1309 display
add_executable(ssd1309Test
	control/test/ssd1309Test.cpp
	board/emu/boardConfig.hpp
	control/src/appConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
	control/src/SSD1309.cpp
	control/src/SSD1309.hpp
	control/src/emu/display.cpp
)
target_include_directories(ssd1309Test
	PRIVATE
	board/emu # boardConfig.hpp
	font
	control/src
	system/src
	protocol/src
	util/src
	glad
)
conan_target_link_libraries(ssd1309Test)


# test for Storage class
add_executable(storageTest
	control/test/storageTest.cpp
	board/emu/boardConfig.hpp
	${UTIL}
	${PROTOCOL}
	${SYSTEM}
	node/src/State.cpp
	node/src/State.hpp
	node/src/Storage.cpp
	node/src/Storage.hpp
	system/src/emu/flash.cpp
)
target_include_directories(storageTest
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
conan_target_link_libraries(storageTest)


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
conan_target_link_libraries(utilTest)
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
conan_target_link_libraries(protocolTest)
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
conan_target_link_libraries(systemTest)
add_test(NAME systemTest
	COMMAND systemTest --gtest_output=xml:report.xml
	#WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../testdata
)
