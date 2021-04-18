#include "RoomControl.hpp"
#include "Gui.hpp"
#include <flash.hpp>
#include <emu/spi.hpp>
#include <display.hpp>
#include <radio.hpp>
#include <emu/loop.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <iterator>



// emulator drivers
namespace spi {
	void setTemperature(float celsius);
	
	void doGui(Gui &gui, int &id) {
		// air sensor
		int temperature = gui.temperatureSensor(id++);
		if (temperature != -1)
			setTemperature(temperature / 20.0f - 273.15f);
		gui.newLine();
	}
}
namespace display {
	void getDisplay(uint8_t *buffer);
}
namespace bus {
  void doGui(Gui &gui, int &id);
}
namespace radio {
  void doGui(Gui &gui, int &id);
}


static void errorCallback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static void mouseCallback(GLFWwindow* window, int button, int action, int mods) {
 /*   if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if(GLFW_PRESS == action)
            lbutton_down = true;
        else if(GLFW_RELEASE == action)
            lbutton_down = false;
    }

    if(lbutton_down) {
         // do your drag here
    }*/
}

struct DeviceData {
	uint32_t deviceId;
	String name;
	
	struct Component {
		RoomControl::Device::Component::Type type;
		uint8_t endpointIndex;
		uint8_t elementIndex;
		SystemDuration duration;
	};
	
	int componentCount;
	Component components[10];
};

// configuration for local devices (air sensor, brightness sensor, motion detector)
constexpr DeviceData localDeviceData[] = {
	{0x00000001, "tempsensor", 1, {
		{RoomControl::Device::Component::CELSIUS, 0, 0}
	}}
};


// configuration for emulated devices in Bus.cpp
constexpr DeviceData busDeviceData[] = {
	{0x00000001, "switch1", 7, {
		{RoomControl::Device::Component::ROCKER, 0, 0},
		{RoomControl::Device::Component::BUTTON, 1, 0},
		{RoomControl::Device::Component::DELAY_BUTTON, 1, 0, 2s},
		{RoomControl::Device::Component::SWITCH, 1, 1},
		{RoomControl::Device::Component::RELAY, 2, 0},
		{RoomControl::Device::Component::TIME_RELAY, 3, 0, 3s},
		{RoomControl::Device::Component::RELAY, 4, 0}
	}},
	{0x00000002, "switch2", 10, {
		{RoomControl::Device::Component::ROCKER, 0, 0},
		{RoomControl::Device::Component::BUTTON, 1, 0},
		{RoomControl::Device::Component::BLIND, 2, 0, 6500ms},
		{RoomControl::Device::Component::HOLD_ROCKER, 3, 0, 2s},
		{RoomControl::Device::Component::HOLD_BUTTON, 4, 0, 2s},
		{RoomControl::Device::Component::BLIND, 5, 0, 6500ms},
		{RoomControl::Device::Component::SWITCH, 5, 0},
		{RoomControl::Device::Component::SWITCH, 5, 1},
		{RoomControl::Device::Component::RELAY, 6, 0},
		{RoomControl::Device::Component::RELAY, 7, 0}
	}},
	{0x00000003, "tempsensor", 1, {
		{RoomControl::Device::Component::CELSIUS, 0, 0}
	}}
};

void setDevices(Array<DeviceData> deviceData, Storage::Array<RoomControl::Device, RoomControl::DeviceState> &devices) {
	for (auto d : deviceData) {
		RoomControl::Device device = {};
		RoomControl::DeviceState deviceState = {};
		device.deviceId = d.deviceId;
		device.setName(d.name);
		
		// add components
		for (int i = 0; i < d.componentCount; ++i) {
			auto &componentData = d.components[i];
			auto type = componentData.type;

			RoomControl::ComponentEditor editor(device, deviceState, i);
			auto &component = editor.insert(type);
			++device.componentCount;
			
			component.endpointIndex = componentData.endpointIndex;
			component.nameIndex = device.getNameIndex(type, i);
			component.elementIndex = componentData.elementIndex;
			
			if (component.is<RoomControl::Device::TimeComponent>()) {
				auto &timeComponent = component.cast<RoomControl::Device::TimeComponent>();
				timeComponent.duration = componentData.duration;
			}
		}
		devices.write(devices.size(), &device);
	}
}

constexpr String routeData[][2] = {
	{"room/switch1/rk0", "room/switch1/rl0"},
	{"room/switch1/bt0", "room/switch1/rl1"},
	{"room/switch1/bt1", "room/switch1/rl2"},
	{"room/switch1/sw0", "room/switch1/rl2"},
	{"room/switch2/rk0", "room/switch2/bl0"},
	{"room/switch2/bt0", "room/switch2/bl0"},
	{"room/switch2/rk1", "room/switch2/bl1"},
	{"room/switch2/bt1", "room/switch2/bl1"},
	{"room/switch2/sw0", "room/switch2/rl0"},
	{"room/switch2/sw1", "room/switch2/rl1"},
};

struct TimerData {
	ClockTime time;
	String topic;
};
constexpr TimerData timerData[] = {
	{ClockTime(1, 10, 00), "room/switch1/rl0"},
	{ClockTime(2, 22, 41), "room/switch1/rl1"}
};


/**
 * Emulator main, start without parameters
 */
int main(int argc, const char **argv) {

	// erase emulated flash
	memset(const_cast<uint8_t*>(flash::getAddress(0)), 0xff, FLASH_PAGE_COUNT * FLASH_PAGE_SIZE);

	// read flash contents from file
	std::ifstream is("flash.bin", std::ios::binary);
	//is.read(reinterpret_cast<char*>(const_cast<uint8_t*>(Flash::getAddress(0))), Flash::PAGE_COUNT * Flash::PAGE_SIZE);
	is.close();

	// get device from command line
	//std::string device = argv[1];
	
	// init GLFW
	glfwSetErrorCallback(errorCallback);
	if (!glfwInit())
		exit(EXIT_FAILURE);

	// window size
	int width = 800;
	int height = 800;

	// scale window size on linux, is done automatically on mac
#ifdef __linux__
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	float xScale, yScale;
	glfwGetMonitorContentScale(monitor, &xScale, &yScale);

	width = int(width * xScale);
	height = int(height * yScale);
#endif

	// create GLFW window and OpenGL 3.3 Core context
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	//glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GL_TRUE);
	GLFWwindow *window = glfwCreateWindow(width, height, "RoomControl", NULL, NULL);
	if (!window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwSetKeyCallback(window, keyCallback);
	glfwSetMouseButtonCallback(window, mouseCallback);
	
	// make OpenGL context current
	glfwMakeContextCurrent(window);
	
	// load OpenGL functions
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	
	// v-sync
	glfwSwapInterval(1);

	// emulator user interface
	Gui gui;

	// set global variables
	boost::system::error_code ec;
	asio::ip::address localhost = asio::ip::address::from_string("::1", ec);
	global::local = asio::ip::udp::endpoint(asio::ip::udp::v6(), 1337);
	global::upLink = asio::ip::udp::endpoint(localhost, 47193);

	// init emulated drivers
	timer::init();
	calendar::init();
	spi::init();
	display::init();
	radio::init();

	// the room control application
	RoomControl roomControl;

	// add test data
	setDevices(localDeviceData, roomControl.localDevices);
	setDevices(busDeviceData, roomControl.busDevices);
	for (auto r : routeData) {
		RoomControl::Route route = {};
		route.setSrcTopic(r[0]);
		route.setDstTopic(r[1]);
		roomControl.routes.write(roomControl.routes.size(), &route);
	}
	for (auto t : timerData) {
		RoomControl::Timer timer = {};
		RoomControl::TimerState timerState = {};
		timer.time = t.time;
		
		// add commands
		{
			RoomControl::CommandEditor editor(timer, timerState);
			editor.insert();
			editor.setTopic(t.topic);
			editor.setValueType(RoomControl::Command::ValueType::SWITCH);
			++timer.commandCount;
			editor.next();
		}
		
		roomControl.timers.write(roomControl.timers.size(), &timer);
	}

	// debug print devices
	for (auto e : roomControl.busDevices) {
		RoomControl::Device const &device = *e.flash;
		RoomControl::DeviceState &deviceState = *e.ram;
		std::cout << device.getName() << std::endl;
		for (RoomControl::ComponentIterator it(device, deviceState); !it.atEnd(); it.next()) {
			auto &component = it.getComponent();
			StringBuffer<8> b = component.getName();
			std::cout << "\t" << b << std::endl;
		}
	}

	// debug print timers
	for (auto e : roomControl.timers) {
		RoomControl::Timer const &timer = *e.flash;
		RoomControl::TimerState &timerState = *e.ram;
		
		int minutes = timer.time.getMinutes();
		int hours = timer.time.getHours();
		StringBuffer<16> b = dec(hours) + ':' + dec(minutes, 2);
		std::cout << b << std::endl;
		for (RoomControl::CommandIterator it(timer, timerState); !it.atEnd(); it.next()) {
			auto &command = it.getCommand();
			std::cout << "\t" << it.getTopic() << std::endl;
			std::cout << "\t";
			switch (command.valueType) {
			case RoomControl::Command::ValueType::BUTTON:
				std::cout << "BUTTON";
				break;
			case RoomControl::Command::ValueType::SWITCH:
				std::cout << "SWITCH";
				break;
			}
			std::cout << std::endl;
		}
	}

	// subscribe devices after test data has been added
	roomControl.subscribeInterface(roomControl.localInterface, roomControl.localDevices);
	roomControl.subscribeAll();

	// main loop
	int frameCount = 0;
	auto start = std::chrono::steady_clock::now();
	while (!glfwWindowShouldClose(window)) {
		auto frameStart = std::chrono::steady_clock::now();

		// process events
		glfwPollEvents();

		// process emulated system events
		loop::context.poll();
		if (loop::context.stopped())
			loop::context.reset();

		// mouse
		gui.doMouse(window);

		// set viewport
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
		
		// clear screen
		glClear(GL_COLOR_BUFFER_BIT);

		// gui
		{
			int id = 0;
		
			// user interface
			{
				// display
				uint8_t displayBuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];
				display::getDisplay(displayBuffer);
				gui.display(displayBuffer);

				// poti
				auto poti = gui.poti(id++);
				roomControl.onPotiChanged(poti.first, poti.second);

				gui.newLine();
			}
						
			// local devices
			spi::doGui(gui, id);
			
			// bus devices
			bus::doGui(gui, id);

			// radio devices
			radio::doGui(gui, id);
		}
		
		// swap render buffer to screen
		glfwSwapBuffers(window);
		
		
		// show frames per second
		auto now = std::chrono::steady_clock::now();
		++frameCount;
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
		if (duration.count() > 1000) {
			//std::cout << frameCount * 1000 / duration.count() << "fps" << std::endl;
			frameCount = 0;
			start = std::chrono::steady_clock::now();
		}

	}

	// write flash
	std::ofstream os("flash.bin", std::ios::binary);
	os.write(reinterpret_cast<const char*>(flash::getAddress(0)), FLASH_PAGE_COUNT * FLASH_PAGE_SIZE);
	os.close();

	// cleanup
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
