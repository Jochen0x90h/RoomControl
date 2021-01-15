#pragma once

#include "String.hpp"


// decimal numbers
template <typename T>
struct Dec {
	T value;
	int digitCount;
};
template <typename T>
Dec<T> dec(T value, int digitCount = 1) {
	return {value, digitCount};
}

// hexadecimal numbers
template <typename T>
struct Hex {
	T value;
	int digitCount;
};
template <typename T>
Hex<T> hex(T value, int digitCount = sizeof(T) * 2) {
	return {value, digitCount};
}

// floating point numbers
struct Flt {
	float value;
	int digitCount;
	int decimalCount;
};
constexpr Flt flt(float value, int decimalCount = 3) {
	return {value, 1, decimalCount};
}
constexpr Flt flt(float value, int digitCount, int decimalCount) {
	return {value, digitCount, decimalCount};
}

// plus operators
template <typename A, typename B>
struct Plus {
	A a;
	B b;
};


inline Plus<char, String> operator +(char a, String b) {
	return {a, b};
}

template <typename B>
Plus<char, Dec<B>> operator +(char a, Dec<B> b) {
	return {a, b};
}

template <typename B>
Plus<char, Hex<B>> operator +(char a, Hex<B> b) {
	return {a, b};
}

inline Plus<char, Flt> operator +(char a, Flt b) {
	return {a, b};
}


inline Plus<String, char> operator +(String a, char b) {
	return {a, b};
}

inline Plus<String, String> operator +(String a, String b) {
	return {a, b};
}

template <typename B>
Plus<String, Dec<B>> operator +(String a, Dec<B> b) {
	return {a, b};
}

template <typename B>
Plus<String, Hex<B>> operator +(String a, Hex<B> b) {
	return {a, b};
}

inline Plus<String, Flt> operator +(String a, Flt b) {
	return {a, b};
}

template <typename BA, typename BB>
inline Plus<String, Plus<BA, BB>> operator +(String a, Plus<BA, BB> b) {
	return {a, b};
}


template <typename A>
inline Plus<Dec<A>, char> operator +(Dec<A> a, char b) {
	return {a, b};
}

template <typename A>
inline Plus<Dec<A>, String> operator +(Dec<A> a, String b) {
	return {a, b};
}

template <typename A, typename B>
Plus<Dec<A>, Dec<B>> operator +(Dec<A> a, Dec<B> b) {
	return {a, b};
}

template <typename A, typename B>
Plus<Dec<A>, Hex<B>> operator +(Dec<A> a, Hex<B> b) {
	return {a, b};
}

template <typename A>
inline Plus<Dec<A>, Flt> operator +(Dec<A> a, Flt b) {
	return {a, b};
}


template <typename A>
inline Plus<Hex<A>, char> operator +(Hex<A> a, char b) {
	return {a, b};
}

template <typename A>
inline Plus<Hex<A>, String> operator +(Hex<A> a, String b) {
	return {a, b};
}

template <typename A, typename B>
Plus<Hex<A>, Dec<B>> operator +(Hex<A> a, Dec<B> b) {
	return {a, b};
}

template <typename A, typename B>
Plus<Hex<A>, Hex<B>> operator +(Hex<A> a, Hex<B> b) {
	return {a, b};
}

template <typename A>
inline Plus<Hex<A>, Flt> operator +(Hex<A> a, Flt b) {
	return {a, b};
}


inline Plus<Flt, char> operator +(Flt a, char b) {
	return {a, b};
}

inline Plus<Flt, String> operator +(Flt a, String b) {
	return {a, b};
}

template <typename B>
Plus<Flt, Dec<B>> operator +(Flt a, Dec<B> b) {
	return {a, b};
}

template <typename B>
Plus<Flt, Hex<B>> operator +(Flt a, Hex<B> b) {
	return {a, b};
}

inline Plus<Flt, Flt> operator +(Flt a, Flt b) {
	return {a, b};
}


template <typename AA, typename AB>
inline Plus<Plus<AA, AB>, char> operator +(Plus<AA, AB> a, char b) {
	return {a, b};
}

template <typename AA, typename AB>
inline Plus<Plus<AA, AB>, String> operator +(Plus<AA, AB> a, String b) {
	return {a, b};
}

template <typename AA, typename AB, typename B>
Plus<Plus<AA, AB>, Dec<B>> operator +(Plus<AA, AB> a, Dec<B> b) {
	return {a, b};
}

template <typename AA, typename AB, typename B>
Plus<Plus<AA, AB>, Hex<B>> operator +(Plus<AA, AB> a, Hex<B> b) {
	return {a, b};
}

template <typename AA, typename AB>
inline Plus<Plus<AA, AB>, Flt> operator +(Plus<AA, AB> a, Flt b) {
	return {a, b};
}
