#pragma once

#include "../QuadratureDecoder.hpp"
#include "Loop.hpp"


class QuadratureDecoderImpl : public QuadratureDecoder, public Loop::Handler2 {
public:
	/**
	 * Constructor
	 */
	QuadratureDecoderImpl();

	Awaitable<Parameters> change(int8_t& delta) override;

	void handle(Gui &gui) override;

protected:

	Waitlist<Parameters> waitlist;
};