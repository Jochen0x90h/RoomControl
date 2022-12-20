#include "Handlers.hpp"
#include "../Timer.hpp"
#include <util.hpp>
#include <assert.hpp>


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
