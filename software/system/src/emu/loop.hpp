#pragma once

#include "../loop.hpp"
#include "Gui.hpp"
#include <boost/asio.hpp>

namespace asio = boost::asio;

namespace loop {

// io context (event loop for asynchronous input/output)
extern asio::io_context context;


using Handler = void (*)(Gui &);

/**
 * Add a handler to the event loop
 */
Handler addHandler(Handler handler);

} // namespace loop
