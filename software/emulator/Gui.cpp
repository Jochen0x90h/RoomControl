#include "Gui.hpp"
#include "StringBuffer.hpp"
#include "tahoma_8pt.hpp"
#include <vector>
#include <stdexcept>
#include <cmath>


float const MARGIN = 0.02f;

int const TEMPERATURE_BITMAP_WIDTH = 40;

GLuint createTexture(int width, int height) {
	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);
	return texture;
}

GLuint createShader(GLenum type, char const *code)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &code, NULL);
	glCompileShader(shader);
	
	// check status
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		// get length of log (including trailing null character)
		GLint length;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
	
		// get log
		std::vector<char> errorLog(length);
		glGetShaderInfoLog(shader, length, &length, (GLchar*)errorLog.data());
		
		throw std::runtime_error(errorLog.data());
	}
	return shader;
}

template <int W, int H>
void convert(uint8_t *buffer, Bitmap<W, H> const &bitmap) {
	uint8_t const foreground = 255;
	uint8_t const background = 48;
	int width = W;;
	int height = H;
	for (int j = 0; j < height; ++j) {
		uint8_t *b = &buffer[width * j];
		for (int i = 0; i < width; ++i) {
			// data layout: rows of 8 pixels where each byte describes a column in each row
			// this would be the layout of a 16x16 display where each '|' is one byte
			// ||||||||||||||||
			// ||||||||||||||||
			bool bit = (bitmap.data[i + width * (j >> 3)] & (1 << (j & 7))) != 0;
			b[i] = bit ? foreground : background;
		}
	}
}

// Gui::Render

Gui::Render::Render(char const *fragmentShaderSource) {
	// create vertex shader
	GLuint vertexShader = createShader(GL_VERTEX_SHADER,
		"#version 330\n"
		"uniform mat4 mat;\n"
		"in vec2 position;\n"
		"out vec2 xy;\n"
		"void main() {\n"
		"	gl_Position = mat * vec4(position, 0.0, 1.0);\n"
		"	xy = position;\n"
		"}\n");
	
	// create fragment shader
	GLuint fragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
	
	// create and link program
	this->program = glCreateProgram();
	glAttachShader(this->program, vertexShader);
	glAttachShader(this->program, fragmentShader);
	glLinkProgram(this->program);

	// get uniform locations
	this->matLocation = getUniformLocation("mat");

	// create vertex array objct
	GLuint positionLocation = glGetAttribLocation(this->program, "position");
	glGenVertexArrays(1, &this->vertexArray);
	glBindVertexArray(this->vertexArray);
	glEnableVertexAttribArray(positionLocation);
	// vertex buffer is already bound
	glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
	glBindVertexArray(0);
}

void Gui::Render::setState(float x, float y, float w, float h) {
	// use program
	glUseProgram(this->program);
	
	// set matrix
	float mat[16] = {
		w * 2.0f, 0, 0, x * 2.0f - 1.0f,
		0, -h * 2.0f, 0, -(y * 2.0f - 1.0f),
		0, 0, 0, 0,
		0, 0, 0, 1};
	glUniformMatrix4fv(this->matLocation, 1, true, mat);
	
	// set vertex array
	glBindVertexArray(this->vertexArray);
}

void Gui::Render::drawAndResetState() {
	// draw
	glDrawArrays(GL_TRIANGLES, 0, 6);
	
	// reset
	glBindVertexArray(0);
}


// Gui::Widget

Gui::Widget::~Widget() {
}


// Gui::PotiWidget

Gui::PotiWidget::~PotiWidget() {
}

void Gui::PotiWidget::touch(bool first, float x, float y) {
	float ax = x - 0.5f;
	float ay = y - 0.5f;
	bool inner = std::sqrt(ax * ax + ay * ay) < 0.1;
	if (first) {
		// check if button (inner circle) was hit
		this->state = inner;
	} else if (!inner) {
		float bx = this->x - 0.5f;
		float by = this->y - 0.5f;

		float d = (ax * by - ay * bx) / (std::sqrt(ax * ax + ay * ay) * std::sqrt(bx * bx + by * by));
		this->value = this->value - int(d * 10000 * 24);
	}
	this->x = x;
	this->y = y;
}

void Gui::PotiWidget::release() {
	this->state = false;
}


// Gui::ButtonWidget

Gui::ButtonWidget::~ButtonWidget() {
}

void Gui::ButtonWidget::touch(bool first, float x, float y) {
	this->state = 1;
}

void Gui::ButtonWidget::release() {
	this->state = 0;
}


// Gui::SwitchWidget

Gui::SwitchWidget::~SwitchWidget() {
}

void Gui::SwitchWidget::touch(bool first, float x, float y) {
	this->state = y < 0.5f ? 0 : 1;
}

void Gui::SwitchWidget::release() {
	// stay in current state
}


// Gui::RockerWidget

Gui::RockerWidget::~RockerWidget() {
}

void Gui::RockerWidget::touch(bool first, float x, float y) {
	this->state = y < 0.5f ? 1 : 2;
}

void Gui::RockerWidget::release() {
	this->state = 0;
}

/*
// Gui::DoubleRockerWidget

Gui::DoubleRockerWidget::~DoubleRockerWidget() {
}

void Gui::DoubleRockerWidget::touch(bool first, float x, float y) {
	this->state = x < 0.5f ? (y < 0.5f ? 1 : 2) : (y < 0.5f ? 4 : 8);
}

void Gui::DoubleRockerWidget::release() {
	this->state = 0;
}
*/

// Gui

Gui::Gui() {
	static float const quadData[6*2] =
	{
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f
	};

	// create vertex buffer containing a quad
	glGenBuffers(1, &this->quad);
	glBindBuffer(GL_ARRAY_BUFFER, this->quad);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadData), quadData, GL_STATIC_DRAW);

	// display
	this->displayRender = new Render("#version 330\n"
		"uniform sampler2D tex;\n"
		"in vec2 xy;\n"
		"out vec4 pixel;\n"
		"void main() {\n"
			"pixel = texture(tex, xy).xxxw;\n"
		"}\n");
	this->displayTexture = createTexture(Display::WIDTH, Display::HEIGHT);
	
	// poti
	this->potiRender = new Render("#version 330\n"
		"uniform float value;\n"
		"uniform float state;\n"
		"in vec2 xy;\n"
		"out vec4 pixel;\n"
		"void main() {\n"
			"vec2 a = xy - vec2(0.5, 0.5f);\n"
			"float angle = atan(a.y, a.x) - value;\n"
			"float radius = sqrt(a.x * a.x + a.y * a.y);\n"
			"float limit = cos(angle * 24) * 0.02 + 0.48;\n"
			"pixel = radius < limit ? (radius < 0.1 ? vec4(state, state, state, 1) : vec4(0.7, 0.7, 0.7, 1)) : vec4(0, 0, 0, 1);\n"
		"}\n");
	this->potiValue = this->potiRender->getUniformLocation("value");
	this->potiState = this->potiRender->getUniformLocation("state");

	// rocker switch
	this->rockerRender = new Render("#version 330\n"
		"uniform float up;\n"
		"uniform float down;\n"
		"in vec2 xy;\n"
		"out vec4 pixel;\n"
		"void main() {\n"
			"float state = xy.y < 0.5 ? up : down;\n"
			"pixel = vec4(state, state, state, 1);\n"
		"}\n");
	this->rockerUp = this->rockerRender->getUniformLocation("up");
	this->rockerDown = this->rockerRender->getUniformLocation("down");
/*
	// double rocker switch
	this->doubleRockerRender = new Render("#version 330\n"
		"uniform float a0;\n"
		"uniform float a1;\n"
		"uniform float b0;\n"
		"uniform float b1;\n"
		"in vec2 xy;\n"
		"out vec4 pixel;\n"
		"void main() {\n"
			"float state = xy.x < 0.5 ? (xy.y < 0.5 ? a0 : a1) : (xy.y < 0.5 ? b0 : b1);\n"
			"pixel = vec4(state, state, state, 1);\n"
		"}\n");
	this->doubleRockerA0 = this->doubleRockerRender->getUniformLocation("a0");
	this->doubleRockerA1 = this->doubleRockerRender->getUniformLocation("a1");
	this->doubleRockerB0 = this->doubleRockerRender->getUniformLocation("b0");
	this->doubleRockerB1 = this->doubleRockerRender->getUniformLocation("b1");
*/
	// temperature sensor
	this->temperatureTexture = createTexture(TEMPERATURE_BITMAP_WIDTH, tahoma_8pt.height);

	// light
	this->lightRender = new Render("#version 330\n"
		"uniform float value;\n"
		"uniform float innerValue;\n"
		"in vec2 xy;\n"
		"out vec4 pixel;\n"
		"void main() {\n"
			"vec2 a = xy - vec2(0.5, 0.5f);\n"
			"float angle = atan(a.y, a.x) + value;\n"
			"float length = sqrt(a.x * a.x + a.y * a.y);\n"
			"float limit = cos(angle * 50) * 0.1 + 0.4;\n"
			"float innerLimit = 0.1;\n"
			"vec4 background = vec4(0, 0, 0, 1);\n"
			"vec4 color = vec4(value, value, 0, 1);\n"
			"vec4 innerColor = vec4(innerValue, innerValue, 0, 1);\n"
			"pixel = length < limit ? (length < innerLimit ? innerColor : color) : background;\n"
		"}\n");
	this->lightValue = this->lightRender->getUniformLocation("value");
	this->lightInnerValue = this->lightRender->getUniformLocation("innerValue");

	// blind
	this->blindRender = new Render("#version 330\n"
		"uniform float value;\n"
		"in vec2 xy;\n"
		"out vec4 pixel;\n"
		"void main() {\n"
			"float y = 1 - xy.y;"
			"float f = fract((y - value) * 20) * 1.5;"
			"float g = f < 1.0 ? f : (1.5 - f) * 2;"
			"float blind = g * 0.5 + 0.1;"
			"pixel = y < value ? vec4(0.8, 0.8, 0.8, 1) : vec4(blind, blind, blind, 1);\n"
		"}\n");
	this->blindValue = this->blindRender->getUniformLocation("value");

	// button

	// reset state
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

Gui::~Gui() {

}

void Gui::doMouse(GLFWwindow *window) {
	// get window size
	int windowWidth, windowHeight;
	glfwGetWindowSize(window, &windowWidth, &windowHeight);

	double x;
	double y;
	glfwGetCursorPos(window, &x, &y);
	bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
	//std::cout << "x " << x << " y " << y << " down " << leftDown << std::endl;

	if (leftDown) {
		bool first = this->activeWidget == nullptr;
		float windowX = x / windowWidth;
		float windowY = y / windowHeight;
		if (first) {
			// search widget under mouse
			for (auto && p : this->widgets) {
				Widget *widget = p.second;
				if (widget->contains(windowX, windowY))
					this->activeWidget = widget;
			}
		}
		if (this->activeWidget != nullptr) {
			float localX = (windowX - this->activeWidget->x1) / (this->activeWidget->x2 - this->activeWidget->x1);
			float localY = (windowY - this->activeWidget->y1) / (this->activeWidget->y2 - this->activeWidget->y1);
			this->activeWidget->touch(first, localX, localY);
		}
	} else {
		if (this->activeWidget != nullptr)
			this->activeWidget->release();
		this->activeWidget = nullptr;
	}

	// prepare for rendering
	
	// reset screen coordinates
	this->x = MARGIN;
	this->y = MARGIN;
	this->maxHeight = 0;

	// garbage collect unused widgets
	auto it = this->widgets.begin();
	while (it != this->widgets.end()) {
		auto next = it;
		++next;
		
		if (!it->second->used)
			this->widgets.erase(it);
		else
			it->second->used = false;
			
		it = next;
	}
}

void Gui::next(float w, float h) {
	this->x += w + MARGIN;
	this->maxHeight = std::max(this->maxHeight, h);
}

void Gui::newLine() {
	this->x = MARGIN;
	this->y += this->maxHeight + MARGIN;
}

void Gui::display(Bitmap<Display::WIDTH, Display::HEIGHT> const &bitmap) {
	float const w = 0.4f;
	float const h = 0.2f;

	// convert bitmap to 8 bit display buffer
	convert(this->displayBuffer, bitmap);

	// set state
	this->displayRender->setState(this->x, this->y, w, h);
	glBindTexture(GL_TEXTURE_2D, this->displayTexture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, Display::WIDTH, Display::HEIGHT, GL_RED, GL_UNSIGNED_BYTE,
		this->displayBuffer);

	// draw and reset state
	this->displayRender->drawAndResetState();
	glBindTexture(GL_TEXTURE_2D, 0);
	
	next(w, h);
}

std::pair<int, bool> Gui::poti(int id) {
	float const w = 0.2f;
	float const h = 0.2f;

	auto widget = getWidget<PotiWidget>(id, w, h);

	// set state
	this->potiRender->setState(this->x, this->y, w, h);
	glUniform1f(this->potiValue, float(widget->value & 0xffff) / (65536.0f * 24.0f) * 2.0f * M_PI);
	glUniform1f(this->potiState, widget->state ? 1.0f : 0.0f);
	
	// draw and reset state
	this->potiRender->drawAndResetState();
	
	next(w, h);


	// delta
	int16_t value = widget->value >> 16;
	int delta = value - widget->lastValue;
	widget->lastValue = value;

	// activated
	bool activated = !widget->state && widget->lastState;
	widget->lastState = widget->state;
	
	return {delta, activated};
}

int Gui::button(int id) {
	float const w = 0.1f;
	float const h = 0.1f;

	auto widget = getWidget<ButtonWidget>(id, w, h);

	// set state
	this->rockerRender->setState(this->x, this->y, w, h);
	glUniform1f(this->rockerUp, widget->state == 1 ? 1.0f : 0.6f);
	glUniform1f(this->rockerDown, widget->state == 1 ? 1.0f : 0.6f);

	// draw and reset state
	this->rockerRender->drawAndResetState();
	
	next(w, h);

	int state = widget->state;
	bool activated = state != widget->lastState;
	widget->lastState = state;

	return activated ? state : -1;
}

int Gui::switcher(int id) {
	float const w = 0.1f;
	float const h = 0.1f;

	auto widget = getWidget<SwitchWidget>(id, w, h);

	// set state
	this->rockerRender->setState(this->x, this->y, w, h);
	glUniform1f(this->rockerUp, widget->state == 0 ? 1.0f : 0.6f);
	glUniform1f(this->rockerDown, widget->state == 1 ? 1.0f : 0.7f);

	// draw and reset state
	this->rockerRender->drawAndResetState();
	
	next(w, h);

	int state = widget->state;
	bool activated = state != widget->lastState;
	widget->lastState = state;

	return activated ? state : -1;
}

int Gui::rocker(int id) {
	float const w = 0.1f;
	float const h = 0.1f;

	auto widget = getWidget<RockerWidget>(id, w, h);

	// set state
	this->rockerRender->setState(this->x, this->y, w, h);
	glUniform1f(this->rockerUp, widget->state & 1 ? 1.0f : 0.6f);
	glUniform1f(this->rockerDown, widget->state & 2 ? 1.0f : 0.7f);

	// draw and reset state
	this->rockerRender->drawAndResetState();
	
	next(w, h);

	int state = widget->state;
	bool activated = state != widget->lastState;
	widget->lastState = state;

	return activated ? state : -1;
}
/*
int Gui::doubleRocker(int id) {
	float const w = 0.1f;
	float const h = 0.1f;

	auto widget = getWidget<DoubleRockerWidget>(id, w, h);

	// set state
	this->doubleRockerRender->setState(this->x, this->y, w, h);
	glUniform1f(this->doubleRockerA0, widget->state & 1 ? 1.0f : 0.6f);
	glUniform1f(this->doubleRockerA1, widget->state & 2 ? 1.0f : 0.7f);
	glUniform1f(this->doubleRockerB0, widget->state & 4 ? 1.0f : 0.7f);
	glUniform1f(this->doubleRockerB1, widget->state & 8 ? 1.0f : 0.8f);

	// draw and reset state
	this->doubleRockerRender->drawAndResetState();
	
	next(w, h);

	int state = widget->state;
	bool activated = state != widget->lastState;
	widget->lastState = state;

	return activated ? state : -1;
}
*/
int Gui::temperatureSensor(int id) {
	float const w = 0.1f;
	float const h = 0.1f;

	auto widget = getWidget<PotiWidget>(id, w, h, 20*10 << 14);

	// temperature poti
	{
		// set state
		this->potiRender->setState(this->x, this->y, w, h);
		glUniform1f(this->potiValue, float(widget->value & 0xffff) / (65536.0f * 24.0f) * 2.0f * M_PI);
		glUniform1f(this->potiState, 0.0f);
		
		// draw and reset state
		this->potiRender->drawAndResetState();
	}
	
	// temperature display	
	Bitmap<TEMPERATURE_BITMAP_WIDTH, 16> bitmap;
	bitmap.clear();
	int temperature = ((widget->value >> 14) & 0x1ff);
	bool changed = temperature != widget->lastValue;
	widget->lastValue = temperature;
	StringBuffer<8> buffer = dec(temperature / 10) + '.' + dec(temperature % 10) + " oC";
	bitmap.drawText(2, 0, tahoma_8pt, buffer);

	// convert bitmap to 8 bit display buffer
	convert(this->displayBuffer, bitmap);

	// set state
	this->displayRender->setState(this->x + w*0.15f, this->y + h*0.35f, w*0.7f, h*0.3f);
	glBindTexture(GL_TEXTURE_2D, this->temperatureTexture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, TEMPERATURE_BITMAP_WIDTH, tahoma_8pt.height, GL_RED, GL_UNSIGNED_BYTE,
		this->displayBuffer);

	// draw and reset state
	this->displayRender->drawAndResetState();
	glBindTexture(GL_TEXTURE_2D, 0);

	next(w, h);

	// return temperature in 1/20 Kelvin when changed, else -1
	return changed ? temperature * 2 + 27315 * 20 / 100 : -1;
}

void Gui::light(bool power, int percentage) {
	float const w = 0.1f;
	float const h = 0.1f;

	// set state
	this->lightRender->setState(this->x, this->y, w, h);
	float value = ((power ? percentage : 0) + 30.0f) / 130.0f;
	glUniform1f(this->lightValue, value);
	glUniform1f(this->lightInnerValue, value);

	// draw and reset state
	this->lightRender->drawAndResetState();
	
	this->x += w + MARGIN;
	this->maxHeight = std::max(this->maxHeight, h);
}

void Gui::blind(int percentage) {
	float const w = 0.1f;
	float const h = 0.1f;

	// set state
	this->blindRender->setState(this->x, this->y, w, h);
	float value = percentage / 100.0f;
	glUniform1f(this->blindValue, value);

	// draw and reset state
	this->blindRender->drawAndResetState();
	
	this->x += w + MARGIN;
	this->maxHeight = std::max(this->maxHeight, h);
}
