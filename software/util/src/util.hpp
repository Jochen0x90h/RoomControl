#pragma once

#include "Array.hpp"
#include <cstdint>

#define offsetOf(Type, member) intptr_t(&((Type*)nullptr)->member)

using uint = unsigned int;

//constexpr int abs(int x) {return x >= 0 ? x : -x;}

constexpr int min(int x, int y) {return x < y ? x : y;}
constexpr float min(float x, float y) {return x < y ? x : y;}

constexpr int max(int x, int y) {return x > y ? x : y;}
constexpr float max(float x, float y) {return x > y ? x : y;}

constexpr int clamp(int x, int lo, int hi) {return x < lo ? lo : (x > hi ? hi : x);}
constexpr float clamp(float x, float lo, float hi) {return x < lo ? lo : (x > hi ? hi : x);}

constexpr int iround(float x) {return int(x + (x > 0.0f ? 0.5f : -0.5f));}


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
 * Insert elements at the beginning of the array defined by (length, begin)
 * @tparam It
 * @param length
 * @param it
 * @param count
 */
template <typename It>
void insert(int length, It begin, int count = 1) {
	auto it = begin + length - count;
	while (it > begin) {
		--it;
		it[count] = *it;
	}
}

/**
 * Erase elements from the beginning of the array defined by (length, begin)
 * @param length
 * @param begin
 * @param count
 */
template <typename It>
void erase(int length, It begin, int count = 1) {
	auto end = begin + length - count;
	for (It it = begin; it < end; ++it) {
		*it = it[count];
	}
}

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
/*
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
*/
template <typename T, int N, typename C>
int binaryLowerBound(T (&array)[N], C const &compare) {
	int l = 0;
	int h = N;
	while (l < h) {
		int mid = l + (h - l) / 2;
		if (compare(array[mid])) {
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

/**
 * Convert a number to little endian so that a number like 0x2211 is stored in memory like this: 0x11 0x22
 * independent of the endianness of the platform
 * @return big endian number
 */
constexpr uint16_t u16L(uint16_t x) {
	return x;
}

/**
 * Convert a number to little endian so that a number like 0x44332211 is stored in memory like this: 0x11 0x22 0x33 0x44
 * independent of the endianness of the platform
 * @return big endian number
 */
constexpr uint32_t u32L(uint32_t x) {
	return x;
}

/**
 * Convert a number to big endian so that a number like 0x2211 is stored in memory like this: 0x22 0x11
 * independent of the endianness of the platform
 * @return big endian number
 */
constexpr uint16_t u16B(uint16_t x) {
	return (x >> 8) | (x << 8);
}

/**
 * Convert a number to big endian so that a number like 0x44332211 is stored in memory like this: 0x44 0x33 0x22 0x11
 * independent of the endianness of the platform
 * @return big endian number
 */
constexpr uint32_t u32B(uint32_t x) {
	return (x >> 24)
		| ((x >> 8) & 0x0000ff00)
		| ((x << 8) & 0x00ff0000)
		| (x << 24);
}


// concepts
/*
template<typename T>
concept Stringable = requires(T str) {
	{ toString(0, (char *)nullptr, str) };
};
*/
