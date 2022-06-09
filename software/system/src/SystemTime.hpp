#pragma once

#include <cstdint>


/**
 * System duration, internal unit is 1/1024 second
 */
struct SystemDuration {
	int32_t value;

	/**
	 * Convert duration to seconds
	 * @return duration in seconds
	 */
	int toSeconds() const {
		return value >> 10;
	}

	constexpr SystemDuration &operator +=(SystemDuration b) {
		this->value += b.value;
		return *this;
	}
	
	constexpr SystemDuration &operator -=(SystemDuration b) {
		this->value -= b.value;
		return *this;
	}
	
	constexpr SystemDuration &operator *=(int b) {
		this->value *= b;
		return *this;
	}

	static constexpr SystemDuration max() {return {0x7fffffff};}
};

constexpr SystemDuration operator -(SystemDuration a) {
	return {-a.value};
}

constexpr SystemDuration operator +(SystemDuration a, SystemDuration b) {
	return {a.value + b.value};
}

constexpr SystemDuration operator -(SystemDuration a, SystemDuration b) {
	return {a.value - b.value};
}

constexpr SystemDuration operator *(int a, SystemDuration b) {
	return {a * b.value};
}

constexpr SystemDuration operator *(SystemDuration a, int b) {
	return {a.value * b};
}

constexpr SystemDuration operator *(SystemDuration a, float b) {
	return {int(float(a.value) * b + (a.value < 0 ? -0.5f : 0.5f))};
}

constexpr SystemDuration operator /(SystemDuration a, int b) {
	return {a.value / b};
}

struct DurationDiv {
	SystemDuration a;
	SystemDuration b;
	
	operator int () {return a.value / b.value;}
	operator float () {return float(a.value) / float(b.value);}
};

/**
 * Divide two durations
 * @return quotient can be assigned to int or float
 */
constexpr DurationDiv operator /(SystemDuration a, SystemDuration b) {
	return {a.value, b.value};
}

constexpr SystemDuration operator %(SystemDuration a, SystemDuration b) {
	return {a.value % b.value};
}

constexpr bool operator ==(SystemDuration a, SystemDuration b) {
	return a.value == b.value;
}

constexpr bool operator <(SystemDuration a, SystemDuration b) {
	return a.value < b.value;
}

constexpr bool operator <=(SystemDuration a, SystemDuration b) {
	return a.value <= b.value;
}

constexpr bool operator >(SystemDuration a, SystemDuration b) {
	return a.value > b.value;
}

constexpr bool operator >=(SystemDuration a, SystemDuration b) {
	return a.value >= b.value;
}

constexpr SystemDuration min(SystemDuration x, SystemDuration y) {return {x.value < y.value ? x.value : y.value};}



/**
 * System time, internal unit is 1/1024 second
 */
struct SystemTime {
	uint32_t value;
		
	constexpr SystemTime &operator +=(SystemDuration b) {
		this->value += b.value;
		return *this;
	}
	
	constexpr SystemTime &operator -=(SystemDuration b) {
		this->value -= b.value;
		return *this;
	}

	bool operator ==(SystemTime const &t) const {
		return this->value == t.value;
	}
};

constexpr SystemTime operator +(SystemTime a, SystemDuration b) {
	return {a.value + b.value};
}

constexpr SystemTime operator -(SystemTime a, SystemDuration b) {
	return {a.value - b.value};
}

constexpr SystemDuration operator -(SystemTime a, SystemTime b) {
	return {int32_t(a.value - b.value)};
}

constexpr bool operator <(SystemTime a, SystemTime b) {
	return int32_t(a.value - b.value) < 0;
}

constexpr bool operator <=(SystemTime a, SystemTime b) {
	return int32_t(a.value - b.value) <= 0;
}

constexpr bool operator >(SystemTime a, SystemTime b) {
	return int32_t(a.value - b.value) > 0;
}

constexpr bool operator >=(SystemTime a, SystemTime b) {
	return int32_t(a.value - b.value) >= 0;
}

constexpr SystemTime min(SystemTime x, SystemTime y) {return {x.value < y.value ? x.value : y.value};}

/**
 * Suffix for milliseconds, e.g. 100ms
 */
constexpr SystemDuration operator "" ms(unsigned long long ms) {
	return {int32_t(ms)};
}

/**
 * Suffix for seconds, e.g. 5s
 */
constexpr SystemDuration operator "" s(unsigned long long s) {
	return {int32_t(s * 1000)};
}

/**
 * Suffix for minutes, e.g. 3min
 */
constexpr SystemDuration operator "" min(unsigned long long s) {
	return {int32_t(s * 60 * 1000)};
}

/**
 * Suffix for hours, e.g. 8h
 */
constexpr SystemDuration operator "" h(unsigned long long s) {
	return {int32_t(s * 60 * 60 * 1000)};
}

/*
struct SystemDuration16 {
	uint16_t value;
	
	constexpr SystemDuration16 &operator =(SystemDuration d) {
		this->value = uint16_t(d.value);
		return *this;
	}

	operator SystemDuration () {
		return {this->value};
	}
};

struct SystemTime16 {
	uint16_t value;

	constexpr SystemTime16 &operator =(SystemTime t) {
		this->value = uint16_t(t.value);
		return *this;
	}
	
	SystemTime expand(SystemTime t) {
		int carry = int(uint16_t(t.value) < this->value);
		return {(t.value & 0xffff0000) + this->value - (carry << 16)};
	}
};
*/