#pragma once

#include "glad/glad.h"
#include <GLFW/glfw3.h> // http://www.glfw.org/docs/latest/quick_guide.html
#include "assert.hpp"
#include "Display.hpp"
#include <map>


/**
 * Immetiate mode emulator user interface. The user interface has to be rebuilt every frame in the render loop
 */
class Gui {
public:

	
	Gui();
	
	~Gui();
	
	void doMouse(GLFWwindow *window);
	
	void next(float w, float h);
	void newLine();

	/**
	 * Add a display
	 * @param bitmap content of display
	 */
	void display(Bitmap<Display::WIDTH, Display::HEIGHT> const &bitmap);

	/**
	 * Add a digital potentiometer with button
	 * @param id id of poti, must stay the same during its lifetime
	 */
	std::pair<int, bool> poti(int id);

	/**
	 * Add a button with pressed/release states
	 * @return button state or -1 when no event
	 */
	int button(int id);

	/**
	 * Add a switch with on/off states
	 * @return switch state or -1 when no event
	 */
	int switcher(int id);

	/**
	 * Add a rocker with up/down/release states
	 * @return rocker state or -1 when no event
	 */
	int rocker(int id);

	/**
	 * Add a virtual double rocker switch like PTM 216Z
	 * @return button state or -1 when no event
	 */
	//int doubleRocker(int id);

	/**
	 * Add a virtual temperature sensor
	 * @return temperature in 1/20 Kelvin or -1 when no change
	 */
	int temperatureSensor(int id);

	/**
	 * Add a light
	 * @param power power on/off
	 * @param percentage brightness percentage
	 */
	void light(bool power, int percentage = 100);

	/**
	 * Add a blind
	 */
	void blind(int percentage);
	
	//bool button(int id);

protected:

	// render state
	class Render {
	public:
		
		Render(char const *fragmentShaderSource);
		
		void setState(float x, float y, float w, float h);
		
		void drawAndResetState();
	
		GLuint getUniformLocation(char const *name) {return glGetUniformLocation(this->program, name);}
	
	protected:
		
		GLuint program;
		GLuint matLocation;
		GLuint vertexArray;
	};

	// widget
	class Widget {
	public:
	
		virtual ~Widget();
	
		virtual void touch(bool first, float x, float y) = 0;
		virtual void release() = 0;
	
		/**
		 * check if widget contains the given point
		 */
		bool contains(float x, float y) {
			return x >= this->x1 && x <= this->x2 && y >= this->y1 && y <= this->y2;
		}
		
		// flag for garbage collection
		bool used = false;
		
		// bounding box
		float x1;
		float y1;
		float x2;
		float y2;
	};

	template <typename W, typename... Args>
	W *getWidget(int id, float width, float height, Args... args) {
		Widget *widget = this->widgets[id];
		W *w = static_cast<W*>(widget);
		if (w == nullptr) {
			w = new W(args...);
			this->widgets[id] = w;
		} else {
			// check if correct type
			assert(dynamic_cast<W*>(widget) != nullptr);
		}
		w->used = true;
		w->x1 = this->x;
		w->y1 = this->y;
		w->x2 = this->x + width;
		w->y2 = this->y + height;
		
		return w;
	}
	

	// vertex buffer containing a quad for drawing widgets
	GLuint quad;
	
	std::map<int, Widget*> widgets;
	Widget *activeWidget = nullptr;

	// display
	Render *displayRender;
	
	// buffer for copying bitmap contents into the texture
	uint8_t displayBuffer[Display::WIDTH * Display::HEIGHT];
	
	// display texture
	GLuint displayTexture;
	
	
	// poti
	Render *potiRender;
	
	// uniform locations
	GLint potiValue;
	GLint potiState;
	
	class PotiWidget : public Widget {
	public:
	
		PotiWidget(int defaultValue = 0) : value(defaultValue), lastValue(0) {}
		~PotiWidget() override;
	
		void touch(bool first, float x, float y) override;
		void release() override;

		// current (mechanical) increment count in 16:16 format
		uint32_t value;
		int16_t lastValue;
		
		// current press state
		bool state = false;
		bool lastState = false;
		
		// last mouse position
		float x;
		float y;
	};
	
	
	// button
	class ButtonWidget : public Widget {
	public:
	
		~ButtonWidget() override;
	
		void touch(bool first, float x, float y) override;
		void release() override;

		int state = 0;
		int lastState = 0;
	};


	// switch
	class SwitchWidget : public Widget {
	public:
	
		~SwitchWidget() override;
	
		void touch(bool first, float x, float y) override;
		void release() override;

		int state = 0;
		int lastState = 0;
	};


	// rocker
	class RockerWidget : public Widget {
	public:
	
		~RockerWidget() override;
	
		void touch(bool first, float x, float y) override;
		void release() override;

		int state = 0;
		int lastState = 0;
	};

	Render *rockerRender;

	// uniform locations
	GLint rockerUp;
	GLint rockerDown;

/*
	// double rocker
	class DoubleRockerWidget : public Widget {
	public:
	
		~DoubleRockerWidget() override;
	
		void touch(bool first, float x, float y) override;
		void release() override;

		int state = 0;
		int lastState = 0;
	};

	Render *doubleRockerRender;

	// uniform locations
	GLint doubleRockerA0;
	GLint doubleRockerA1;
	GLint doubleRockerB0;
	GLint doubleRockerB1;
*/

	// temperature sensor
	GLuint temperatureTexture;
/*
	class TemperatureSensorWidget : public Widget {
	public:
	
		~TemperatureSensorWidget() override;
	
		void touch(bool first, float x, float y) override;
		void release() override;

		int state = 0;
		int lastState = 0;
	};*/


	// light
	Render *lightRender;
	
	// uniform locations
	GLint lightValue;
	GLint lightInnerValue;


	// blind
	Render *blindRender;
	
	// uniform locations
	GLint blindValue;

	
	
	// current "paint" location
	float x;
	float y;
	float maxHeight;
};
