#pragma once

#include "../loop.hpp"
#include <boost/asio.hpp>

namespace asio = boost::asio;

namespace loop {

// io context (event loop for asynchronous input/output)
extern asio::io_service context;

} // namespace loop
