#pragma once

#include "util.hpp"
#include "defines.hpp"
#include <iostream>


// helper classes for distinguishing between fixed size array and c-string
struct False {};
struct True {};

template <typename T>
struct IsArray : False {};
  
template <typename T, int N>
struct IsArray<T[N]> : True {};

constexpr int getLength(char const *str, False) {
	int length = 0;
	while (str[length] != 0)
		++length;
	return length;
}

template<int N>
constexpr int getLength(char const (&str)[N], True) {
	int length = 0;
	while (length < N && str[length] != 0)
		++length;
	return length;
}

template<typename T>
constexpr int length(T &str) {
	return getLength(str, IsArray<T>());
}


/**
 * String, only references the data
 */
struct String {
	char const *const data;
	int const length;

	constexpr String()
		: data(), length(0)
	{}

	String(String &str)
		: data(str.data), length(str.length)
	{}

	String(String const &str)
		: data(str.data), length(str.length)
	{}

	template<typename T>
	constexpr String(T &str)
		: data(str), length(getLength(str, IsArray<T>()))
	{}

	String(char const *data, int length)
		: data(data), length(length)
	{}

	String(uint8_t const *data, int length)
		: data(reinterpret_cast<char const*>(data)), length(length)
	{}

	bool empty() {return this->length <= 0;}
	
	String substring(int startIndex) {
		return String(this->data + startIndex, max(this->length - startIndex, 0));
	}

	/**
	 * Get a substring
	 * @param startIndex start of substring
	 * @param endIndex end of substring (not included)
	 * @return substring that references the data of this string
	 *
	 */
	String substring(int startIndex, int endIndex) {
		return String(this->data + startIndex, max(min(endIndex, this->length) - startIndex, 0));
	}

	/**
	 * return the index of the first occurrence of a character
	 * @param ch character to search
	 * @param startIndex index where to start searching
	 * @param defaultValue value to return when the character is not found
	 * @return index of first occurrence of the character
	 */
	int indexOf(char ch, int startIndex = 0, int defaultValue = -1) {
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
	int lastIndexOf(char ch, int startIndex = INT_MAX, int defaultValue = -1) {
		int i = min(startIndex, this->length);
		while (i > 0) {
			--i;
			if (this->data[i] == ch)
				return i;
		}
		return defaultValue;
	}

	String &operator =(String const &str) {
		const_cast<char const *&>(this->data) = str.data;
		const_cast<int&>(this->length) = str.length;
		return *this;
		
	}
	constexpr char const operator [](int index) const {return this->data[index];}

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

inline bool operator !=(String a, String b) {return !operator ==(a, b);}

inline bool operator <(String a, String b) {
	int length = min(a.length, b.length);
	for (int i = 0; i < length; ++i) {
		unsigned char c = a.data[i];
		unsigned char d = b.data[i];
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
inline void assign(char (&str)[N], String s) {
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

inline std::ostream &operator <<(std::ostream &s, String str) {
	s.write(str.data, str.length);
	return s;
}
