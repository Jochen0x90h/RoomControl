#pragma once

#include "Array.hpp"
#include <stdint.h>

#define getOffset(Type, member) intptr_t(&((Type*)nullptr)->member)

using uint = unsigned int;

//constexpr int abs(int x) {return x >= 0 ? x : -x;}

constexpr int min(int x, int y) {return x < y ? x : y;}
constexpr int max(int x, int y) {return x > y ? x : y;}

constexpr int clamp(int x, int lo, int hi) {return x < lo ? lo : (x > hi ? hi : x);}
constexpr float clamp(float x, float lo, float hi) {return x < lo ? lo : (x > hi ? hi : x);}


// helper classes for distinguishing between fixed size array and c-string
struct False {};
struct True {};

template <typename T>
struct IsArray : False {};
  
template <typename T, int N>
struct IsArray<T[N]> : True {};


namespace array {

template <typename T, int N>
constexpr int size(const T (&array)[N]) {return N;}

template <typename T, int N>
constexpr int size(T (&array)[N]) {return N;}

template <typename T, int N>
constexpr T *end(T (&array)[N]) {return array + N;}

template <typename T, int N>
constexpr T const *end(const T (&array)[N]) {return array + N;}

template <typename It>
void insert(It begin, It end, int count = 1) {
	It it = end - count;
	while (it > begin) {
		--it;
		it[count] = *it;
	}
}

template <typename It>
void erase(It begin, It end, int count = 1) {
	for (It it = begin + count; it < end; ++it) {
		it[-count] = *it;
	}
}

template <typename OutputIt, typename T>
void fill(OutputIt begin, OutputIt end, const T &value) {
	for (OutputIt it = begin; it < end; ++it) {
		*it = value;
	}
}

template <typename OutputIt, typename InputIt>
void copy(OutputIt begin, OutputIt end, InputIt src) {
	for (OutputIt it = begin; it < end; ++it, ++src) {
		*it = *src;
	}
}


template <typename T, int N, typename V>
void fill(T (&array)[N], V const &value) {
	for (T &element : array)
		element = value;
}

template <typename OutputIt, typename V>
void fill(int length, OutputIt dst, V const &value) {
	auto end = dst + length;
	for (; dst < end; ++dst) {
		*dst = value;
	}
}

/**
 * Copy data, also cast each value to destination type
 * @param length length to copy
 * @param dst destination iterator
 * @param src source iterator
 */
template <typename OutputIt, typename InputIt>
void copy(int length, OutputIt dst, InputIt src) {
	auto end = dst + length;
	for (; dst < end; ++dst, ++src) {
		*dst = decltype(*dst)(*src);
	}
}

template <typename OutputElement, typename InputIt>
void copy(Array<OutputElement> dst, InputIt src) {
	auto it = dst.data;
	auto end = it + dst.length;
	for (; it < end; ++it, ++src) {
		*it = OutputElement(*src);
	}
}

template <typename InputIt1, typename InputIt2>
bool equals(int length, InputIt1 a, InputIt2 b) {
	auto end = a + length;
	for (; a < end; ++a, ++b) {
		if (*a != *b)
			return false;
	}
	return true;
}

} // namespace array


constexpr int align4(int size) {
	return (size + 3) & ~3;
}


template <int N>
struct UIntBase {
	using Type = uint32_t;
};
template <>
struct UIntBase<1> {
	using Type = uint8_t;
};
template <>
struct UIntBase<2> {
	using Type = uint16_t;
};

/**
 * Unsigned integer that adapts to given number of values
 * @tparam N number of values the integer can represent
 */
template <int N>
struct UInt : public UIntBase<(N > 256 ? (N > 65536 ? 4 : 2) : 1)> {
};
