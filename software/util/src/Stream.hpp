#pragma once

#include "String.hpp"
#include <concepts>


class Stream {
public:

	enum class Command {
		SET_UNDERLINE = 1,
		CLEAR_UNDERLINE = 2,
		SET_INVERT = 3,
		CLEAR_INVERT = 4,
	};

	virtual ~Stream();
	virtual Stream &operator <<(char ch) = 0;
	virtual Stream &operator <<(String const &str) = 0;
	virtual Stream &operator <<(Command command) = 0;

	Stream &operator <<(short) = delete;
	Stream &operator <<(unsigned short) = delete;
	Stream &operator <<(int) = delete;
	Stream &operator <<(unsigned int) = delete;
	Stream &operator <<(long) = delete;
	Stream &operator <<(unsigned long) = delete;
	Stream &operator <<(float) = delete;
};

/**
 * Streamable concept, a streamable object can be streamed into a stream using the shift left operator
 */
template <typename T>
concept Streamable = requires(Stream &s, T value) {
	{ s << value } -> std::convertible_to<Stream &>;
};
