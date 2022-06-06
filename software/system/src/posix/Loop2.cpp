#include "Loop.hpp"
#include "../Timer.hpp"
#include <util.hpp>
#include <assert.hpp>
#include <ctime>
#include <poll.h>


namespace Loop {

bool inited = false;

void init() {
	Loop::inited = true;
}

void run() {
	assert(Loop::inited);
	while (true) {
		runOnce();
	}
}

} // namespace Loop
