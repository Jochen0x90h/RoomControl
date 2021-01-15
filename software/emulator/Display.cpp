#include "Display.hpp"
#include <boost/bind.hpp>
#include "global.hpp"


Display::~Display() {
}

void Display::setDisplay(Bitmap<WIDTH, HEIGHT> const &bitmap) {
	this->emulatorDisplayBitmap = bitmap;
	
	// notify that we are ready
	asio::post(global::context, boost::bind(&Display::onDisplayReady, this));
}
