#pragma once

#include "../QuadratureDecoder.hpp"
#include "Loop.hpp"


class QuadratureDecoderDevice : public QuadratureDecoder, public Loop::Handler2 {
public:
	/**
	 * Constructor
	 * @param aPin pin of input A of quadrature decoder
	 * @param bPin pin of input A of quadrature decoder
	 */
	QuadratureDecoderDevice(int aPin, int bPin);

	Awaitable<Parameters> change(int8_t& delta) override;

	void handle() override;

protected:

	int acc = 0;

	Waitlist<Parameters> waitlist;
};