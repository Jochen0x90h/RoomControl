#include "../sys.hpp"
#include <unistd.h>


namespace sys {

Writer out{1};
Writer err{2};

void Writer::write(String str) {
	::write(this->fd, str.data, str.length);
}

}
