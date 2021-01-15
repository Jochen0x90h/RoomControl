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

# OpenGL support
set(GLAD
	emulator/glad/glad.c
	emulator/glad/glad.h
	emulator/glad/khrplatform.h
)
source_group(glad FILES ${GLAD})

# emulator
set(EMULATOR
	emulator/main.cpp
	emulator/assert.hpp
	emulator/config.hpp
	emulator/global.cpp
	emulator/global.hpp
	emulator/Bus.cpp
	emulator/Bus.hpp
	emulator/Clock.cpp
	emulator/Clock.hpp
	emulator/Display.cpp
	emulator/Display.hpp
	emulator/Flash.cpp
	emulator/Gui.cpp
	emulator/Gui.hpp
	emulator/Network.cpp
	emulator/Network.hpp
	emulator/Poti.cpp
	emulator/Poti.hpp
	emulator/SystemTimer.cpp
	emulator/SystemTimer.hpp
)
source_group(emulator FILES ${EMULATOR})

# util classes
set(UTIL
	control/Array.hpp
	control/ArrayList.hpp
	control/Bitmap.cpp
	control/Bitmap.hpp
	control/ClockTime.hpp
	control/convert.cpp
	control/convert.hpp
	control/defines.hpp
	control/optional.hpp
	control/String.hpp
	control/StringBuffer.hpp
	control/StringOperators.hpp
	control/StringSet.hpp
	control/TopicBuffer.cpp
	control/TopicBuffer.hpp
	control/util.hpp
)

# room control software running on the nRF52840
set(CONTROL
	control/Device.hpp
	control/Font.cpp
	control/Font.hpp
	control/Flash.hpp
	control/MqttSnClient.cpp
	control/MqttSnClient.hpp
	control/MqttSnBroker.cpp
	control/MqttSnBroker.hpp
	control/RoomControl.cpp
	control/RoomControl.hpp
	control/Storage.cpp
	control/Storage.hpp
	control/SystemTime.hpp
	control/tahoma_8pt.cpp
	control/tahoma_8pt.hpp
)
source_group(control FILES ${CONTROL} ${UTIL})

# emulator executable
add_executable(emulator
	${GLAD}
	${EMULATOR}
	${UTIL}
	${CONTROL}
)
include_directories(emulator
	#emulator/src
	control
)
conan_target_link_libraries(emulator)

# install the emulator
install(TARGETS emulator
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
)



# test broker
add_executable(testBroker
	test/testBroker.cpp
	control/MqttSnClient.cpp
	control/MqttSnClient.hpp
	control/MqttSnBroker.cpp
	control/MqttSnBroker.hpp
	control/SystemTime.hpp
	emulator/Network.cpp
	emulator/Network.hpp
	emulator/SystemTimer.cpp
	emulator/SystemTimer.hpp
	emulator/global.cpp
	emulator/global.hpp
)
include_directories(testBroker
	control
)
conan_target_link_libraries(testBroker)



# enable testing, adds a target to run all tests
enable_testing()

add_executable(testUtil
	test/testUtil.cpp
	${UTIL}
)
conan_target_link_libraries(testUtil)
add_test(NAME utilTest
	COMMAND utilTest --gtest_output=xml:report.xml
	#WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../testdata
)
