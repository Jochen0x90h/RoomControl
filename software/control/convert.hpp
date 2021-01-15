#pragma once

#include "String.hpp"
#include "optional.hpp"


/**
 * Convert string to integer
 * @param str string to convert
 * @return optional int, defined if conversion was successful
 */
optional<int> parseInt(String str);

/**
 * Convert string to floating point number in the form x.y without support for exponential notation
 * @param str string to convert
 * @return optional floating point number, defined if conversion was successful
 */
optional<float> parseFloat(String str);

/**
 * Convert a 32 bit unsigned integer to string
 * @param value value to convert
 * @param str string buffer to convert into
 * @param length length of string buffer
 * @param digitCount minimum number of digits to convert, pad smaller numbers with leading zeros
 * @return actual length of string
 */
int toString(uint32_t value, char *str, int length, int digitCount = 1);

/**
 * Convert a 32 bit unsigned integer to hex string
 * @param value value to convert
 * @param str string buffer to convert into
 * @param length length of string buffer
 * @param digitCount number of hex digits to convert
 * @return actual length of string
*/
int hexToString(uint32_t value, char *str, int length, int digitCount);

/**
 * Convert a float to string
 * @param value value to convert
 * @param str string buffer to convert into
 * @param length length of string buffer
 * @param digitCount minimum number of digits to convert before the decimal point
 * @param decimalCount minimum number of digits to convert after the decimal point
 * @return actual length of string
*/
int toString(float value, char *str, int length, int digitCount, int decimalCount);
