#include "../Terminal.hpp"
#include <iostream>
#include <algorithm>
//#include <unistd.h>


namespace Terminal {

void write(int index, String const &str) {
	std::string s(str.data, str.count());
	switch (index) {
	case 1:
		std::cout << s;
		std::cout.flush();
		break;
	case 2:
		std::cerr << s;
		std::cerr.flush();
		break;
	}
	//int size = ::write(index, str.data, str.count());
}

Stream out{1};
Stream err{2};

} // namespace Terminal
