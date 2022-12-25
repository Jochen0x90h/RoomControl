#include "Loop.inc.hpp"
#include <assert.hpp>


namespace loop {

void init() {
	initTimer();
}

void run() {
	while (true) {
		handleEvents();
	}
}

} // namespace loop
