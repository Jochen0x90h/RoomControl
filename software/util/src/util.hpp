#pragma once

#include <stdint.h>

#define getOffset(Type, member) intptr_t(&((Type*)nullptr)->member)

constexpr int min(int x, int y) {return x < y ? x : y;}
constexpr int max(int x, int y) {return x > y ? x : y;}

/*
#ifndef _STDLIB_H_
constexpr int abs(int x) {return x > 0 ? x : -x;}
#endif
constexpr int clamp(int x, int minval, int maxval) {return x < minval ? minval : (x > maxval ? maxval : x);}
constexpr int limit(int x, int size) {return x < 0 ? 0 : (x >= size ? size - 1 : x);}
*/
namespace array {

template <typename T, int N>
constexpr int size(const T (&array)[N]) {return N;}

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

}

/*
template <int N, int M>
constexpr void copy(const char (&src)[N], char (&dst)[M]) {
	for (int i = 0; i < min(N, M); ++i)
		dst[i] = src[i];
}
*/

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
