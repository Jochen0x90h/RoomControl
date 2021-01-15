#pragma once

#include "Gui.hpp"
#include <boost/asio.hpp>


//using namespace std::chrono_literals;
namespace asio = boost::asio;

namespace global {

	// io context (event loop for asynchronous input/output)
	extern asio::io_service context;

	// local endpoint (e.g. asio::ip::udp::endpoint(asio::ip::udp::v6(), port))
	extern asio::ip::udp::endpoint local;

	// remote endpoint of uplink
	extern asio::ip::udp::endpoint upLink;

	// emulator user interface
	extern Gui *gui;
};
