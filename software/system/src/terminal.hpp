#include <StringBuffer.hpp>


namespace terminal {

/*
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
*/

/**
 * Write a string to a terminal (blocking)
 * @param index of channel
 * @param str string to write
 */
void write(int index, String const& str);


struct Stream {
	int index;
	
	Stream &operator <<(char ch) {
		terminal::write(this->index, String(1, &ch));
		return *this;
	}
	
	Stream &operator <<(String const &str) {
		terminal::write(this->index, str);
		return *this;
	}
};

extern Stream out;
extern Stream err;

}
