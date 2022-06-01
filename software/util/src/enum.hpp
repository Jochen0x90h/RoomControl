#pragma once

#include <type_traits>


/**
 * Macro that adds helper operator to enums that are flag fields
 * Important: Define the enum as "enum class ..."
 */
#define FLAGS_ENUM(T) \
constexpr T operator ~ (T a) {return T(~std::underlying_type<T>::type(a));} \
constexpr T operator | (T a, T b) {return T(std::underlying_type<T>::type(a) | std::underlying_type<T>::type(b));} \
constexpr T operator & (T a, T b) {return T(std::underlying_type<T>::type(a) & std::underlying_type<T>::type(b));} \
constexpr T operator ^ (T a, T b) {return T(std::underlying_type<T>::type(a) ^ std::underlying_type<T>::type(b));} \
inline T &operator |= (T &a, T b) {(std::underlying_type<T>::type &)a |= std::underlying_type<T>::type(b); return a;} \
inline T &operator &= (T &a, T b) {(std::underlying_type<T>::type &)a &= std::underlying_type<T>::type(b); return a;} \
inline T &operator ^= (T &a, T b) {(std::underlying_type<T>::type &)a ^= std::underlying_type<T>::type(b); return a;} \
constexpr bool operator == (T a, std::underlying_type<T>::type b) {return std::underlying_type<T>::type(a) == b;}

//constexpr bool operator != (T a, std::underlying_type<T>::type b) {return std::underlying_type<T>::type(a) != b;}
