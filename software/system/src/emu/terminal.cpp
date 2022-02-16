#include "../terminal.hpp"
#include <unistd.h>


namespace terminal {

void write(int index, String const &str) {
	::write(index, str.data, str.count());
}

Stream out{1};
Stream err{2};

}
