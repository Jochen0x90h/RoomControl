#include "loop.hpp"


namespace loop {

asio::io_context context;

// opengl window
GLFWwindow *window = nullptr;

// handler chain
void nop(Gui &gui) {}
Handler nextHandler = nop;
Handler addHandler(Handler handler) {
	Handler h = nextHandler;
	loop::nextHandler = handler;
	return h;
}

static void errorCallback(int error, const char* description) {
    fprintf(stderr, "Error: %s\n", description);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
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

void init() {
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
	loop::window = glfwCreateWindow(width, height, "RoomControl", NULL, NULL);
	if (!window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwSetKeyCallback(loop::window, keyCallback);
	glfwSetMouseButtonCallback(loop::window, mouseCallback);
	
	// make OpenGL context current
	glfwMakeContextCurrent(loop::window);
	
	// load OpenGL functions
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	
	// v-sync
	glfwSwapInterval(1);
}

void run() {
	//context.run();
	
	// check if loop::init() was called
	assert(loop::window != nullptr);
	
	// emulator user interface
	Gui gui;
	
	int frameCount = 0;
	auto start = std::chrono::steady_clock::now();
	while (!glfwWindowShouldClose(loop::window)) {
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
		glfwGetFramebufferSize(loop::window, &width, &height);
		glViewport(0, 0, width, height);
		
		// clear screen
		glClear(GL_COLOR_BUFFER_BIT);

		// handle gui
		loop::nextHandler(gui);

		// swap render buffer to screen
		glfwSwapBuffers(loop::window);
				
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
	
	// cleanup
	glfwDestroyWindow(loop::window);
	glfwTerminate();
}

} // namespace loop
