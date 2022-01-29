#pragma once


enum class StreamCommand {
	SET_UNDERLINE = 1,
	CLEAR_UNDERLINE = 2,
};


/**
 * Output stream concept
 */
template <typename T>
concept Stream = requires(T s) {
	{ s << ' ' << String() };
};

/**
 * This is a conceptual stream to check if an object is streamable
 */
struct ConceptStream {
	ConceptStream &operator <<(char ch);
	ConceptStream &operator <<(String const &str);
};

/**
 * Streamable concept, a streamable object can be streamed into a stream using the shift left operator
 */
template<typename T>
concept Streamable = requires(ConceptStream s, T a) {
	{ s << a };
};
