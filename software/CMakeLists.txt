cmake_minimum_required(VERSION 3.9)

# set project
project(Emulator)

#set(CMAKE_VERBOSE_MAKEFILE ON)

# use C++20 standard with coroutines
set(CMAKE_CXX_STANDARD 20)
if (APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
endif()

# suppress warning for own s and ms literal operators that are used instad of those in std::chrono
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-user-defined-literals")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-literal-suffix")
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
	util/src/Bitmap.cpp
	util/src/Bitmap.hpp
	util/src/convert.cpp
	util/src/convert.hpp
	util/src/Coroutine.hpp
	util/src/Data.hpp
	util/src/DataQueue.hpp
	util/src/defines.hpp
	util/src/enum.hpp
	util/src/Font.cpp
	util/src/Font.hpp
	util/src/optional.hpp
	util/src/Queue.hpp
	util/src/String.hpp
	util/src/StringBuffer.hpp
	util/src/StringOperators.hpp
	util/src/StringSet.hpp
	util/src/TopicBuffer.cpp
	util/src/TopicBuffer.hpp
	util/src/util.hpp
)
source_group(util FILES ${UTIL})

# network utilities
set(NETWORK
	network/src/crypt.cpp
	network/src/crypt.hpp
	network/src/DataBuffer.hpp
	network/src/hash.cpp
	network/src/hash.hpp
	network/src/PacketReader.hpp
	network/src/PacketWriter.hpp
	network/src/ieee.hpp
	network/src/zb.hpp
	network/src/gp.hpp
)
set(TINYCRYPT
	network/src/tinycrypt/aes.h
	network/src/tinycrypt/aes_encrypt.c
	network/src/tinycrypt/constants.h
	network/src/tinycrypt/utils.c
	network/src/tinycrypt/utils.h
)
source_group(network FILES ${NETWORK})
source_group(network/tinycrypt FILES ${TINYCRYPT})
list(APPEND NETWORK ${TINYCRYPT})

# system
set(SYSTEM
	system/src/bus.hpp
	system/src/emu/calendar.cpp
	system/src/calendar.hpp
	system/src/ClockTime.hpp
	system/src/emu/debug.cpp
	system/src/debug.hpp
	system/src/display.hpp
	system/src/emu/flash.cpp
	system/src/flash.hpp
	system/src/emu/global.cpp
	system/src/emu/global.hpp
	system/src/emu/Gui.cpp
	system/src/emu/Gui.hpp
	system/src/emu/loop.cpp
	system/src/emu/loop.hpp
	system/src/loop.hpp
	system/src/emu/poti.cpp
	system/src/poti.hpp
	system/src/emu/radio.cpp
	system/src/radio.hpp
	system/src/radioDefs.hpp
	system/src/emu/rng.cpp
	system/src/rng.hpp
	system/src/spi.hpp
	system/src/SystemTime.hpp
	system/src/emu/timer.cpp
	system/src/timer.hpp
	system/src/emu/usb.cpp
	system/src/usb.hpp
	system/src/usbDefs.hpp
	control/src/tahoma_8pt.cpp # used by Gui
	control/src/tahoma_8pt.hpp
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
	node/src/MqttSnClient.cpp
	node/src/MqttSnClient.hpp
	node/src/MqttSnBroker.cpp
	node/src/MqttSnBroker.hpp
)
source_group(node FILES ${NODE})

# room control software running on the nRF52840
set(CONTROL
	control/src/appConfig.hpp
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
	control/src/emu/Network.cpp
	control/src/emu/Network.hpp
	control/src/RadioInterface.cpp
	control/src/RadioInterface.hpp
	control/src/RoomControl.cpp
	control/src/RoomControl.hpp
	control/src/emu/spi.cpp
	control/src/SSD1309.cpp
	control/src/SSD1309.hpp
	control/src/Storage.cpp
	control/src/Storage.hpp
	control/src/emu/sysConfig.hpp
	#control/src/tahoma_8pt.cpp
	#control/src/tahoma_8pt.hpp
)
source_group(control FILES ${CONTROL})

# emulator executable
add_executable(emulator
	control/src/main.cpp
	${UTIL}
	${NETWORK}
	${SYSTEM}
	${NODE}
	${CONTROL}
)
target_include_directories(emulator
	PRIVATE
	control/src/emu
	control/src
	node/src
	system/src
	network/src
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



# broker test (command line tool)
#add_executable(brokerTest
#	node/test/brokerTest.cpp
#	${NODE}
#	control/src/emu/Network.cpp
#	control/src/emu/Network.hpp
#)
#target_include_directories(brokerTest
#	PRIVATE
#	control/src/emu
#	control/src
#	node/src
#	util/src
#	glad
#)
#conan_target_link_libraries(brokerTest)



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
	${NETWORK}
)
target_include_directories(sniffer
	PRIVATE
	util/src
	network/src
	system/src
)
conan_target_link_libraries(sniffer)



# host for usbTest on device (command line tool)
add_executable(usbTestHost
	system/test/usbTestHost.cpp
)
target_include_directories(usbTestHost
	PRIVATE
	system/src
	util/src
)
conan_target_link_libraries(usbTestHost)



# test that sends packets using the radio
add_executable(radioSendTest
	system/test/radioSendTest.cpp
	${UTIL}
	${NETWORK}
	${SYSTEM}
)
target_include_directories(radioSendTest
	PRIVATE
	control/src
	system/test/emu
	system/src
	network/src
	util/src
	glad

)
conan_target_link_libraries(radioSendTest)


# timer test
add_executable(timerTest
	system/test/timerTest.cpp
	${UTIL}
	${NETWORK}
	${SYSTEM}
)
target_include_directories(timerTest
	PRIVATE
	control/src
	system/test/emu
	system/src
	network/src
	util/src
	glad
)
conan_target_link_libraries(timerTest)


# test for BME680 air sensor
add_executable(bme680Test
	control/test/bme680Test.cpp
	${UTIL}
	${NETWORK}
	${SYSTEM}
	control/src/BME680.cpp
	control/src/BME680.hpp
	control/src/emu/spi.cpp
)
target_include_directories(bme680Test
	PRIVATE
	control/test/emu
	control/src
	system/src
	network/src
	util/src
	glad
)
conan_target_link_libraries(bme680Test)


# test for SSD1309 display
add_executable(ssd1309Test
	control/test/ssd1309Test.cpp
	${UTIL}
	${NETWORK}
	${SYSTEM}
	control/src/SSD1309.cpp
	control/src/SSD1309.hpp
	control/src/emu/spi.cpp
	control/src/emu/display.cpp
)
target_include_directories(ssd1309Test
	PRIVATE
	control/test/emu
	control/src
	system/src
	network/src
	util/src
	glad
)
conan_target_link_libraries(ssd1309Test)



# unit tests (GTest)

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

# crypt test
add_executable(cryptTest
	network/test/cryptTest.cpp
	#${UTIL}
	${NETWORK}
)
target_include_directories(cryptTest
	PRIVATE
	network/src
	util/src
)
conan_target_link_libraries(cryptTest)
add_test(NAME cryptTest
	COMMAND cryptTest --gtest_output=xml:report.xml
	#WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../testdata
)

# system test
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
