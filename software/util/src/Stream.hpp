#pragma once

#include "String.hpp"

/*
enum class StreamCommand {
	SET_UNDERLINE = 1,
	CLEAR_UNDERLINE = 2,
	SET_INVERT = 3,
	CLEAR_INVERT = 4,
};
*/

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
};


/**
 * Output stream concept
 */
/*template <typename T>
concept Stream = requires(T s) {
	{ s << ' ' << String() };
};*/

/**
 * This is a conceptual stream to check if an object is streamable
 *//*
struct ConceptStream {
	ConceptStream &operator <<(char ch);
	ConceptStream &operator <<(String const &str);
};*/

/**
 * Streamable concept, a streamable object can be streamed into a stream using the shift left operator
 */
template<typename T>
concept Streamable = requires(Stream &s, T a) {
	{ s << a };
};
