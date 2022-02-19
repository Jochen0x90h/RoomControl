#include "../Terminal.hpp"
#include <unistd.h>


namespace Terminal {

void write(int index, String const &str) {
	::write(index, str.data, str.count());
}

Stream out{1};
Stream err{2};

} // namespace Terminal
