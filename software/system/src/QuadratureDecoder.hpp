#pragma once

#include <Coroutine.hpp>
#include <stdint.h>


/**
 * Support for incremental encoders such as digital potentiometers
 */
class QuadratureDecoder {
public:
	struct Parameters {
		int8_t &delta;
	};


	virtual ~QuadratureDecoder();

	/**
	 * Wait for a change of the incremental encoder
	 * @param delta delta motion of incremental encoder
	 * @return use co_await on return value to await a change of the encoder
	 */
	[[nodiscard]] virtual Awaitable<Parameters> change(int8_t& delta) = 0;
};
