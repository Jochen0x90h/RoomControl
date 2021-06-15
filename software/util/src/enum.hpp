#pragma once

#include <type_traits>


#define FLAGS_ENUM(T) \
constexpr T operator ~ (T a) {return T(~std::underlying_type<T>::type(a));} \
constexpr T operator | (T a, T b) {return T(std::underlying_type<T>::type(a) | std::underlying_type<T>::type(b));} \
inline T &operator |= (T &a, T b) {(std::underlying_type<T>::type &)a |= std::underlying_type<T>::type(b); return a;} \
inline std::underlying_type<T>::type &operator |= (std::underlying_type<T>::type &a, T b) {a |= std::underlying_type<T>::type(b); return a;} \
constexpr T operator & (T a, T b) {return T(std::underlying_type<T>::type(a) & std::underlying_type<T>::type(b));} \
inline T &operator &= (T &a, T b) {(std::underlying_type<T>::type &)a &= std::underlying_type<T>::type(b); return a;} \
inline std::underlying_type<T>::type &operator &= (std::underlying_type<T>::type &a, T b) {a &= std::underlying_type<T>::type(b); return a;} \
constexpr bool operator != (T a, std::underlying_type<T>::type b) {return std::underlying_type<T>::type(a) != b;} \
constexpr bool operator == (T a, std::underlying_type<T>::type b) {return std::underlying_type<T>::type(a) == b;}
