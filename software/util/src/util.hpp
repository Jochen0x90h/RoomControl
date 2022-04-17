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
struct IsArray : False {
	static constexpr bool value = false;
};
  
template <typename T, int N>
struct IsArray<T[N]> : True {
	static constexpr bool value = true;
};


namespace array {

template <typename T, int N>
constexpr int count(const T (&array)[N]) {return N;}

template <typename T, int N>
constexpr int count(T (&array)[N]) {return N;}

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

/**
 * Fill a C-array
 * @param array destination array
 * @param value fill value
 */
template <typename T, int N, typename V>
void fill(T (&dst)[N], V const &value) {
	for (T &element : dst)
		element = value;
}

/**
 * Fill an iterator range
 * @param length
 * @param dst destination iterator
 * @param value fill value
 */
template <typename OutputIt, typename V>
void fill(int length, OutputIt dst, V const &value) {
	auto end = dst + length;
	for (; dst < end; ++dst) {
		*dst = value;
	}
}


/**
 * Copy between C-arrays of same size
 * @param dst destination array
 * @param src source array
 */
template <typename T, int N>
void copy(T (&dst)[N], T const (&src)[N]) {
	for (int i = 0; i < N; ++i)
		dst[i] = src[i];
}

/**
 * Copy data using two iterators, also cast each value to destination type
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

/**
 * Copy data into an Array from an iterator, also cast each value to destination type
 * @param length length to copy
 * @param dst destination iterator
 * @param src source iterator
 */
/*template <typename OutputElement, typename InputIt>
void copy(Array<OutputElement> &dst, InputIt src) {
	auto it = dst.begin();
	auto end = dst.end();
	for (; it < end; ++it, ++src) {
		*it = OutputElement(*src);
	}
}*/

/**
 * Compare data using two iterators
 * @param length length to copy
 * @param a iterator 1
 * @param b iterator 2
 */
template <typename InputIt1, typename InputIt2>
bool equals(int length, InputIt1 a, InputIt2 b) {
	auto end = a + length;
	for (; a < end; ++a, ++b) {
		if (*a != *b)
			return false;
	}
	return true;
}

template <typename T, int N, typename V>
int binaryLowerBound(T (&array)[N], V const &element) {
	int l = 0;
	int h = N;
	while (l < h) {
		int mid = l + (h - l) / 2;
		if (array[mid] < element) {
			l = mid + 1;
		} else {
			h = mid;
		}
	}
	return l;
}

} // namespace array

/*
constexpr int align4(int size) {
	return (size + 3) & ~3;
}
*/

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



// concepts
/*
template<typename T>
concept Stringable = requires(T str) {
	{ toString(0, (char *)nullptr, str) };
};
*/
