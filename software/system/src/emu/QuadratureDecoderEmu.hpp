#pragma once

#include "../QuadratureDecoder.hpp"
#include "Loop.hpp"


class QuadratureDecoderEmu : public QuadratureDecoder, public Loop::Handler2 {
public:
	/**
	 * Constructor
	 */
	QuadratureDecoderEmu();

	Awaitable<Parameters> change(int8_t& delta) override;

	void handle(Gui &gui) override;

protected:

	Waitlist<Parameters> waitlist;
};