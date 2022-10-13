#include <QuadratureDecoder.hpp>
#include <Input.hpp>
#include <Debug.hpp>
#include <Loop.hpp>
#ifdef EMU
#include <Terminal.hpp>
#include <StringOperators.hpp>
#endif


Coroutine handlePoti() {
	while (true) {
		int8_t delta;
		int index;
		bool value;

		// wait until poti has changed or trigger detected on input
		int s = co_await select(
			QuadratureDecoder::change(0, delta),
			Input::trigger(1 << INPUT_POTI_BUTTON, 1 << INPUT_PCB_BUTTON, index, value));
		switch (s) {
		case 1:
			// poti changed
#ifdef EMU
			Terminal::out << "delta " << dec(delta) << '\n';
#endif
			Debug::setRedLed(delta & 1);
			Debug::setGreenLed(delta & 2);
			Debug::setBlueLed(delta & 4);
			break;
		case 2:
			// button activated
#ifdef EMU
			Terminal::out << "activated " << dec(index) << '\n';
#endif
			if (index == 0) {
				Debug::toggleRedLed();
				Debug::setGreenLed(false);
			} else {
				Debug::setRedLed(false);
				Debug::toggleGreenLed();
			}
			Debug::setBlueLed(false);
			break;
		}
	}
}

int main(void) {
	Loop::init();
	QuadratureDecoder::init();
	Output::init(); // for debug signals on pins
	Input::init();

	handlePoti();

	Loop::run();
}
