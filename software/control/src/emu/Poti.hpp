#pragma once

#include "global.hpp"


/**
 * Interface to a digital potentiometer
 */
class Poti {
public:

	Poti() {}

	virtual ~Poti();

	/**
	 * Called when the digital potentiometer recorded a change
	 */
	virtual void onPotiChanged(int delta, bool activated) = 0;

};
