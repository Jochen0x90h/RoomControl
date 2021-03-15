#include "../poti.hpp"


namespace poti {

std::function<void (int, bool)> onChanged = [](int, bool) {};

void init() {
}

void handle() {
}

void setHandler(std::function<void (int, bool)> const &onChanged) {
	poti::onChanged = onChanged;
}

} // namespace poti
