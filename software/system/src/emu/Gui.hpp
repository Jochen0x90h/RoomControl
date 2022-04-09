#pragma once

#include <glad.h>
#include <GLFW/glfw3.h> // http://www.glfw.org/docs/latest/quick_guide.html
#include "assert.hpp"
#include <map>
#include <optional>


/**
 * Immetiate mode emulator user interface. The user interface has to be rebuilt every frame in the render loop
 */
class Gui {
public:
	
	Gui();
	
	~Gui() = default;
	
	/**
	 * Handle mouse input and prepare for rendering
	 */
	void doMouse(GLFWwindow *window);
	
	void next(float w, float h);

	/**
	 * Put the following gui elements onto a new line
	 */
	void newLine();

	/**
	 * Add a switch with on/off states
	 * @return switch state or -1 when no event
	 */
	std::optional<int> onOff(uint32_t id);

	/**
	 * Add a button with pressed/release states
	 * @return button state or -1 when no event
	 */
	std::optional<int> button(uint32_t id, float size = 1.0f);

	/**
	 * Add a rocker with up/down/release states
	 * @return rocker state or -1 when no event
	 */
	std::optional<int> rocker(uint32_t id);

	/**
	 * Add a virtual double rocker switch
	 * @param id widget id
	 * @param combinations support pressing button combinations
	 * @return button state or -1 when no event
	 */
	std::optional<int> doubleRocker(uint32_t id, bool combinations = false);

	struct Poti {
		std::optional<int> delta;
		std::optional<bool> button;
	};

	/**
	 * Add a digital potentiometer with button
	 * @param id id of poti, must stay the same during its lifetime
	 */
	Poti poti(uint32_t id);

	/**
	 * Add a virtual temperature sensor
	 * @return temperature in celsius
	 */
	std::optional<float> temperatureSensor(uint32_t id);

    enum class LightState {
        DISABLED = 0,
        OFF,
        ON
    };

	/**
	 * Add a light
	 * @param power power on/off
	 * @param percentage brightness percentage
	 */
    void light(LightState state, int percentage = 100);
	void light(bool on, int percentage = 100) {light(on ? LightState::ON : LightState::OFF, percentage);}

	/**
	 * Add a blind
	 */
	void blind(int percentage);

	/**
	 * Add an indicator led
	 */
	void led(int color);

	/**
	 * Add an 8 bit grayscale display
	 * @param width width of display
	 * @param height height of display
	 * @param bitmap content of display
	 */
	void display(int width, int height, uint8_t const *displayBuffer);

protected:

	// render state
	class Render {
	public:
		
		Render(char const *fragmentShaderSource);
		
		void setState(float x, float y, float w, float h);
		
		void drawAndResetState();
	
		GLint getUniformLocation(char const *name) const {return glGetUniformLocation(this->program, name);}
	
	protected:
		
		GLuint program;
		GLint matLocation;
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
		bool contains(float x, float y) const {
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
	W *getWidget(uint32_t id, float width, float height, Args... args) {
		W *widget = dynamic_cast<W*>(this->widgets[id]);
		if (widget == nullptr) {
			delete this->widgets[id];
			widget = new W(args...);
			this->widgets[id] = widget;
		}
		widget->used = true;
		widget->x1 = this->x;
		widget->y1 = this->y;
		widget->x2 = this->x + width;
		widget->y2 = this->y + height;
		
		return widget;
	}
	

	// vertex buffer containing a quad for drawing widgets
	GLuint quad;
	
	std::map<uint32_t, Widget*> widgets;
	Widget *activeWidget = nullptr;


	// switch
	class SwitchWidget : public Widget {
	public:
		SwitchWidget() = default;
		~SwitchWidget() override;

		void touch(bool first, float x, float y) override;
		void release() override;

		int state = 0;
		int lastState = 0;
	};

	// button
	class ButtonWidget : public Widget {
	public:
		ButtonWidget() = default;
		~ButtonWidget() override;
	
		void touch(bool first, float x, float y) override;
		void release() override;

		int state = 0;
		int lastState = 0;
	};

	// rocker
	class RockerWidget : public Widget {
	public:
		RockerWidget() = default;
		~RockerWidget() override;
	
		void touch(bool first, float x, float y) override;
		void release() override;

		int state = 0;
		int lastState = 0;
	};

	// rocker render state (also used by onOff and button)
	Render *rockerRender;

	// rocker uniform locations (also used by onOff and button)
	GLint rockerA0;
	GLint rockerA1;


	// double rocker
	class DoubleRockerWidget : public Widget {
	public:
		DoubleRockerWidget(bool combinations) : combinations(combinations) {}
		~DoubleRockerWidget() override;
	
		void touch(bool first, float x, float y) override;
		void release() override;

		bool combinations;
		int state = 0;
		int lastState = 0;
	};

	// double rocker render state
	Render *doubleRockerRender;

	// double rocker uniform locations
	GLint doubleRockerA0;
	GLint doubleRockerA1;
	GLint doubleRockerB0;
	GLint doubleRockerB1;


	// poti
	class PotiWidget : public Widget {
	public:

		PotiWidget(int defaultValue = 0) : value(defaultValue), lastValue(0) {}
		~PotiWidget() override;

		void touch(bool first, float x, float y) override;
		void release() override;

		// current (mechanical) increment count in 16:16 format
		uint32_t value;
		int lastValue;

		// current button state
		bool button = false;
		bool lastButton = false;

		// last mouse position
		float x = 0;
		float y = 0;
	};

	// poti render state
	Render *potiRender;

	// poti uniform locations
	GLint potiValue;
	GLint potiState;


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


	// light render state
	Render *lightRender;
	
	// light uniform locations
	GLint lightColor;
	GLint lightInnerColor;


	// blind render state
	Render *blindRender;
	
	// blind uniform locations
	GLint blindValue;


	// display render state
	Render *displayRender;

	// display texture
	GLuint displayTexture;



	// current "paint" location
	float x;
	float y;
	float maxHeight;
};
