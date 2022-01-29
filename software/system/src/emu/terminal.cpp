#include "../terminal.hpp"
#include <unistd.h>


namespace terminal {

//Writer out{1};
//Writer err{2};
/*
void Writer::write(String str) {
	::write(this->fd, str.data, str.length);
}
*/

void write(int index, String const &str) {
	::write(index, str.data, str.count());
}

Stream out{1};
Stream err{2};

}
