#include "../Terminal.hpp"
#include <StringOperators.hpp>


namespace Output {


void init() {
}

void enable(int index, bool enabled) {
	Terminal::out << "Output " << dec(index) << ": " << (enabled ? "enabled" : "disabled") << '\n';
}

void set(int index, bool value) {
	Terminal::out << "Output " << dec(index) << ": " << (value ? "on" : "off") << '\n';
}

void toggle(int index) {
	Terminal::out << "Output " << dec(index) << ": toggle\n";
}

} // namespace Output
