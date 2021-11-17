#include <StringBuffer.hpp>


namespace sys {

struct Writer {
	int fd;
	
	void write(String str);
	
	template <int N>
	void write(StringBuffer<N> const &buffer) {
		write(buffer.string());
	}

	template <typename T>
	void write(T const &str) {
		StringBuffer<120> buffer(str);
		write(buffer.string());
	}
};

extern Writer out;
extern Writer err;

}
