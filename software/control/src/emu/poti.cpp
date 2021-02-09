#include "../poti.hpp"


namespace poti {

std::function<void (int, bool)> onChanged;

void init() {
}

void handle() {
}

void setHandler(std::function<void (int, bool)> onChanged) {
	poti::onChanged = onChanged;
}

} // namespace poti
