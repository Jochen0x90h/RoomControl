cmake_minimum_required(VERSION 3.9)

# set project
project(Emulator)

#set(CMAKE_VERBOSE_MAKEFILE ON)

# use C++17 standard by default
set(CMAKE_CXX_STANDARD 17)

# suppress warning for own s and ms literal operators that are used instad of those in std::chrono
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-user-defined-literals")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-literal-suffix")
endif()

# enable address sanitizer
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

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

# strip unised dylibs
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
	util/src/crypt.cpp
	util/src/crypt.hpp
	util/src/DataBuffer.hpp
	util/src/defines.hpp
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

# system independent utilities
set(TINYCRYPT
	util/src/tinycrypt/aes.h
	util/src/tinycrypt/aes_encrypt.c
	util/src/tinycrypt/constants.h
	util/src/tinycrypt/utils.c
	util/src/tinycrypt/utils.h
)
source_group(util/tinycrypt FILES ${TINYCRYPT})

# system
set(SYSTEM
	system/src/bus.hpp
	system/src/emu/calendar.cpp
	system/src/calendar.hpp
	system/src/ClockTime.hpp
	system/src/display.hpp
	system/src/emu/flash.cpp
	system/src/flash.hpp
	system/src/emu/global.cpp
	system/src/emu/global.hpp
	system/src/emu/poti.cpp
	system/src/poti.hpp
	system/src/spi.hpp
	system/src/SystemTime.hpp
	system/src/emu/timer.cpp
	system/src/timer.hpp
)
source_group(system FILES ${SYSTEM})

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
	control/src/emu/main.cpp
	control/src/BME680.cpp
	control/src/BME680.hpp
	control/src/BusInterface.cpp
	control/src/BusInterface.hpp
	control/src/emu/bus.cpp
	control/src/emu/config.hpp
	control/src/Device.hpp
	control/src/emu/display.cpp
	control/src/Font.cpp
	control/src/Font.hpp
	control/src/emu/Gui.cpp
	control/src/emu/Gui.hpp
	control/src/Interface.cpp
	control/src/Interface.hpp
	control/src/LocalInterface.cpp
	control/src/LocalInterface.hpp
	control/src/emu/Network.cpp
	control/src/emu/Network.hpp
	control/src/RoomControl.cpp
	control/src/RoomControl.hpp
	control/src/emu/spi.cpp
	control/src/SSD1309.cpp
	control/src/SSD1309.hpp
	control/src/Storage.cpp
	control/src/Storage.hpp
	control/src/tahoma_8pt.cpp
	control/src/tahoma_8pt.hpp
)
source_group(control FILES ${CONTROL})

# OpenGL support
set(GLAD
	glad/glad.c
	glad/glad.h
	glad/khrplatform.h
)
source_group(glad FILES ${GLAD})

# emulator executable
add_executable(emulator
	${UTIL}
	${TINYCRYPT}
	${SYSTEM}
	${NODE}
	${CONTROL}
	${GLAD}
)
include_directories(emulator
	control/src/emu
	control/src
	node/src
	system/src
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
#include_directories(brokerTest
#	control/src/emu
#	control/src
#	node/src
#	util/src
#	glad
#)
#conan_target_link_libraries(brokerTest)



# host for usbTest on device (command line tool)
add_executable(usbTestHost
	system/test/usbTestHost.cpp
)
conan_target_link_libraries(usbTestHost)



# test green power devices, e.g. PTM215Z (command line tool, needs radioTest on device)
add_executable(greenPowerTest
	node/test/greenPowerTest.cpp
	${UTIL}
	${TINYCRYPT}
)
conan_target_link_libraries(greenPowerTest)



# unit tests

# enable testing, adds a target to run all unit tests
enable_testing()

# util test (GTest)
add_executable(utilTest
	util/test/main.cpp
	util/test/utilTest.cpp
	util/test/cryptTest.cpp
	${UTIL}
	${TINYCRYPT}
)
conan_target_link_libraries(utilTest)
add_test(NAME utilTest
	COMMAND utilTest --gtest_output=xml:report.xml
	#WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../testdata
)
