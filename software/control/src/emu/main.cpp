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



// extension to emulated spi driver
namespace spi {
	// set temperature of emulated air sensor
	void setTemperature(float celsius);
	
	void doGui(Gui &gui, int &id) {
		// air sensor
		int temperature = gui.temperatureSensor(id++);
		if (temperature != -1)
			setTemperature(temperature / 20.0f - 273.15f);
		gui.newLine();
	}
}

// extension to emulated display driver
namespace display {
	// get dipslay contents into an 8 bit grayscale image
	void getDisplay(uint8_t *buffer);
}

// extension to emulated bus driver
namespace bus {
	void doGui(Gui &gui, int &id);
}

// extension to emulated radio driver
namespace radio {
	// poll usb device
	//void poll();

	// let the emulated radio receive some data
	void receiveData(int channel, uint8_t *data);

	// little endian 32 bit integer
	#define LE_INT32(value) uint8_t(value), uint8_t(value >> 8), uint8_t(value >> 16), uint8_t(value >> 24)

	static AesKey const defaultAesKey = {{0x5a696742, 0x6565416c, 0x6c69616e, 0x63653039, 0x166d75b9, 0x730834d5, 0x1f6155bb, 0x7c046582, 0xe62066a9, 0x9528527c, 0x8a4907c7, 0xf64d6245, 0x018a08eb, 0x94a25a97, 0x1eeb5d50, 0xe8a63f15, 0x2dff5170, 0xb95d0be7, 0xa7b656b7, 0x4f1069a2, 0xf7066bf4, 0x4e5b6013, 0xe9ed36a4, 0xa6fd5f06, 0x83c904d0, 0xcd9264c3, 0x247f5267, 0x82820d61, 0xd01eebc3, 0x1d8c8f00, 0x39f3dd67, 0xbb71d006, 0xf36e8429, 0xeee20b29, 0xd711d64e, 0x6c600648, 0x3801d679, 0xd6e3dd50, 0x01f20b1e, 0x6d920d56, 0x41d66745, 0x9735ba15, 0x96c7b10b, 0xfb55bc5d}};

	struct GreenPowerDevice {
		// the emulated radio channel the device sends on
		int channel;
		
		// device id
		uint32_t deviceId;

		// device security counter
		uint32_t counter;

		// device key
		AesKey key;
		
		// time for long button press
		std::chrono::steady_clock::time_point time;
		
		// last rocker state
		int lastRocker = 0;
	};

	GreenPowerDevice devices[1];

	// data used to initialize the green power devices
	struct GreenPowerDeviceData {
		uint32_t deviceId;
		uint32_t counter;
		uint8_t key[16];
	};

	GreenPowerDeviceData const deviceData[1] = {{
		0x12345678,
		0xfffffff0,
		{0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf}}};

	void doGui(Gui &gui, int &id) {
		static bool inited = false;
		if (!inited) {
			inited = true;
			// init emulated green power devices (on user interface)
			for (int i = 0; i < array::size(devices); ++i) {
				GreenPowerDeviceData const &d = deviceData[i];
				GreenPowerDevice &device = devices[i];
				device.channel = 11; // default channel
				device.deviceId = d.deviceId;
				device.counter = d.counter;
				setKey(device.key, d.key);
			}
		}
		
		// emulated devices on user interface
		for (GreenPowerDevice &device : devices) {
			int rocker = gui.doubleRocker(id++);
			if (rocker != -1) {

				// time difference
				auto now = std::chrono::steady_clock::now();
				int ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - device.time).count();
				device.time = now;

				if (ms > 3000 && rocker == 0) {
					// released after some time: commission
					if (device.lastRocker & 1)
						device.channel = 15;
					else if (device.lastRocker & 2)
						device.channel = 20;
					else if (device.lastRocker & 4)
						device.channel = 11;
					else if (device.lastRocker & 8)
						device.channel = 25;

					uint8_t data[] = {0,
						0x01, 0x08, uint8_t(device.counter), 0xff, 0xff, 0xff, 0xff, // mac header
						0x0c, // network header
						LE_INT32(device.deviceId), // deviceId
						0xe0, 0x02, 0xc5, 0xf2, // command and flags
						0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, // key
						LE_INT32(0), // mic
						LE_INT32(device.counter)}; // counter
					data[0] = array::size(data) - 1 + 2;

					// nonce
					Nonce nonce(device.deviceId, device.deviceId);

					// header is deviceId
					uint8_t const *header = data + 9;
					int headerLength = 4;
					
					// payload is key
					uint8_t *message = data + 17;
					
					encrypt(message, nonce, header, headerLength, message, 16, 4, defaultAesKey);

					radio::receiveData(device.channel, data);
				} else {
					uint8_t command = rocker != 0 ? 0 : 0x04;
					int r = rocker | device.lastRocker;
					if (r == 1)
						command |= 0x10;
					else if (r == 2)
						command |= 0x11;
					else if (r == 4)
						command |= 0x13;
					else if (r == 8)
						command |= 0x12;

					uint8_t data[] = {0,
						0x01, 0x08, uint8_t(device.counter), 0xff, 0xff, 0xff, 0xff, // mac header
						0x8c, 0x30, // network header
						LE_INT32(device.deviceId), // deviceId
						LE_INT32(device.counter), // counter
						command,
						0x00, 0x00, 0x00, 0x00}; // mic
					data[0] = array::size(data) - 1 + 2;
					
					// nonce
					Nonce nonce(device.deviceId, device.counter);

					// header is network header, deviceId, counter and command
					uint8_t const *header = data + 8;
					int headerLength = 11;

					// message: empty payload and mic
					uint8_t *message = data + 19;

					encrypt(message, nonce, header, headerLength, message, 0, 4, device.key);

					radio::receiveData(device.channel, data);
				}
				++device.counter;
				device.lastRocker = rocker;
			}
		}
		
		gui.newLine();
	}
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
	radio::start(15); // start on channel 15
	radio::enableReceiver(true); // enable baseband decoder

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
