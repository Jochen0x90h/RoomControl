#include "loop.hpp"


namespace loop {

asio::io_context context;

void init() {
}

void run() {
	context.run();
}

} // namespace loop
