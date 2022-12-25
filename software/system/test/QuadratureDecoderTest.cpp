#include <QuadratureDecoder.hpp>
#include <Input.hpp>
#include <Debug.hpp>
#include <Loop.hpp>
#ifdef DEBUG
#include <Terminal.hpp>
#include <StringOperators.hpp>
#endif
#include <boardConfig.hpp>


Coroutine handleDecoder(QuadratureDecoder &decoder) {
	while (true) {
		int8_t delta;
		int index;
		bool value;

		// wait until poti has changed or trigger detected on input
		int s = co_await select(
			decoder.change(delta),
			Input::trigger(1 << INPUT_POTI_BUTTON, 1 << INPUT_PCB_BUTTON, index, value));
		switch (s) {
		case 1:
			// poti changed
#ifdef DEBUG
			Terminal::out << "delta " << dec(delta) << '\n';
#endif
			debug::setRed(delta & 1);
			debug::setGreen(delta & 2);
			debug::setBlue(delta & 4);
			break;
		case 2:
			// button activated
#ifdef DEBUG
			Terminal::out << "activated " << dec(index) << '\n';
#endif
			if (index == 0) {
				debug::toggleRed();
			} else {
				debug::toggleGreen();
			}
			debug::setBlue(false);
			break;
		}
	}
}

int main() {
	loop::init();
	Output::init(); // for debug signals on pins
	Input::init();
	Drivers drivers;

	handleDecoder(drivers.quadratureDecoder);

	loop::run();
}
