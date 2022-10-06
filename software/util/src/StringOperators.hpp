#pragma once

#include "Stream.hpp"
#include "convert.hpp"
#include "util.hpp"

// remove when gcc 11 is available for all target platforms
//#define Stream typename
//#define Streamable typename


// decimal number
template <typename T>
struct Dec {
	T value;
	int digitCount;
};

template <typename T>
Stream &operator <<(Stream &s, Dec<T> dec) {
	char buffer[11];
	return s << toString(buffer, int32_t(dec.value), dec.digitCount);
}

// stream as r-value reference, e.g. stream() << dec(10)
template <typename T>
Stream &operator <<(Stream &&s, Dec<T> dec) {
	char buffer[11];
	return s << toString(buffer, int32_t(dec.value), dec.digitCount);
}

template <typename T>
Dec<T> dec(T value, int digitCount = 1) {
	return {value, digitCount};
}


// hexadecimal number
template <typename T>
struct Hex {
	T value;
	int digitCount;
};

template <typename T>
Stream &operator <<(Stream &s, Hex<T> hex) {
	char buffer[16];
	return s << toHexString(buffer, uint64_t(hex.value), hex.digitCount);
}

template <typename T>
Stream &operator <<(Stream &&s, Hex<T> hex) {
	char buffer[16];
	return s << toHexString(buffer, uint64_t(hex.value), hex.digitCount);
}

template <typename T>
Hex<T> hex(T value, int digitCount = sizeof(T) * 2) {
	return {value, digitCount};
}


// floating point number
struct Flt {
	float value;
	
	// number of digits before the decimal point (e.g. 2 for 00.0)
	int digitCount;
	
	// maximum number of decimals (e.g. 3 for 0.123). Negative value for fixed number of decimals (e.g. -3 for 0.000)
	int decimalCount;
};

inline Stream &operator <<(Stream &s, Flt flt) {
	char buffer[21];
	return s << toString(buffer, flt.value, flt.digitCount, flt.decimalCount);
}

inline Stream &operator <<(Stream &&s, Flt flt) {
	char buffer[21];
	return s << toString(buffer, flt.value, flt.digitCount, flt.decimalCount);
}

/**
 * Create a float value wrapper with parameters for printing
 * @param value value
 * @param decimalCount minimum number of digits to convert after the decimal point, negative to keep trailing zeros
 * @return float value wrapper
 */
constexpr Flt flt(float value, int decimalCount = 3) {
	return {value, 1, decimalCount};
}

/**
 * Create a float value wrapper with parameters for printing
 * @param value value
 * @param digitCount minimum number of digits to convert before the decimal point
 * @param decimalCount minimum number of digits to convert after the decimal point, negative to keep trailing zeros
 * @return float value wrapper
 */
constexpr Flt flt(float value, int digitCount, int decimalCount) {
	return {value, digitCount, decimalCount};
}


// c-string
constexpr String str(char const *value) {return String(value);}


// underline node
template <typename A>
struct Underline {
	A const &a;
	bool on;
};

template <typename A>
Stream &operator <<(Stream &s, Underline<A> underline) {
	if (underline.on)
		s << Stream::Command::SET_UNDERLINE;
	s << underline.a;
	if (underline.on)
		s << Stream::Command::CLEAR_UNDERLINE;
	return s;
}

template <typename A>
constexpr Underline<A> underline(A const &a, bool on = true) {
	return {a, on};
}


// invert node
template <typename A>
struct Invert {
	A const &a;
	bool on;
};

template <typename A>
Stream &operator <<(Stream &s, Invert<A> invert) {
	if (invert.on)
		s << Stream::Command::SET_INVERT;
	s << invert.a;
	if (invert.on)
		s << Stream::Command::CLEAR_INVERT;
	return s;
}

template <typename A>
constexpr Invert<A> invert(A const &a, bool on = true) {
	return {a, on};
}


// plus node
template <typename A, typename B>
struct Plus {
	A const &a;
	B const &b;
};

template <typename A, typename B>
Stream &operator <<(Stream &s, Plus<A, B> plus) {
	return s << plus.a << plus.b;
}

template <typename A, typename B>
Stream &operator <<(Stream &&s, Plus<A, B> plus) {
	return s << plus.a << plus.b;
}


// plus operators
template <Streamable A, Streamable B>
Plus<A, B> operator +(A const &a, B const &b) {
	return {a, b};
}

template <Streamable B>
Plus<char, B> operator +(char const &a, B const &b) {
	return {a, b};
}

template <Streamable A>
Plus<A, char> operator +(A const &a, char const &b) {
	return {a, b};
}

template <Streamable B>
Plus<String, B> operator +(String const &a, B const &b) {
	return {a, b};
}

template <Streamable A>
Plus<A, String> operator +(A const &a, String const &b) {
	return {a, b};
}

inline Plus<String, char> operator +(String const &a, char const &b) {
	return {a, b};
}

inline Plus<char, String> operator +(char const &a, String const &b) {
	return {a, b};
}


//#undef Stream
//#undef Streamable
