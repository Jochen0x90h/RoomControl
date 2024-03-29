#pragma once

#include "util.hpp"
#include "defines.hpp"
#include <limits>


template <typename T>
struct IsCString {
	static constexpr bool value = false;
};

template <int N>
struct IsCString<char[N]> {
	static constexpr bool value = true;
};

template <>
struct IsCString<char const *> {
	static constexpr bool value = true;
};

// c-string concept, either char array or char pointer
template <typename T>
concept CString = IsCString<T>::value;


constexpr int getLength(char const *str, False) {
	int length = 0;
	while (str[length] != 0)
		++length;
	return length;
}

template <int N>
constexpr int getLength(char const (&str)[N], True) {
	int length = 0;
	while (length < N && str[length] != 0)
		++length;
	return length;
}

template <typename T> requires CString<T>
constexpr int length(T const &str) {
	return getLength(str, IsArray<T>());
}


/**
 * String, only references the data
 */
struct String {
	int const length;
	char const *const data;

	constexpr String() noexcept
		: length(0), data()
	{}

	String(String &str) = default;

	String(String const &str) = default;

	/**
	 * Construct from c-string or char array
	 */
	template <typename T> requires CString<T>
	constexpr String(T const &str) noexcept
		: length(getLength(str, IsArray<T>())), data(str)
	{}

	/**
	 * Construct String from any type of data
	 */
	String(int length, void const *data)
		: length(length), data(reinterpret_cast<char const*>(data))
	{}

	bool isEmpty() const {return this->length <= 0;}
	
	int count() const {return this->length;}
	
	/**
	 * Get a substring
	 * @param startIndex start of substring
	 * @return substring that references the data of this string
	 *
	 */
	String substring(int startIndex) const {
		return String(max(this->length - startIndex, 0), this->data + startIndex);
	}

	/**
	 * Get a substring
	 * @param startIndex start of substring
	 * @param endIndex end of substring (not included)
	 * @return substring that references the data of this string
	 *
	 */
	String substring(int startIndex, int endIndex) const {
		return String(max(min(endIndex, this->length) - startIndex, 0), this->data + startIndex);
	}

	/**
	 * return the index of the first occurrence of a character
	 * @param ch character to search
	 * @param startIndex index where to start searching
	 * @param defaultValue value to return when the character is not found
	 * @return index of first occurrence of the character
	 */
	int indexOf(char ch, int startIndex = 0, int defaultValue = -1) const {
		for (int i = startIndex; i < this->length; ++i) {
			if (this->data[i] == ch)
				return i;
		}
		return defaultValue;
	}

	/**
	 * return the index of the last occurrence of a character
	 * @param ch character to search
	 * @param startIndex index where to start searching
	 * @param defaultValue value to return when the character is not found
	 * @return index of first occurrence of the character
	 */
	int lastIndexOf(char ch, int startIndex = std::numeric_limits<int>::max(), int defaultValue = -1) const {
		int i = min(startIndex, this->length);
		while (i > 0) {
			--i;
			if (this->data[i] == ch)
				return i;
		}
		return defaultValue;
	}

	/**
	 * Assignment operator
	 */
	String &operator =(String const &str) {
		const_cast<char const *&>(this->data) = str.data;
		const_cast<int&>(this->length) = str.length;
		return *this;
		
	}
	
	/**
	 * Index operator
	 */
	constexpr char const operator [](int index) const {return this->data[index];}

	/**
	 * Calculate a hash of the string
	 * http://www.cse.yorku.ca/~oz/hash.html
	 * @return dj2 hash of string
	 */
	uint32_t hash() const {
		uint32_t h = 5381;
		for (char c : *this) {
			h = (h << 5) + h + uint8_t(c); // hash * 33 + c
		}
		return h;
	}
		
	char const *begin() const {return this->data;}
	char const *end() const {return this->data + this->length;}
};

inline bool operator ==(String a, String b) {
	if (a.length != b.length)
		return false;
	for (int i = 0; i < a.length; ++i) {
		if (a.data[i] != b.data[i])
			return false;
	}
	return true;
}

//inline bool operator !=(String a, String b) {return !operator ==(a, b);}

inline bool operator <(String a, String b) {
	int length = min(a.length, b.length);
	for (int i = 0; i < length; ++i) {
		auto c = (unsigned char)a.data[i];
		auto d = (unsigned char)b.data[i];
		if (c < d)
			return true;
		if (c > d)
			return false;
	}
	return a.length < b.length;
}


/**
 * Assign a string to a fixed size c-string
 * @param str fixed size c-string of type char[N]
 * @param s source string
 */
template <int N>
inline void assign(char (&str)[N], String const &s) {
	// copy string
	int count = min(N, s.length);
	for (int i = 0; i < count; ++i) {
		str[i] = s[i];
	}
	
	// pad with zeros
	for (int i = count; i < N; ++i) {
		str[i] = 0;
	}
}
