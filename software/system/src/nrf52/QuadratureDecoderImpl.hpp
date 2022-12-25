#pragma once

#include "../QuadratureDecoder.hpp"
#include "Loop.hpp"


class QuadratureDecoderImpl : public QuadratureDecoder, public loop::Handler2 {
public:
	/**
	 * Constructor
	 * @param aPin pin of input A of quadrature decoder
	 * @param bPin pin of input A of quadrature decoder
	 */
	QuadratureDecoderImpl(int aPin, int bPin);

	Awaitable<Parameters> change(int8_t& delta) override;

	void handle() override;

protected:

	int acc = 0;

	Waitlist<Parameters> waitlist;
};