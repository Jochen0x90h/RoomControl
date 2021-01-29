#include "convert.hpp"
#include "util.hpp"


optional<int> parseInt(String str) {
	if (str.length <= 0)
		return nullptr;

	// check for sign
	int i = 0;
	bool minus = str[0] == '-';
	if (minus || str[0] == '+') {
		if (str.length == 1)
			return nullptr;
		i = 1;
	}

	// parse integer
	int value = 0;
	for (; i < str.length; ++i) {
		uint8_t ch = str[i];
		if (ch >= '0' && ch <= '9') {
			value = value * 10 + ch - '0';
		} else {
			// invalid character
			return nullptr;
		}
	}
	
	return minus ? -value : value;
}

optional<float> parseFloat(String str) {
	if (str.length <= 0)
		return nullptr;
	
	// check for sign
	int i = 0;
	bool minus = str[0] == '-';
	if (minus || str[0] == '+') {
		if (str.length == 1)
			return nullptr;
		i = 1;
	}
	
	// check if there is only a decimal point
	if (str.length == i + 1 && str[i] == '.')
		return nullptr;
	
	// parse integer part
	float value = 0.0f;
	for (; i < str.length; ++i) {
		char ch = str[i];
		if (ch == '.') {
			++i;
			break;
		} else if (ch >= '0' && ch <= '9') {
			float digit = ch - '0';
			value = value * 10.0f + digit;
		} else {
			// invalid character
			return nullptr;
		}
	}
	
	// parse decimal places
	float decimal = 1.0f;
	for (; i < str.length; ++i) {
		char ch = str[i];
		if (ch >= '0' && ch <= '9') {
			float digit = ch - '0';
			decimal *= 0.1f;
			value += digit * decimal;
		} else {
			// invalid character
			return nullptr;
		}
	}
	
	return minus ? -value : value;
}

int toString(uint32_t value, char *str, int length, int digitCount) {
	// enforce valid parameters
	if (digitCount > 10)
		digitCount = 10;

	// convert into buffer
	char buffer[10];
	char *end = buffer + 10;
	char *it = end;
	while (value > 0 || digitCount > 0) {
		--it;
		*it = '0' + value % 10;
		value /= 10;
		--digitCount;
	};
	
	// copy to output string
	int i;
	for (i = 0; i < length && it != end; ++i, ++it) {
		str[i] = *it;
	}
	return i;
}

static char const *hexTable = "0123456789abcdef";

int hexToString(uint32_t value, char *str, int length, int digitCount) {
	int l = min(length, digitCount);
	for (int i = 0; i < l; ++i) {
		str[i] = hexTable[(value >> (digitCount - 1 - i) * 4) & 0xf];
	}
	return l;
}

static float const roundTable[] = {0.5f,
	0.05f, 0.005f, 0.0005f,
	0.00005f, 0.000005f, 0.0000005f,
	0.00000005f, 0.000000005f, 0.0000000005f};

static float const powTable[] = {1.0f,
	10.0f, 100.0f, 1000.0f,
	10000.0f, 100000.0f, 1000000.0f,
	10000000.0f, 100000000.0f, 1000000000.0f};

int toString(float value, char *str, int length, int digitCount, int decimalCount) {
	// enforce valid parameters
	if (decimalCount > 9)
		decimalCount = 9;
	if (decimalCount < 0)
		decimalCount = 0;
	
	// handle sign
	int i = 0;
	if (value < 0) {
		if (0 < length)
			str[i++] = '-';
		value = -value;
	}
		
	// round
	value += roundTable[decimalCount];
	
	// extract integer and fractional part
	// check with https://godbolt.org/ and parameters -mthumb -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Os
	uint32_t ipart = uint32_t(value);
	uint32_t fpart = uint32_t((value - float(ipart)) * powTable[decimalCount]);

	// convert integer part
	i += toString(ipart, str + i, length - i, (fpart == 0 && digitCount == 0) ? 1 : digitCount);

	// convert decimal part
	if (fpart != 0 && decimalCount > 0 && i < length) {
		str[i++] = '.';
		
		// remove trailing zeros
		while (fpart % 10 == 0) {
			fpart /= 10;
			--decimalCount;
		}
		
		// truncate to length
		while (i + decimalCount > length) {
			fpart /= 10;
			--decimalCount;
		}
		
		// convert decimal part
		i += decimalCount;
		char *it = str + i;
		while (decimalCount > 0) {
			--decimalCount;
			--it;
			*it = '0' + fpart % 10;
			fpart /= 10;
		}
	}

	return i;
}
